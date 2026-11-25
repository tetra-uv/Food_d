// DEV-ONLY mock gateway (port 8790). Serves the doc 05 endpoints from static fixtures in mock/fixtures/.
// It does NOT implement the product's business rules: match results are replayed from fixtures
// (generated once by mock/gen-fixtures.mjs). Only trivial bookkeeping is kept in memory so the UI flows can be exercised
// (offers, accept/decline/withdraw, close, create donation, SSE). Never use this for anything but UI development.
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const fx = (n) => JSON.parse(fs.readFileSync(path.join(here, 'fixtures', n), 'utf8'));
const PORT = Number(process.env.MOCK_PORT || 8790);
const BASE = '/api/v1';

const network = fx('network.json');
const matches = fx('matches.json');
const previews = fx('preview.json');
const fields = fx('fields.json');
const routes = fx('routes.json');
const previewKeys = Object.keys(previews).map(Number).sort((a, b) => a - b);

// geo overlay: prefer the real data file if it exists, else the clearly labelled TEMPORARY fake one
const realGeo = path.resolve(here, '../../data/scenarios/delhi-geo/geo_overlay.json');
const geoPath = fs.existsSync(realGeo) ? realGeo : path.join(here, 'fixtures', 'geo_overlay.TEMP-FAKE.json');
const geoOverlay = JSON.parse(fs.readFileSync(geoPath, 'utf8'));
let geoOn = process.argv.includes('--geo') || process.env.MOCK_GEO === '1';

let S; // mutable state
function reset() {
  const e = fx('entities.json');
  S = { ...e, rev: 17, bootId: 'mock' + Date.now().toString(36).slice(-4), seq: 0, buffer: [], idem: new Map(), engine: 'ready' };
}
reset();

const clients = new Set();
const sessions = new Map();

// ---------- helpers ----------
const json = (res, status, body, headers = {}) => {
  const s = JSON.stringify(body);
  res.writeHead(status, { 'Content-Type': 'application/json; charset=utf-8', 'Cache-Control': 'no-store', ETag: `"${S.bootId}:${S.rev}"`, ...headers });
  res.end(s);
};
const problem = (res, status, code, detail, details) =>
  json(res, status, { type: 'about:blank', code, title: code, detail, status, requestId: 'mock-' + Date.now().toString(36), ...(details ? { details } : {}) }, { 'Content-Type': 'application/problem+json' });
const body = (req) => new Promise((ok) => { let b = ''; req.on('data', (c) => (b += c)); req.on('end', () => { try { ok(b ? JSON.parse(b) : {}); } catch { ok(null); } }); });
const sessionOf = (req) => {
  const m = /cb_session=([^;]+)/.exec(req.headers.cookie || '');
  if (!m) return null;
  try { return JSON.parse(Buffer.from(decodeURIComponent(m[1]), 'base64').toString('utf8')); } catch { return null; }
};
const donorOf = (id) => S.donors.find((d) => d.id === id);
const recOf = (id) => S.recipients.find((r) => r.id === id);
const donOf = (id) => S.donations.find((d) => d.id === id);
const isOpen = (d) => d.status === 'PENDING' || d.status === 'PARTIAL';

function emit(type, entityType, entityId, extra = {}, actor = { role: 'coordinator', id: null }) {
  const id = ++S.seq;
  const ev = { id, type, data: { rev: S.rev, bootId: S.bootId, at: new Date().toISOString(), actor, entity: { type: entityType, id: entityId }, ...extra } };
  S.buffer.push(ev); if (S.buffer.length > 500) S.buffer.shift();
  for (const c of clients) write(c, ev);
}
const write = (res, ev) => res.write(`id: ${ev.id}\nevent: ${ev.type}\ndata: ${JSON.stringify(ev.data)}\n\n`);

function enrichOffer(o) {
  const d = donOf(o.donationId), r = recOf(o.recipientId), dn = d && donorOf(d.donorId);
  return { ...o, context: d && r && dn ? { food: d.food, usableMinutes: d.usableMinutes, donorId: dn.id, donorName: dn.name, donorNode: dn.node, recipientName: r.name, recipientNode: r.node } : undefined };
}
function recompute(d) {
  const pend = S.offers.filter((o) => o.donationId === d.id && o.status === 'PENDING').reduce((s, o) => s + o.quantity, 0);
  d.pendingOffered = pend;
  d.effectiveRemaining = isOpen(d) ? d.remaining - pend : 0;
}
function stats() {
  const open = S.donations.filter(isOpen);
  return {
    openDonations: open.length, waiting: open.filter((d) => d.effectiveRemaining > 0).length,
    pendingOffers: S.offers.filter((o) => o.status === 'PENDING').length,
    givenOut: S.transactions.reduce((s, t) => s + t.quantity, 0),
    wasted: S.donations.filter((d) => d.status === 'CLOSED').reduce((s, d) => s + d.remaining, 0),
  };
}
function summary() {
  const byFood = { COOKED: 0, PACKAGED: 0, PRODUCE: 0, BAKERY: 0 };
  for (const t of S.transactions) byFood[donOf(t.donationId)?.food] += t.quantity;
  return {
    offered: S.donations.reduce((s, d) => s + d.original, 0), allocated: S.transactions.reduce((s, t) => s + t.quantity, 0),
    wasted: stats().wasted, stillOpen: S.donations.filter(isOpen).reduce((s, d) => s + d.remaining, 0), byFood,
  };
}
function scoped(req) {
  const s = sessionOf(req);
  const role = s?.role || 'coordinator';
  const donations = role === 'donor' ? S.donations.filter((d) => d.donorId === s.identityId) : S.donations;
  let offers = S.offers, txns = S.transactions;
  if (role === 'donor') { const ids = new Set(donations.map((d) => d.id)); offers = offers.filter((o) => ids.has(o.donationId)); txns = txns.filter((t) => ids.has(t.donationId)); }
  if (role === 'recipient') { offers = offers.filter((o) => o.recipientId === s.identityId); txns = txns.filter((t) => t.recipientId === s.identityId); }
  return { s, role, donations, offers, txns };
}

function matchFor(d) {
  const keyed = matches[`${d.id}@${d.effectiveRemaining}`];
  if (keyed) return keyed;
  const base = matches[d.id];
  if (base && base.donation.effectiveRemaining === d.effectiveRemaining) return base;
  // fixture fallback (no rules): replay DON001's/own base view with this donation's header. Marked so it is never mistaken for real.
  const src = base || matches.DON001;
  const dn = donorOf(d.donorId);
  return { ...src, mockNote: 'replayed fixture; ranking not recomputed by the mock', donation: { ...src.donation, id: d.id, food: d.food, original: d.original, remaining: d.remaining, effectiveRemaining: d.effectiveRemaining, pendingOffered: d.pendingOffered, usableMinutes: d.usableMinutes, status: d.status, donorId: d.donorId, donorNode: dn ? dn.node : src.donation.donorNode } };
}
function previewFor(b, role) {
  const u = Number(b.usableMinutes);
  let key = previewKeys[0];
  for (const k of previewKeys) if (k <= u) key = k;
  const v = previews[key];
  const q = Number(b.quantity) || 60;
  const view = { ...v, mockNote: 'preview replayed from fixture (food/quantity/node not re-evaluated by the mock)', donation: { ...v.donation, food: b.food || v.donation.food, original: q, remaining: q, effectiveRemaining: q, usableMinutes: u } };
  if (role === 'donor') {
    const byReason = {};
    for (const r of v.rejected) byReason[r.reason] = (byReason[r.reason] || 0) + 1;
    return { feasibleCount: v.survivors, bestTravelMinutes: v.candidates.length ? Math.min(...v.candidates.map((c) => c.travelMinutes)) : null, byReason, totalRecipients: v.totals.recipients };
  }
  return view;
}

const mutOk = (res, status, message, effects, data) => {
  S.rev++;
  json(res, status, { revision: S.rev, etag: `"${S.bootId}:${S.rev}"`, message, effects, data });
};

// ---------- routing ----------
const routesTable = [];
const R = (method, re, fn) => routesTable.push({ method, re: new RegExp('^' + re + '$'), fn });

R('GET', '/health', (req, res) => json(res, 200, { state: S.engine, engine: { pid: 0, uptimeSec: 1 }, rev: S.rev, bootId: S.bootId, scenario: 'lab' }));
R('GET', '/meta', (req, res) => json(res, 200, {
  engineVersion: 'mock-0.1', backendManifest: { hash: 'mock-fixture-not-a-real-hash' }, counts: { donors: S.donors.length, recipients: S.recipients.length, donations: S.donations.length },
  warnings: ['Mock gateway: answers are replayed from static fixtures, not computed by the C++ engine.'], scenario: 'lab', simulated: true, capabilities: { geo: geoOn }, bootId: S.bootId,
}));
R('GET', '/identities', (req, res) => json(res, 200, { donors: S.donors.map((d) => ({ id: d.id, name: d.name, type: d.type, node: d.node })), recipients: S.recipients.map((r) => ({ id: r.id, name: r.name, type: r.type, node: r.node })) }));
R('POST', '/session', async (req, res) => {
  const b = await body(req);
  if (!b || !['coordinator', 'donor', 'recipient'].includes(b.role)) return problem(res, 400, 'VALIDATION_FAILED', 'role must be coordinator, donor or recipient');
  if (b.role !== 'coordinator' && !(b.role === 'donor' ? donorOf(b.identityId) : recOf(b.identityId))) return problem(res, 422, 'VALIDATION_FAILED', 'unknown identity');
  const s = { role: b.role, identityId: b.identityId || null };
  const name = b.role === 'donor' ? donorOf(b.identityId).name : b.role === 'recipient' ? recOf(b.identityId).name : 'Coordinator';
  s.name = name;
  json(res, 200, s, { 'Set-Cookie': `cb_session=${encodeURIComponent(Buffer.from(JSON.stringify(s)).toString('base64'))}; Path=/; HttpOnly; SameSite=Lax` });
});
R('GET', '/session', (req, res) => { const s = sessionOf(req); return s ? json(res, 200, s) : problem(res, 401, 'UNAUTHENTICATED', 'No session'); });
R('DELETE', '/session', (req, res) => json(res, 200, { ok: true }, { 'Set-Cookie': 'cb_session=; Path=/; Max-Age=0' }));

R('GET', '/snapshot', (req, res) => json(res, 200, { etag: `${S.bootId}:${S.rev}`, donors: S.donors, recipients: S.recipients, donations: S.donations, transactions: S.transactions, offers: S.offers.map(enrichOffer), summary: summary(), stats: stats() }));
R('GET', '/network', (req, res) => json(res, 200, geoOn ? { ...network, geo: geoOverlay } : network, { 'Cache-Control': 'no-store' }));
R('GET', '/queue', (req, res) => {
  const open = S.donations.filter((d) => isOpen(d) && d.effectiveRemaining > 0).sort((a, b) => a.usableMinutes - b.usableMinutes || a.id.localeCompare(b.id));
  json(res, 200, { order: open.map((d) => ({ donationId: d.id, usableMinutes: d.usableMinutes, effectiveRemaining: d.effectiveRemaining })), nextId: open[0]?.id ?? null });
});
R('GET', '/donors', (req, res) => json(res, 200, S.donors));
R('GET', '/donors/(DN\\d+)', (req, res, m) => { const d = donorOf(m[1]); return d ? json(res, 200, d) : problem(res, 404, 'DONOR_NOT_FOUND', 'No such donor'); });
R('GET', '/recipients', (req, res) => json(res, 200, S.recipients));
R('GET', '/recipients/(R\\d+)', (req, res, m) => { const d = recOf(m[1]); return d ? json(res, 200, d) : problem(res, 404, 'RECIPIENT_NOT_FOUND', 'No such recipient'); });
R('GET', '/donations', (req, res) => json(res, 200, scoped(req).donations));
R('GET', '/donations/(DON\\d+)', (req, res, m) => { const d = donOf(m[1]); return d ? json(res, 200, d) : problem(res, 404, 'DONATION_NOT_FOUND', 'No such donation'); });
R('GET', '/transactions', (req, res) => json(res, 200, scoped(req).txns));
R('GET', '/offers', (req, res) => json(res, 200, scoped(req).offers.map(enrichOffer)));
R('GET', '/summary', (req, res) => json(res, 200, summary()));
R('GET', '/stats/coordinator', (req, res) => json(res, 200, stats()));
R('GET', '/stats/donor/(DN\\d+)', (req, res, m) => {
  const ds = S.donations.filter((d) => d.donorId === m[1]); const ids = new Set(ds.map((d) => d.id));
  json(res, 200, { donations: ds.length, openDonations: ds.filter(isOpen).length, pendingOffers: S.offers.filter((o) => ids.has(o.donationId) && o.status === 'PENDING').length, givenOut: S.transactions.filter((t) => ids.has(t.donationId)).reduce((s, t) => s + t.quantity, 0) });
});
R('GET', '/stats/recipient/(R\\d+)', (req, res, m) => json(res, 200, { incoming: S.offers.filter((o) => o.recipientId === m[1] && o.status === 'PENDING').length, received: S.transactions.filter((t) => t.recipientId === m[1]).reduce((s, t) => s + t.quantity, 0) }));

R('GET', '/donations/(DON\\d+)/match', (req, res, m) => { const d = donOf(m[1]); return d ? json(res, 200, matchFor(d)) : problem(res, 404, 'DONATION_NOT_FOUND', 'No such donation'); });
R('POST', '/match/preview', async (req, res) => {
  const b = await body(req);
  if (!b || !b.food || !(b.usableMinutes >= 1) || !(b.quantity >= 1)) return problem(res, 400, 'VALIDATION_FAILED', 'food, quantity and usableMinutes are required');
  await new Promise((r) => setTimeout(r, 30));
  json(res, 200, previewFor(b, scoped(req).role));
});
R('GET', '/network/field', (req, res) => {
  const u = new URL(req.url, 'http://x'); const src = Number(u.searchParams.get('source'));
  return fields[src] ? json(res, 200, fields[src]) : problem(res, 404, 'NODE_NOT_FOUND', 'No such node');
});
R('GET', '/network/route', (req, res) => {
  const u = new URL(req.url, 'http://x'); const r = routes[`${u.searchParams.get('from')}-${u.searchParams.get('to')}`];
  return r ? json(res, 200, r) : problem(res, 404, 'NODE_NOT_FOUND', 'No such node');
});
R('GET', '/offers/(OFF\\d+)/check', (req, res, m) => {
  const o = S.offers.find((x) => x.id === m[1]); if (!o) return problem(res, 404, 'OFFER_NOT_FOUND', 'No such offer');
  const r = recOf(o.recipientId);
  if (o.status !== 'PENDING') return json(res, 200, { wouldSucceed: false, code: 'OFFER_ALREADY_ANSWERED', message: 'This offer has already been answered' });
  if (r.availableCapacity < o.quantity) return json(res, 200, { wouldSucceed: false, code: 'EXCEEDS_CAPACITY', message: `Capacity ${r.availableCapacity} is below the offered ${o.quantity}`, details: { max: r.availableCapacity } });
  json(res, 200, { wouldSucceed: true });
});

// ---- mutations ----
const needKey = (req, res) => { if (!req.headers['idempotency-key']) { problem(res, 400, 'VALIDATION_FAILED', 'Idempotency-Key header is required'); return false; } return true; };
R('POST', '/donations', async (req, res) => {
  if (!needKey(req, res)) return;
  const b = await body(req); const { s } = scoped(req);
  const donorId = s?.role === 'donor' ? s.identityId : b?.donorId;
  if (!b || !donorOf(donorId)) return problem(res, 404, 'DONOR_NOT_FOUND', 'No such donor');
  if (!['COOKED', 'PACKAGED', 'PRODUCE', 'BAKERY'].includes(b.food)) return problem(res, 422, 'VALIDATION_FAILED', 'Unknown food type');
  if (!Number.isInteger(b.quantity) || b.quantity < 1) return problem(res, 422, 'BAD_QUANTITY', 'The quantity must be at least 1');
  if (!Number.isInteger(b.usableMinutes) || b.usableMinutes < 1) return problem(res, 422, 'INVALID_MINUTES', 'The usable minutes must be at least 1');
  const id = 'DON' + String(Math.max(...S.donations.map((d) => +d.id.slice(3))) + 1).padStart(3, '0');
  const d = { id, donorId, food: b.food, original: b.quantity, remaining: b.quantity, usableMinutes: b.usableMinutes, status: 'PENDING', pendingOffered: 0, effectiveRemaining: b.quantity };
  S.donations.push(d);
  mutOk(res, 201, `Donation ${id} created`, { donations: [{ id, after: d }] }, { donation: d });
  emit('donation.created', 'donation', id, { effects: { donations: [{ id, after: d }] } }, { role: s?.role || 'coordinator', id: donorId });
});
R('POST', '/donations/(DON\\d+)/close', async (req, res, m) => {
  if (!needKey(req, res)) return;
  const b = await body(req); if (!b?.confirm) return problem(res, 400, 'VALIDATION_FAILED', 'confirm:true is required');
  const d = donOf(m[1]); if (!d) return problem(res, 404, 'DONATION_NOT_FOUND', 'No such donation');
  if (!isOpen(d)) return problem(res, 409, 'DONATION_NOT_OPEN', 'This donation is no longer open');
  const before = { remaining: d.remaining, status: d.status }, wasted = d.remaining, withdrawn = [];
  for (const o of S.offers) if (o.donationId === d.id && o.status === 'PENDING') { o.status = 'WITHDRAWN'; withdrawn.push(o.id); }
  d.status = 'CLOSED'; recompute(d);
  mutOk(res, 200, `Donation ${d.id} closed, ${wasted} recorded as wasted`, { donations: [{ id: d.id, before, after: { remaining: d.remaining, status: 'CLOSED' } }], offers: withdrawn.map((id) => ({ id, after: { status: 'WITHDRAWN' } })) }, { donation: d, wasted });
  emit('donation.closed', 'donation', d.id, { wasted });
});
R('POST', '/offers', async (req, res) => {
  if (!needKey(req, res)) return;
  const b = await body(req); const d = b && donOf(b.donationId);
  if (!d) return problem(res, 404, 'DONATION_NOT_FOUND', 'No such donation');
  if (!isOpen(d)) return problem(res, 409, 'DONATION_NOT_OPEN', 'This donation is no longer open');
  const view = matchFor(d); const c = view.candidates.find((x) => x.recipientId === b.recipientId);
  if (!c) { const rj = view.rejected.find((x) => x.recipientId === b.recipientId); return problem(res, 409, 'NOT_A_CANDIDATE', `${b.recipientId} cannot take this donation`, { reason: rj ? rj.reason : 'DECLINED_EARLIER' }); }
  if (!Number.isInteger(b.quantity) || b.quantity < 1 || b.quantity > c.accept) return problem(res, 422, 'QUANTITY_OUT_OF_RANGE', `The quantity must be from 1 to ${c.accept}`, { min: 1, max: c.accept });
  const id = 'OFF' + String(S.offers.length + 1).padStart(3, '0');
  const o = { id, donationId: d.id, recipientId: b.recipientId, quantity: b.quantity, travelMinutes: c.travelMinutes, status: 'PENDING', txnId: null };
  S.offers.push(o); const before = { pendingOffered: d.pendingOffered, effectiveRemaining: d.effectiveRemaining }; recompute(d);
  const effects = { offers: [{ id, after: o }], donations: [{ id: d.id, before, after: { pendingOffered: d.pendingOffered, effectiveRemaining: d.effectiveRemaining } }] };
  mutOk(res, 201, `Offer ${id} sent to ${recOf(b.recipientId).name}`, effects, { offer: enrichOffer(o) });
  emit('offer.sent', 'offer', id, { offerId: id, donationId: d.id, recipientId: b.recipientId, quantity: o.quantity, travelMinutes: o.travelMinutes, effects });
});
const answer = (action) => async (req, res, m) => {
  if (!needKey(req, res)) return;
  const b = await body(req);
  const o = S.offers.find((x) => x.id === m[1]); if (!o) return problem(res, 404, 'OFFER_NOT_FOUND', 'That offer could not be found');
  if (action !== 'accept' && !b?.confirm) return problem(res, 400, 'VALIDATION_FAILED', 'confirm:true is required');
  if (o.status !== 'PENDING') return problem(res, 409, 'OFFER_ALREADY_ANSWERED', 'This offer has already been answered');
  const d = donOf(o.donationId), r = recOf(o.recipientId);
  if (action === 'accept') {
    if (r.availableCapacity < o.quantity) return problem(res, 409, 'EXCEEDS_CAPACITY', `Only ${r.availableCapacity} fits at this recipient`, { max: r.availableCapacity });
    const tid = 'T' + String(S.transactions.length + 1).padStart(3, '0');
    const tx = { id: tid, donationId: d.id, recipientId: r.id, quantity: o.quantity, travelMinutes: o.travelMinutes };
    S.transactions.push(tx);
    const before = { donation: { remaining: d.remaining, status: d.status }, cap: r.availableCapacity };
    d.remaining -= o.quantity; r.availableCapacity -= o.quantity; o.status = 'ACCEPTED'; o.txnId = tid;
    d.status = d.remaining === 0 ? 'COMPLETED' : 'PARTIAL'; recompute(d);
    const effects = {
      offers: [{ id: o.id, before: { status: 'PENDING' }, after: { status: 'ACCEPTED', txnId: tid } }],
      donations: [{ id: d.id, before: before.donation, after: { remaining: d.remaining, status: d.status } }],
      recipients: [{ id: r.id, before: { capacity: before.cap }, after: { capacity: r.availableCapacity } }],
      transactions: [{ id: tid, after: tx }],
    };
    mutOk(res, 200, `The offer was accepted and recorded as ${tid}`, effects, { offer: enrichOffer(o), transaction: tx });
    emit('offer.accepted', 'offer', o.id, { offerId: o.id, txnId: tid, donationId: d.id, recipientId: r.id, effects });
    emit('transaction.created', 'transaction', tid, { effects });
    return;
  }
  o.status = action === 'decline' ? 'DECLINED' : 'WITHDRAWN'; recompute(d);
  const effects = { offers: [{ id: o.id, before: { status: 'PENDING' }, after: { status: o.status } }] };
  mutOk(res, 200, `The offer was ${action === 'decline' ? 'declined' : 'withdrawn'}`, effects, { offer: enrichOffer(o) });
  emit(action === 'decline' ? 'offer.declined' : 'offer.withdrawn', 'offer', o.id, { offerId: o.id, donationId: d.id, recipientId: r.id, effects });
};
R('POST', '/offers/(OFF\\d+)/accept', answer('accept'));
R('POST', '/offers/(OFF\\d+)/decline', answer('decline'));
R('POST', '/offers/(OFF\\d+)/withdraw', answer('withdraw'));
R('PATCH', '/recipients/(R\\d+)', async (req, res, m) => {
  if (!needKey(req, res)) return;
  const b = await body(req); const r = recOf(m[1]); if (!r) return problem(res, 404, 'RECIPIENT_NOT_FOUND', 'No such recipient');
  if (b.capacity !== undefined) { if (!Number.isInteger(b.capacity) || b.capacity < 0) return problem(res, 422, 'INVALID_CAPACITY', 'The capacity cannot be below 0'); r.availableCapacity = b.capacity; }
  if (b.accepting !== undefined) r.accepting = !!b.accepting;
  if (b.pickup !== undefined) r.canPickup = !!b.pickup;
  if (b.canPickup !== undefined) r.canPickup = !!b.canPickup;
  mutOk(res, 200, 'Recipient details saved', { recipients: [{ id: r.id, after: r }] }, { recipient: r });
  emit('recipient.updated', 'recipient', r.id, {});
});

// dev helpers
R('POST', '/__mock/geo', async (req, res) => { const b = await body(req); geoOn = !!b?.on; json(res, 200, { geo: geoOn }); });
R('POST', '/__mock/reset', (req, res) => { reset(); for (const c of clients) write(c, { id: ++S.seq, type: 'resync', data: { rev: S.rev, bootId: S.bootId } }); json(res, 200, { ok: true }); });
R('POST', '/__mock/engine', async (req, res) => { const b = await body(req); S.engine = b?.state || 'ready'; emit('engine.status', 'engine', 'engine', { state: S.engine }); json(res, 200, { state: S.engine }); });

R('GET', '/events', (req, res) => {
  res.writeHead(200, { 'Content-Type': 'text/event-stream', 'Cache-Control': 'no-cache, no-transform', Connection: 'keep-alive', 'X-Accel-Buffering': 'no' });
  res.write('retry: 2000\n\n');
  const last = Number(req.headers['last-event-id']);
  if (last) for (const ev of S.buffer) if (ev.id > last) write(res, ev);
  clients.add(res);
  const hb = setInterval(() => res.write(': hb\n\n'), 15000);
  req.on('close', () => { clearInterval(hb); clients.delete(res); });
});

const server = http.createServer(async (req, res) => {
  const u = new URL(req.url, 'http://x');
  if (!u.pathname.startsWith(BASE)) return problem(res, 404, 'NOT_FOUND', 'Mock serves ' + BASE + ' only');
  const p = u.pathname.slice(BASE.length) || '/';
  for (const r of routesTable) {
    const m = r.re.exec(p);
    if (m && r.method === req.method) {
      try { return await r.fn(req, res, m); } catch (e) { console.error(e); return problem(res, 500, 'INTERNAL', String(e)); }
    }
  }
  problem(res, 404, 'NOT_FOUND', `${req.method} ${p} is not in the mock`);
});

export function startMock() {
  return new Promise((ok) => server.listen(PORT, '127.0.0.1', () => { console.log(`[mock] http://127.0.0.1:${PORT}${BASE}  geo=${geoOn} overlay=${path.basename(geoPath)}`); ok(server); }));
}
if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) startMock();
