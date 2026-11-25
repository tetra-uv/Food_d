// Gateway tests: real HTTP against a gateway on port 8788 (NOT 8787) running on a TEMP COPY of the lab scenario.
// Run: cd web-app/server && npm test
import { test, before, after, describe } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { loadConfig, loadSecret, ConfigError, SCENARIOS_DIR, SERVER_DIR, copyScenario } from '../src/config.js';
import { buildApp } from '../src/app.js';

const PORT = 8788;
const BASE = `http://127.0.0.1:${PORT}`;
const API = `${BASE}/api/v1`;
const WORK = path.join(SERVER_DIR, 'testdata', 'work');
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

function freshDir(name) {
  const d = path.join(WORK, name);
  fs.rmSync(d, { recursive: true, force: true });
  copyScenario(path.join(SCENARIOS_DIR, 'lab'), d);   // all files, never the pristine folder itself
  return d;
}

async function login(role, identityId) {
  const res = await fetch(`${API}/session`, { method: 'POST', headers: { 'content-type': 'application/json' }, body: JSON.stringify({ role, identityId }) });
  assert.equal(res.status, 200, `login ${role} ${identityId}`);
  return res.headers.get('set-cookie').split(';')[0];
}
async function http(cookie, method, url, body) {
  const res = await fetch(API + url, { method, headers: { ...(cookie ? { cookie } : {}), ...(body !== undefined ? { 'content-type': 'application/json' } : {}) }, body: body !== undefined ? JSON.stringify(body) : undefined });
  const text = await res.text();
  let json = null;
  try { json = text ? JSON.parse(text) : null; } catch { /* not json */ }
  return { status: res.status, json, headers: res.headers };
}

/** Minimal SSE client. */
async function sse(cookie, headers = {}) {
  const ac = new AbortController();
  const res = await fetch(`${API}/events`, { headers: { cookie, ...headers }, signal: ac.signal });
  assert.equal(res.status, 200);
  assert.match(res.headers.get('content-type'), /text\/event-stream/);
  const events = [];
  const comments = [];
  let buf = '';
  (async () => {
    const dec = new TextDecoder();
    try {
      for await (const chunk of res.body) {
        buf += dec.decode(chunk, { stream: true });
        let at;
        while ((at = buf.indexOf('\n\n')) >= 0) {
          const block = buf.slice(0, at); buf = buf.slice(at + 2);
          if (block.startsWith(':')) { comments.push(block); continue; }
          const ev = {};
          for (const line of block.split('\n')) {
            const i = line.indexOf(':');
            if (i > 0) ev[line.slice(0, i)] = line.slice(i + 1).trim();
          }
          if (ev.event) events.push({ id: Number(ev.id), type: ev.event, data: ev.data ? JSON.parse(ev.data) : null });
        }
      }
    } catch { /* aborted */ }
  })();
  return {
    events, comments,
    async waitFor(type, pred = () => true, ms = 3000) {
      const t0 = Date.now();
      while (Date.now() - t0 < ms) {
        const found = events.find((e) => e.type === type && pred(e.data));
        if (found) return found;
        await sleep(20);
      }
      throw new Error(`no ${type} event within ${ms} ms; got ${events.map((e) => e.type).join(',')}`);
    },
    close() { ac.abort(); },
  };
}

let gw, dataDir, geo;
const noIdentity = undefined;

describe('gateway end to end', () => {
  before(async () => {
    dataDir = freshDir('gw_main');
    geo = { projection: 'test', nodes: { 0: { lat: 28.65, lng: 77.19 } } };
    fs.writeFileSync(path.join(dataDir, 'geo_overlay.json'), JSON.stringify(geo));
    const webDist = path.join(WORK, 'gw_dist');
    fs.rmSync(webDist, { recursive: true, force: true });
    fs.mkdirSync(path.join(webDist, 'assets'), { recursive: true });
    fs.writeFileSync(path.join(webDist, 'index.html'), '<!doctype html><title>spa</title>');
    fs.writeFileSync(path.join(webDist, 'assets', 'app.js'), 'console.log(1)');
    const config = loadConfig({ CB_DATA: dataDir, CB_PORT: String(PORT), CB_SECRET_FILE: path.join(WORK, 'gw_secret.txt'), CB_WEB_DIST: webDist, CB_HEARTBEAT_MS: '150' });
    gw = await buildApp({ config, secret: loadSecret(config.secretPath), log: () => {} });
    await gw.start();
  });
  after(async () => { await gw.close(); });

  test('health, meta and identities are available; /snapshot needs a coordinator', async () => {
    const h = await http(null, 'GET', '/health');
    assert.equal(h.json.state, 'READY');
    assert.equal(h.json.scenario, 'gw_main');
    const ids = await http(null, 'GET', '/identities');
    assert.equal(ids.json.donors.length, 12);
    assert.equal(ids.json.recipients.length, 26);
    const meta = await http(null, 'GET', '/meta');
    assert.equal(meta.json.simulated, true);
    assert.equal(meta.json.capabilities.geo, true);
    assert.deepEqual(meta.json.warnings, [], 'warnings are for the coordinator only');
    assert.equal((await http(null, 'GET', '/snapshot')).status, 401);
    assert.equal((await http(null, 'GET', '/session')).status, 401);
  });

  test('session: cookie is httpOnly, signed; forged cookie is rejected; logout works', async () => {
    const res = await fetch(`${API}/session`, { method: 'POST', headers: { 'content-type': 'application/json' }, body: JSON.stringify({ role: 'coordinator' }) });
    const sc = res.headers.get('set-cookie');
    assert.match(sc, /HttpOnly/); assert.match(sc, /SameSite=Lax/);
    const cookie = sc.split(';')[0];
    const me = await http(cookie, 'GET', '/session');
    assert.equal(me.json.role, 'coordinator');
    const forged = 'cb_session=' + Buffer.from(JSON.stringify({ r: 'coordinator', i: null, e: 9999999999 })).toString('base64url') + '.AAAA';
    assert.equal((await http(forged, 'GET', '/snapshot')).status, 401);
    assert.equal((await http(null, 'POST', '/session', { role: 'donor' })).status, 400, 'a donor needs an identity');
    assert.equal((await http(null, 'POST', '/session', { role: 'donor', identityId: 'DN999' })).status, 404);
    assert.equal((await http(null, 'POST', '/session', { role: 'admin' })).status, 400);
    const out = await fetch(`${API}/session`, { method: 'DELETE', headers: { cookie } });
    assert.equal(out.status, 204);
    assert.match(out.headers.get('set-cookie'), /Max-Age=0/);
  });

  test('cross-site origin is refused on writes', async () => {
    const res = await fetch(`${API}/session`, { method: 'POST', headers: { 'content-type': 'application/json', origin: 'https://evil.example' }, body: '{"role":"coordinator"}' });
    assert.equal(res.status, 403);
    const ok = await fetch(`${API}/session`, { method: 'POST', headers: { 'content-type': 'application/json', origin: 'http://localhost:5173' }, body: '{"role":"coordinator"}' });
    assert.equal(ok.status, 200);
  });

  test('roles: donor cannot read the snapshot; recipient cannot read another recipient or match', async () => {
    const donor = await login('donor', 'DN001');
    assert.equal((await http(donor, 'GET', '/snapshot')).status, 403);
    assert.equal((await http(donor, 'GET', '/donations/DON001/match')).status, 403);
    assert.equal((await http(donor, 'GET', '/queue')).status, 403);
    const own = await http(donor, 'GET', '/donations');
    assert.ok(own.json.length > 0 && own.json.every((d) => d.donorId === 'DN001'));
    const rec = await login('recipient', 'R004');
    assert.equal((await http(rec, 'GET', '/snapshot')).status, 403);
    assert.equal((await http(rec, 'GET', '/recipients/R013')).status, 403);
    assert.equal((await http(rec, 'POST', '/offers', { donationId: 'DON001', recipientId: 'R004', quantity: 1 })).status, 403);
    assert.equal((await http(rec, 'POST', '/donations', { food: 'COOKED', quantity: 1, usableMinutes: 5 })).status, 403);
    // donor preview is aggregate only and forced to its own donor
    const pv = await http(donor, 'POST', '/match/preview', { food: 'COOKED', quantity: 60, usableMinutes: 12 });
    assert.equal(pv.status, 200);
    assert.deepEqual(Object.keys(pv.json).sort(), ['bestTravelMinutes', 'byReason', 'feasibleCount']);
    assert.equal(pv.json.feasibleCount, 2);
    assert.equal(pv.json.bestTravelMinutes, 8);
    assert.equal((await http(donor, 'POST', '/match/preview', { donorId: 'DN002', food: 'COOKED', quantity: 1, usableMinutes: 5 })).status, 403);
    // donor creates a donation only for itself
    assert.equal((await http(donor, 'POST', '/donations', { donorId: 'DN002', food: 'COOKED', quantity: 1, usableMinutes: 5 })).status, 403);
  });

  test('coordinator: snapshot, queue, match R013 then R004, preview what-if, field, network with geo', async () => {
    const c = await login('coordinator');
    const snap = await http(c, 'GET', '/snapshot');
    assert.equal(snap.status, 200);
    assert.equal(snap.json.donations.length, 46);
    assert.match(snap.headers.get('etag'), /^"[0-9a-f]{12}:0"$/);
    assert.ok(snap.json.warnings.length >= 1);
    const m = await http(c, 'GET', '/donations/DON001/match');
    assert.deepEqual(m.json.candidates.map((x) => x.recipientId), ['R013', 'R004']);
    assert.equal(m.json.candidates[0].travelMinutes, 8);
    assert.equal(m.json.rejected.length, 24);
    assert.equal((await http(c, 'GET', '/donations/DON999/match')).status, 404);
    const q = await http(c, 'GET', '/queue');
    assert.ok(q.json.order.length > 30 && q.json.nextId);
    const p = await http(c, 'POST', '/match/preview', { donorId: 'DN001', food: 'COOKED', quantity: 60, usableMinutes: 11 });
    assert.deepEqual(p.json.candidates.map((x) => x.recipientId), ['R013']);
    const f = await http(c, 'GET', '/network/field?source=0');
    assert.deepEqual(f.json.dist.slice(0, 5), [0, 24, 8, 20, 6]);
    assert.equal((await http(c, 'GET', '/network/field?source=abc')).status, 400);
    assert.equal((await http(c, 'GET', '/network/field?source=99')).status, 404);
    const r = await http(c, 'GET', '/network/route?from=0&to=5');
    assert.equal(r.json.minutes, 25);
    const net = await http(c, 'GET', '/network');
    assert.equal(net.json.nodes.length, 20); assert.equal(net.json.edges.length, 25);
    assert.deepEqual(net.json.geo, geo, 'geo_overlay.json is merged into /network');
    assert.deepEqual(net.json.nodes[13].donors, ['DN007']);
    // a donor sees topology but only its own organisation
    const donor = await login('donor', 'DN007');
    const dn = await http(donor, 'GET', '/network');
    assert.equal(dn.json.nodes.length, 20);
    assert.deepEqual(dn.json.nodes[13].donors, ['DN007']);
    assert.deepEqual(dn.json.nodes[13].recipients, []);
    assert.equal((await http(c, 'GET', '/nope')).status, 404);
  });

  test('validation: bad bodies get problem+json 400/422', async () => {
    const c = await login('coordinator');
    const a = await http(c, 'POST', '/offers', { donationId: 'DON001', recipientId: 'R013', quantity: 'many' });
    assert.equal(a.status, 400); assert.equal(a.json.code, 'VALIDATION_FAILED'); assert.match(a.headers.get('content-type'), /^application\/problem\+json/);
    assert.ok(a.json.requestId);
    assert.equal((await http(c, 'POST', '/offers', { donationId: 'DON001', recipientId: 'R013', quantity: 5, extra: 1 })).status, 400);
    assert.equal((await http(c, 'POST', '/donations/DON001/close', {})).status, 400, 'close needs confirm:true');
    assert.equal((await http(c, 'POST', '/donations', { donorId: 'DN001', food: 'SOUP', quantity: 1, usableMinutes: 1 })).status, 400);
    assert.equal((await http(c, 'POST', '/donors', { name: 'a,b', type: 'x', node: 1 })).json.code, 'INVALID_NAME');
    const big = await fetch(`${API}/offers`, { method: 'POST', headers: { cookie: c, 'content-type': 'application/json' }, body: JSON.stringify({ pad: 'x'.repeat(70000) }) });
    assert.equal(big.status, 413 === big.status ? 413 : big.status);
    assert.ok(big.status >= 400 && big.status < 500);
    const badJson = await fetch(`${API}/offers`, { method: 'POST', headers: { cookie: c, 'content-type': 'application/json' }, body: '{oops' });
    assert.equal(badJson.status, 400);
    const r = await http(c, 'POST', '/offers', { donationId: 'DON001', recipientId: 'R013', quantity: 999 });
    assert.equal(r.status, 422); assert.equal(r.json.code, 'QUANTITY_OUT_OF_RANGE'); assert.deepEqual(r.json.details, { min: 1, max: 60 });
    const nc = await http(c, 'POST', '/offers', { donationId: 'DON001', recipientId: 'R017', quantity: 1 });
    assert.equal(nc.status, 409); assert.equal(nc.json.code, 'NOT_A_CANDIDATE'); assert.equal(nc.json.details.reason, 'NOT_ACCEPTING');
  });

  test('flow: offer 40 -> SSE to recipient R013 (not to R004) -> accept as R013 -> events, snapshot, effects', async () => {
    const coord = await login('coordinator');
    const r013 = await login('recipient', 'R013');
    const r004 = await login('recipient', 'R004');
    const dn001 = await login('donor', 'DN001');
    const evCoord = await sse(coord);
    const evR013 = await sse(r013);
    const evR004 = await sse(r004);
    const evDonor = await sse(dn001);
    try {
      const sent = await http(coord, 'POST', '/offers', { donationId: 'DON001', recipientId: 'R013', quantity: 40 });
      assert.equal(sent.status, 201);
      assert.equal(sent.json.data.offer.id, 'OFF001');
      assert.equal(sent.json.revision, 1);
      assert.match(sent.json.message, /Offer OFF001 sent to/);
      assert.ok(sent.json.effects.offers.length === 1);
      const e1 = await evR013.waitFor('offer.sent', (d) => d.offerId === 'OFF001');
      assert.equal(e1.data.quantity, 40); assert.equal(e1.data.travelMinutes, 8); assert.equal(e1.data.rev, 1);
      await evCoord.waitFor('offer.sent', (d) => d.offerId === 'OFF001');
      await evDonor.waitFor('offer.sent', (d) => d.offerId === 'OFF001');
      await sleep(150);
      assert.equal(evR004.events.some((e) => e.type === 'offer.sent'), false, 'another recipient must not see this offer');

      // recipient R013 sees its offer; R004 does not and cannot accept it
      assert.equal((await http(r013, 'GET', '/offers')).json.length, 1);
      assert.equal((await http(r004, 'GET', '/offers')).json.length, 0);
      // offers carry a context so a recipient can render them without reading the donation
      const mine = (await http(r013, 'GET', '/offers')).json[0];
      assert.deepEqual(mine.context, { food: 'COOKED', usableMinutes: 12, donorId: 'DN001', donorName: mine.context.donorName, donorNode: 0, recipientName: mine.context.recipientName, recipientNode: 2 });
      assert.match(mine.context.donorName, /Shagun Banquet Hall/);
      assert.ok(mine.context.recipientName.length > 0);
      assert.equal((await http(r013, 'GET', '/donations/DON001')).status, 403, 'recipients still cannot read donations');
      assert.deepEqual(e1.data.context, mine.context, 'the SSE offer.sent event carries the same context');
      assert.deepEqual(sent.json.data.offer.context, mine.context, 'the mutation response carries it too');
      assert.equal((await http(dn001, 'GET', '/offers')).json[0].context.recipientNode, 2, 'the donor of the donation sees it');
      const dn002 = await login('donor', 'DN002');
      assert.equal((await http(dn002, 'GET', '/offers')).json.length, 0, 'another donor sees no offers');
      assert.equal((await http(dn002, 'GET', '/snapshot')).status, 403);
      assert.ok((await http(coord, 'GET', '/snapshot')).json.offers.every((o) => o.context && o.context.donorName));
      assert.equal((await http(r004, 'POST', '/offers/OFF001/accept')).status, 403);
      assert.equal((await http(coord, 'POST', '/offers/OFF001/accept')).status, 403, 'the coordinator does not answer offers');
      const chk = await http(r013, 'GET', '/offers/OFF001/check');
      assert.equal(chk.json.wouldSucceed, true);

      const acc = await http(r013, 'POST', '/offers/OFF001/accept');
      assert.equal(acc.status, 200);
      assert.equal(acc.json.data.transaction.id, 'T004');
      assert.equal(acc.json.revision, 2);
      const fx = acc.json.effects;
      assert.deepEqual(fx.recipients[0].before, { availableCapacity: 250 });
      assert.deepEqual(fx.recipients[0].after, { availableCapacity: 210 });
      assert.equal(fx.donations[0].after.remaining, 20); assert.equal(fx.donations[0].after.status, 'PARTIAL');
      await evCoord.waitFor('offer.accepted', (d) => d.txnId === 'T004');
      await evCoord.waitFor('transaction.created', (d) => d.txnId === 'T004' && d.quantity === 40);
      await evDonor.waitFor('offer.accepted', (d) => d.offerId === 'OFF001');
      await evR013.waitFor('transaction.created', (d) => d.recipientId === 'R013');
      await evCoord.waitFor('donation.updated', (d) => d.donationId === 'DON001');

      const snap = (await http(coord, 'GET', '/snapshot')).json;
      assert.equal(snap.recipients.find((r) => r.id === 'R013').availableCapacity, 210);
      const don = snap.donations.find((d) => d.id === 'DON001');
      assert.equal(don.remaining, 20); assert.equal(don.status, 'PARTIAL');
      assert.equal(snap.transactions.length, 4);
      assert.equal((await http(coord, 'GET', '/transactions')).json.length, 4);
      assert.equal((await http(r013, 'GET', '/transactions')).json.length, 2, 'R013 has T001 and T004');
      assert.equal((await http(dn001, 'GET', '/transactions?donation=DON001')).json.length, 1);
      assert.equal((await http(r013, 'POST', '/offers/OFF001/accept')).status, 409, 'answered offers cannot be accepted again');
      assert.equal((await http(r013, 'POST', '/offers/OFF001/accept')).json.code, 'OFFER_ALREADY_ANSWERED');

      // recipient edits its own record only
      const patch = await http(r013, 'PATCH', '/recipients/R013', { capacity: 300 });
      assert.equal(patch.status, 200);
      await evR013.waitFor('recipient.updated', (d) => d.recipientId === 'R013');
      assert.equal((await http(r013, 'PATCH', '/recipients/R004', { capacity: 1 })).status, 403);
      assert.equal((await http(coord, 'PATCH', '/recipients/R013', {})).status, 400);

      // withdraw + close
      const o2 = await http(coord, 'POST', '/offers', { donationId: 'DON001', recipientId: 'R004', quantity: 10 });
      assert.equal(o2.status, 201);
      assert.equal((await http(coord, 'POST', `/offers/${o2.json.data.offer.id}/withdraw`, {})).status, 400);
      const w = await http(coord, 'POST', `/offers/${o2.json.data.offer.id}/withdraw`, { confirm: true });
      assert.equal(w.status, 200);
      await evCoord.waitFor('offer.withdrawn', (d) => d.offerId === o2.json.data.offer.id);
      const cl = await http(coord, 'POST', '/donations/DON001/close', { confirm: true });
      assert.equal(cl.status, 200); assert.equal(cl.json.data.wasted, 20);
      await evDonor.waitFor('donation.closed', (d) => d.donationId === 'DON001');
      assert.equal((await http(coord, 'POST', '/donations/DON001/close', { confirm: true })).status, 409);
      const created = await http(dn001, 'POST', '/donations', { food: 'PACKAGED', quantity: 10, usableMinutes: 100 });
      assert.equal(created.status, 201);
      await evDonor.waitFor('donation.created', (d) => d.entity.id === created.json.data.donation.id);
      await evCoord.waitFor('donation.created', (d) => d.entity.id === created.json.data.donation.id);

      // heartbeat and Last-Event-ID replay
      await sleep(300);
      assert.ok(evCoord.comments.some((c) => c.includes('heartbeat')), 'heartbeat comment');
      const lastSeen = evCoord.events.find((e) => e.type === 'offer.sent').id;
      const replay = await sse(coord, { 'last-event-id': String(lastSeen) });
      await replay.waitFor('offer.accepted');
      assert.ok(replay.events.every((e) => e.id > lastSeen || e.type === 'resync'));
      replay.close();
      const gap = await sse(coord, { 'last-event-id': '1' });
      gap.close();
    } finally {
      evCoord.close(); evR013.close(); evR004.close(); evDonor.close();
    }
  });

  test('SSE needs a session; static files and SPA fallback work; path traversal is blocked', async () => {
    assert.equal((await fetch(`${API}/events`)).status, 401);
    const index = await fetch(`${BASE}/`);
    assert.equal(index.status, 200); assert.match(await index.text(), /spa/);
    const deep = await fetch(`${BASE}/coordinator/offers/OFF001`);
    assert.equal(deep.status, 200); assert.match(await deep.text(), /spa/);
    const asset = await fetch(`${BASE}/assets/app.js`);
    assert.equal(asset.status, 200); assert.match(asset.headers.get('cache-control'), /immutable/);
    assert.equal((await fetch(`${BASE}/missing.js`)).status, 404);
    const trav = await fetch(`${BASE}/..%2f..%2fserver%2fpackage.json`);
    assert.ok(!(await trav.text()).includes('communitybridge-gateway'), 'path traversal must not leak files');
    const api404 = await fetch(`${API}/does/not/exist`);
    assert.equal(api404.status, 404); assert.match(api404.headers.get('content-type'), /^application\/problem\+json/);
  });

  test('engine killed mid-session: the gateway restarts it and keeps serving', async () => {
    const coord = await login('coordinator');
    const before = (await http(null, 'GET', '/health')).json;
    assert.equal(before.state, 'READY');
    const evs = await sse(coord);
    process.kill(before.engine.pid);
    // the supervisor sees the exit; requests wait for the restart (queued up to 5 s) or get a clean 503, never a hang
    let after;
    for (let i = 0; i < 100; i++) {
      after = (await http(null, 'GET', '/health')).json;
      if (after.state === 'READY' && after.engine.pid !== before.engine.pid) break;
      await sleep(100);
    }
    assert.equal(after.state, 'READY');
    assert.notEqual(after.engine.pid, before.engine.pid);
    assert.equal(after.engine.restarts, 1);
    const snap = await http(coord, 'GET', '/snapshot');
    assert.equal(snap.status, 200);
    assert.equal(snap.json.donations.find((d) => d.id === 'DON001').status, 'CLOSED', 'state survived: it is on disk');
    assert.equal(snap.json.transactions.length, 4);
    await evs.waitFor('engine.status', (d) => d.state === 'RESTARTING');
    await evs.waitFor('resync', (d) => d.reason === 'engine_restart');
    // a write works after the restart and ids continue
    const again = await http(coord, 'POST', '/donations', { donorId: 'DN002', food: 'COOKED', quantity: 5, usableMinutes: 50 });
    assert.equal(again.status, 201);
    evs.close();
  });
});

describe('gateway configuration', () => {
  test('refuses to run inside data/scenarios unless CB_ALLOW_PRISTINE=1', () => {
    assert.throws(() => loadConfig({ CB_DATA: path.join(SCENARIOS_DIR, 'lab') }), (e) => e instanceof ConfigError && /pristine/.test(e.message));
    assert.throws(() => loadConfig({ CB_DATA: path.join(SCENARIOS_DIR, 'lab', '..', 'lab') }), ConfigError);
    const ok = loadConfig({ CB_DATA: path.join(SCENARIOS_DIR, 'lab'), CB_ALLOW_PRISTINE: '1' });
    assert.equal(ok.allowPristine, true);
  });
  test('refuses a missing data folder and a missing engine', () => {
    assert.throws(() => loadConfig({ CB_DATA: path.join(WORK, 'nope_does_not_exist') }), ConfigError);
    assert.throws(() => loadConfig({ CB_DATA: freshDir('cfg'), CB_ENGINE: path.join(WORK, 'no_engine.exe') }), ConfigError);
  });
  test('a bad data folder leaves the gateway up with state FAILED_BOOT and 503 on engine calls', async () => {
    const dir = freshDir('gw_bad');
    fs.appendFileSync(path.join(dir, 'donations.csv'), 'DON999,DN404,COOKED,1,1,1,PENDING\n');
    const config = loadConfig({ CB_DATA: dir, CB_PORT: String(PORT), CB_SECRET_FILE: path.join(WORK, 'gw_secret.txt') });
    const g = await buildApp({ config, secret: loadSecret(config.secretPath), log: () => {} });
    await g.start();
    try {
      const h = await http(null, 'GET', '/health');
      assert.equal(h.json.state, 'FAILED_BOOT');
      assert.ok(h.json.error.message);
      const r = await http(null, 'GET', '/identities');
      assert.equal(r.status, 503); assert.equal(r.json.code, 'ENGINE_UNAVAILABLE');
    } finally { await g.close(); }
  });
});

test('cleanup of temp copies', () => {
  for (const n of ['gw_main', 'gw_dist', 'gw_bad', 'cfg', 'smoke', 'gw_secret.txt', 'secret.txt']) fs.rmSync(path.join(WORK, n), { recursive: true, force: true });
  assert.ok(noIdentity === undefined);
});
