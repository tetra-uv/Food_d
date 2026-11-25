// The gateway application: REST + SSE over the engine. It contains no business
// rule: every decision comes from cb_engine.exe; this file does sessions, role checks, validation, HTTP mapping, events.
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import Fastify from 'fastify';
import { Supervisor, EngineFailure } from './supervisor.js';
import { EventHub } from './events.js';
import { COOKIE, makeCookieValue, readCookieValue, parseCookies, setCookieHeader, clearCookieHeader } from './auth.js';

// ---------------------------------------------------------------------------------------------- errors
export class HttpError extends Error {
  constructor(status, code, detail, details) {
    super(detail);
    this.status = status;
    this.code = code;
    this.details = details;
  }
}

const STATUS = {
  BAD_ARGS: 400, BAD_REQUEST: 400, VALIDATION_FAILED: 400, UNKNOWN_COMMAND: 400, PROTOCOL_VERSION: 500,
  INVALID_NAME: 422, INVALID_TYPE: 422, INVALID_NODE: 422, NO_FOOD_TYPES: 422, INVALID_CAPACITY: 422, INVALID_MINUTES: 422,
  BAD_QUANTITY: 422, QUANTITY_OUT_OF_RANGE: 422,
  UNAUTHENTICATED: 401, FORBIDDEN: 403,
  DONATION_NOT_FOUND: 404, RECIPIENT_NOT_FOUND: 404, DONOR_NOT_FOUND: 404, OFFER_NOT_FOUND: 404, NODE_NOT_FOUND: 404, NOT_FOUND: 404,
  OFFER_ALREADY_ANSWERED: 409, DONATION_NOT_OPEN: 409, NOT_A_CANDIDATE: 409, EXCEEDS_REMAINING: 409, EXCEEDS_CAPACITY: 409,
  NOT_FEASIBLE: 409, OPERATION_REFUSED: 409,
  BUSY: 429, SAVE_FAILED: 503, ENGINE_UNAVAILABLE: 503, OUTCOME_UNKNOWN: 504, INTERNAL: 500, DATA_INVALID: 500, LINE_TOO_LONG: 400,
};

const TITLES = { 400: 'Bad request', 401: 'Not signed in', 403: 'Not allowed', 404: 'Not found', 409: 'Conflict', 422: 'Invalid value', 429: 'Busy', 500: 'Internal error', 503: 'Unavailable', 504: 'Outcome unknown' };

function fromEngineError(error) {
  return new HttpError(STATUS[error.code] ?? 500, error.code, error.message, error.details);
}

// ---------------------------------------------------------------------------------------------- validation
const ID_RE = /^[A-Z]{1,4}\d{1,9}$/;
const FOODS = ['COOKED', 'PACKAGED', 'PRODUCE', 'BAKERY'];
const bad = (msg, details) => new HttpError(400, 'VALIDATION_FAILED', msg, details);

function bodyObject(body, allowed, required = []) {
  if (body === undefined || body === null) body = {};
  if (typeof body !== 'object' || Array.isArray(body)) throw bad('The body must be a JSON object');
  for (const k of Object.keys(body)) if (!allowed.includes(k)) throw bad(`Unknown field ${k}`, { field: k });
  for (const k of required) if (body[k] === undefined) throw bad(`Missing field ${k}`, { field: k });
  return body;
}
function vId(v, name) {
  if (typeof v !== 'string' || !ID_RE.test(v)) throw bad(`${name} is not a valid id`, { field: name });
  return v;
}
function vInt(v, name, min, max) {
  if (!Number.isInteger(v) || v < min || v > max) throw bad(`${name} must be a whole number from ${min} to ${max}`, { field: name, min, max });
  return v;
}
function vFood(v, name = 'food') {
  if (!FOODS.includes(v)) throw bad(`${name} must be one of ${FOODS.join(', ')}`, { field: name });
  return v;
}
function vName(v, name) {
  if (typeof v !== 'string') throw bad(`${name} must be text`, { field: name });
  const t = v.trim();
  if (t.length < 1 || t.length > 60 || /[,\u0000-\u001f\u007f]/.test(t)) throw new HttpError(422, 'INVALID_NAME', 'The name must be 1 to 60 characters and cannot contain a comma', { field: name });
  return t;
}
function vType(v, name) {
  if (typeof v !== 'string') throw bad(`${name} must be text`, { field: name });
  const t = v.trim();
  if (t.length < 1 || t.length > 30 || !/^[A-Za-z0-9_ ]+$/.test(t)) throw new HttpError(422, 'INVALID_TYPE', 'The type must be 1 to 30 characters: letters, digits, spaces and underscores', { field: name });
  return t;
}
function vBool(v, name) {
  if (typeof v !== 'boolean') throw bad(`${name} must be true or false`, { field: name });
  return v;
}
const MAX_QTY = 1_000_000, MAX_MIN = 100_000_000;

// ---------------------------------------------------------------------------------------------- app
/**
 * @param {{config:object, secret:Buffer, log?:Function}} opts
 */
export async function buildApp({ config, secret, log = () => {} }) {
  const sup = new Supervisor({ exe: config.enginePath, dataDir: config.dataDir, log });
  const hub = new EventHub({ ringSize: config.ringSize, heartbeatMs: config.heartbeatMs });
  const app = Fastify({ logger: false, bodyLimit: 64 * 1024, trustProxy: false });

  let everReady = false;
  sup.on('status', (s) => {
    hub.publish('engine.status', { state: s.state, bootId: s.bootId ?? null, rev: sup.rev, at: new Date().toISOString() }, {}, true);
    if (s.state === 'READY') {
      if (everReady) hub.publish('resync', { reason: 'engine_restart' }, {}, true);
      everReady = true;
    }
  });

  // ---- engine access
  async function engine(cmd, args = {}) {
    try {
      return await sup.call(cmd, args);
    } catch (e) {
      if (e instanceof EngineFailure) throw new HttpError(STATUS[e.code] ?? 503, e.code, e.message, e.details);
      throw e;
    }
  }
  async function engineOk(cmd, args = {}, session = null) {
    const r = await engine(cmd, args);
    if (!r.ok) {
      if (r.error?.details?.stateChanged) await publishEffects(r.error.details.effects, r.rev, session);
      throw fromEngineError(r.error);
    }
    return r;
  }
  const etagOf = (rev) => `"${sup.bootId}:${rev}"`;

  // optional `context` of an offer, built from the snapshot: what a recipient needs to show an incoming offer
  function offerContext(snapData, offer) {
    const d = snapData.donations.find((x) => x.id === offer.donationId);
    const donor = d ? snapData.donors.find((x) => x.id === d.donorId) : null;
    const rec = snapData.recipients.find((x) => x.id === offer.recipientId);
    return {
      food: d?.food ?? null, usableMinutes: d?.usableMinutes ?? null,
      donorId: donor?.id ?? d?.donorId ?? null, donorName: donor?.name ?? null, donorNode: donor?.node ?? null,
      recipientName: rec?.name ?? null, recipientNode: rec?.node ?? null,
    };
  }
  const withContext = (snapData, offer) => ({ ...offer, context: offerContext(snapData, offer) });

  let snapCache = null;
  async function snapshot() {
    const key = `${sup.bootId}:${sup.rev}`;
    if (snapCache && snapCache.key === key) return snapCache;
    const r = await engineOk('read.snapshot');
    snapCache = { key: `${sup.bootId}:${r.rev}`, rev: r.rev, data: r.data };
    return snapCache;
  }

  let netCache = null;
  let geoCache = { mtime: -1, value: null };
  function readGeo() {
    const file = path.join(config.dataDir, 'geo_overlay.json');
    try {
      const st = fs.statSync(file);
      if (st.mtimeMs !== geoCache.mtime) {
        geoCache = { mtime: st.mtimeMs, value: JSON.parse(fs.readFileSync(file, 'utf8')) };
      }
      return geoCache.value;
    } catch (e) {
      if (e.code !== 'ENOENT') log('warn', `geo_overlay.json ignored: ${e.message}`);
      geoCache = { mtime: -1, value: null };
      return null;
    }
  }
  async function network() {
    if (!netCache || netCache.bootId !== sup.bootId) {
      const r = await engineOk('read.network');
      netCache = { bootId: sup.bootId, value: r.data };
    }
    const geo = readGeo();
    return geo ? { ...netCache.value, geo } : netCache.value;
  }

  // ---- events from effects
  async function publishEffects(effects, rev, session) {
    if (!effects) return;
    let snap;
    try { snap = (await snapshot()).data; } catch { return; }
    const donationDonor = new Map(snap.donations.map((d) => [d.id, d.donorId]));
    const offerById = new Map(snap.offers.map((o) => [o.id, o]));
    const base = () => ({ rev, bootId: sup.bootId, at: new Date().toISOString(), actor: { role: session?.role ?? 'system', id: session?.identityId ?? null } });
    const aud = (donationId, recipientId) => ({ donors: donationDonor.has(donationId) ? [donationDonor.get(donationId)] : [], recipients: recipientId ? [recipientId] : [] });

    for (const e of effects.offers || []) {
      const o = offerById.get(e.id);
      if (!o) continue;
      let type = null;
      if (!e.before) type = o.status === 'PENDING' ? 'offer.sent' : null;
      else if (e.before.status === 'PENDING' && e.after.status) type = { ACCEPTED: 'offer.accepted', DECLINED: 'offer.declined', WITHDRAWN: 'offer.withdrawn' }[e.after.status] ?? null;
      if (!type) continue;
      hub.publish(type, {
        ...base(), entity: { type: 'offer', id: o.id }, offerId: o.id, donationId: o.donationId, recipientId: o.recipientId,
        quantity: o.quantity, travelMinutes: o.travelMinutes, status: o.status, txnId: o.txnId, context: offerContext(snap, o),
      }, aud(o.donationId, o.recipientId));
    }
    for (const t of effects.transactions || []) {
      const a = t.after;
      hub.publish('transaction.created', { ...base(), entity: { type: 'transaction', id: t.id }, txnId: t.id, ...a }, aud(a.donationId, a.recipientId));
    }
    for (const d of effects.donations || []) {
      const donorId = donationDonor.get(d.id);
      const audience = { donors: donorId ? [donorId] : [], recipients: [] };
      if (!d.before) hub.publish('donation.created', { ...base(), entity: { type: 'donation', id: d.id }, donation: snap.donations.find((x) => x.id === d.id) }, audience);
      else if (d.after.status === 'CLOSED') hub.publish('donation.closed', { ...base(), entity: { type: 'donation', id: d.id }, donationId: d.id, remaining: snap.donations.find((x) => x.id === d.id)?.remaining }, audience);
      else if ('remaining' in d.after || 'status' in d.after) hub.publish('donation.updated', { ...base(), entity: { type: 'donation', id: d.id }, donationId: d.id, changes: d.after }, audience);
    }
    for (const r of effects.recipients || []) {
      if (!r.before) hub.publish('recipient.created', { ...base(), entity: { type: 'recipient', id: r.id } }, {});
      else hub.publish('recipient.updated', { ...base(), entity: { type: 'recipient', id: r.id }, recipientId: r.id, changes: r.after }, { recipients: [r.id] });
    }
    for (const d of effects.donors || []) {
      if (!d.before) hub.publish('donor.created', { ...base(), entity: { type: 'donor', id: d.id } }, {});
    }
  }

  async function mutate(req, reply, cmd, args, status = 200) {
    const r = await engineOk(cmd, args, req.session);
    await publishEffects(r.effects, r.rev, req.session);
    reply.code(status).header('ETag', etagOf(r.rev));
    let data = r.data;
    if (data && data.offer) data = { ...data, offer: withContext((await snapshot()).data, data.offer) };
    return { revision: r.rev, etag: etagOf(r.rev), message: r.message ?? null, effects: r.effects, data };
  }
  async function read(reply, cmd, args, shape = (d) => d) {
    const r = await engineOk(cmd, args);
    reply.header('ETag', etagOf(r.rev)).header('Cache-Control', 'no-store');
    return shape(r.data, r);
  }

  // ---- request plumbing
  app.removeContentTypeParser('application/json');
  app.addContentTypeParser('application/json', { parseAs: 'string' }, (req, body, done) => {
    if (!body) { done(null, {}); return; }
    try { done(null, JSON.parse(body)); } catch { done(new HttpError(400, 'BAD_REQUEST', 'The body is not valid JSON')); }
  });

  app.addHook('onRequest', async (req, reply) => {
    req.requestId = (typeof req.headers['x-request-id'] === 'string' && req.headers['x-request-id'].slice(0, 100)) || crypto.randomUUID();
    reply.header('X-Request-Id', req.requestId);
    const cookies = parseCookies(req.headers.cookie);
    req.session = readCookieValue(secret, cookies[COOKIE]);
    // CSRF: a browser sends Origin on cross-site posts; accept only same host or a local development origin
    if (req.method !== 'GET' && req.method !== 'HEAD' && req.method !== 'OPTIONS' && req.headers.origin) {
      let ok = false;
      try {
        const o = new URL(req.headers.origin);
        const host = String(req.headers.host || '').replace(/:\d+$/, '');
        ok = ['localhost', '127.0.0.1', '[::1]'].includes(o.hostname) || o.hostname === host;
      } catch { /* bad origin */ }
      if (!ok) throw new HttpError(403, 'FORBIDDEN', 'Cross-site requests are not allowed');
    }
  });

  app.setErrorHandler((err, req, reply) => {
    let status = 500, code = 'INTERNAL', detail = 'Internal error', details;
    if (err instanceof HttpError) { status = err.status; code = err.code; detail = err.message; details = err.details; }
    else if (err.statusCode && err.statusCode >= 400 && err.statusCode < 500) {
      status = err.statusCode;
      code = status === 413 ? 'VALIDATION_FAILED' : 'BAD_REQUEST';
      detail = status === 413 ? 'The request body is too large' : 'The request could not be read';
    } else log('error', `unhandled error: ${err.stack || err}`);
    reply.code(status).header('Content-Type', 'application/problem+json').send({
      type: `https://communitybridge.local/problems/${code}`, code, title: TITLES[status] || 'Error', detail, status,
      requestId: req.requestId, ...(details ? { details } : {}),
    });
  });

  const need = (req) => { if (!req.session) throw new HttpError(401, 'UNAUTHENTICATED', 'Choose a role first'); return req.session; };
  const needRole = (req, ...roles) => {
    const s = need(req);
    if (!roles.includes(s.role)) throw new HttpError(403, 'FORBIDDEN', `The ${s.role} role cannot do this`);
    return s;
  };

  // ---------------------------------------------------------------------------------------------- API
  await app.register(async (api) => {
    // -- meta
    api.get('/health', async (req, reply) => {
      reply.header('Cache-Control', 'no-store');
      const i = sup.info();
      return { state: i.state, engine: { pid: i.pid, uptimeSec: i.uptimeSec, restarts: i.restarts }, rev: i.rev, bootId: i.bootId, scenario: config.scenario, ...(i.error ? { error: i.error } : {}) };
    });

    api.get('/meta', async (req, reply) => {
      const hello = await read(reply, 'sys.hello', {});
      return {
        engineVersion: hello.engineVersion, backendManifest: hello.backendManifest, counts: hello.counts, bootId: hello.bootId, rev: hello.rev,
        warnings: req.session?.role === 'coordinator' ? sup.warnings : [],
        scenario: config.scenario, simulated: true, capabilities: { geo: readGeo() !== null },
      };
    });

    api.get('/identities', async (req, reply) => {
      const s = (await snapshot()).data;
      reply.header('Cache-Control', 'no-store');
      return { donors: s.donors.map((d) => ({ id: d.id, name: d.name })), recipients: s.recipients.map((r) => ({ id: r.id, name: r.name })) };
    });

    // -- session
    api.post('/session', async (req, reply) => {
      const b = bodyObject(req.body, ['role', 'identityId', 'pin'], ['role']);
      if (!['coordinator', 'donor', 'recipient'].includes(b.role)) throw bad('role must be coordinator, donor or recipient', { field: 'role' });
      let identityId = null, name = null;
      if (b.role !== 'coordinator') {
        identityId = vId(b.identityId, 'identityId');
        const s = (await snapshot()).data;
        const found = (b.role === 'donor' ? s.donors : s.recipients).find((x) => x.id === identityId);
        if (!found) throw new HttpError(404, b.role === 'donor' ? 'DONOR_NOT_FOUND' : 'RECIPIENT_NOT_FOUND', 'No such organisation');
        name = found.name;
      }
      reply.header('Set-Cookie', setCookieHeader(makeCookieValue(secret, { role: b.role, identityId })));
      return { role: b.role, identityId, name };
    });
    api.get('/session', async (req) => {
      const s = need(req);
      let name = null;
      if (s.identityId) {
        const snap = (await snapshot()).data;
        name = (s.role === 'donor' ? snap.donors : snap.recipients).find((x) => x.id === s.identityId)?.name ?? null;
      }
      return { role: s.role, identityId: s.identityId, name };
    });
    api.delete('/session', async (req, reply) => {
      return reply.header('Set-Cookie', clearCookieHeader()).code(204).send();
    });

    // -- reads
    api.get('/snapshot', async (req, reply) => {
      needRole(req, 'coordinator');
      const r = await engineOk('read.snapshot');
      reply.header('ETag', etagOf(r.rev)).header('Cache-Control', 'no-store');
      return { etag: etagOf(r.rev), revision: r.rev, bootId: sup.bootId, ...r.data, offers: r.data.offers.map((o) => withContext(r.data, o)) };
    });

    api.get('/network', async (req, reply) => {
      const s = need(req);
      const net = await network();
      reply.header('ETag', `"${sup.bootId}:net"`).header('Cache-Control', 'no-cache');
      if (s.role === 'coordinator') return net;
      // donors and recipients see the topology but only their own organisation and the counterparts of their offers
      const snap = (await snapshot()).data;
      const donorOf = new Map(snap.donations.map((d) => [d.id, d.donorId]));
      const showDonors = new Set(), showRecipients = new Set();
      if (s.role === 'donor') {
        showDonors.add(s.identityId);
        for (const o of snap.offers) if (donorOf.get(o.donationId) === s.identityId) showRecipients.add(o.recipientId);
      } else {
        showRecipients.add(s.identityId);
        for (const o of snap.offers) if (o.recipientId === s.identityId) showDonors.add(donorOf.get(o.donationId));
      }
      return { ...net, nodes: net.nodes.map((n) => ({ ...n, donors: n.donors.filter((d) => showDonors.has(d)), recipients: n.recipients.filter((r) => showRecipients.has(r)) })) };
    });

    api.get('/queue', async (req, reply) => { needRole(req, 'coordinator'); return read(reply, 'read.queue', {}); });
    api.get('/summary', async (req, reply) => {
      needRole(req, 'coordinator');
      const s = (await snapshot()).data;
      reply.header('Cache-Control', 'no-store');
      return { ...s.summary, stats: s.coordinatorStats };
    });
    api.get('/stats/coordinator', async (req, reply) => { needRole(req, 'coordinator'); return read(reply, 'read.stats', { role: 'coordinator' }); });
    api.get('/stats/donor/:id', async (req, reply) => {
      const s = need(req);
      if (s.role === 'recipient' || (s.role === 'donor' && s.identityId !== req.params.id)) throw new HttpError(403, 'FORBIDDEN', 'Not your record');
      return read(reply, 'read.stats', { role: 'donor', id: req.params.id });
    });
    api.get('/stats/recipient/:id', async (req, reply) => {
      const s = need(req);
      if (s.role === 'donor' || (s.role === 'recipient' && s.identityId !== req.params.id)) throw new HttpError(403, 'FORBIDDEN', 'Not your record');
      return read(reply, 'read.stats', { role: 'recipient', id: req.params.id });
    });

    api.get('/donors', async (req, reply) => {
      const s = need(req);
      if (s.role === 'recipient') throw new HttpError(403, 'FORBIDDEN', 'Not allowed');
      const snap = (await snapshot()).data;
      reply.header('Cache-Control', 'no-store');
      const q = String(req.query.q || '').toLowerCase();
      return snap.donors.filter((d) => (s.role === 'coordinator' || d.id === s.identityId) && (!q || d.name.toLowerCase().includes(q)));
    });
    api.get('/donors/:id', async (req) => {
      const s = need(req);
      if (s.role === 'recipient' || (s.role === 'donor' && s.identityId !== req.params.id)) throw new HttpError(403, 'FORBIDDEN', 'Not your record');
      const d = (await snapshot()).data.donors.find((x) => x.id === req.params.id);
      if (!d) throw new HttpError(404, 'DONOR_NOT_FOUND', 'That donor could not be found');
      return d;
    });
    api.get('/recipients', async (req, reply) => {
      const s = need(req);
      if (s.role === 'donor') throw new HttpError(403, 'FORBIDDEN', 'Not allowed');
      const snap = (await snapshot()).data;
      reply.header('Cache-Control', 'no-store');
      const q = String(req.query.q || '').toLowerCase();
      return snap.recipients.filter((r) => (s.role === 'coordinator' || r.id === s.identityId)
        && (!q || r.name.toLowerCase().includes(q))
        && (req.query.accepting === undefined || String(r.accepting) === req.query.accepting)
        && (!req.query.food || r.acceptedTypes.includes(req.query.food))
        && (!req.query.type || r.type === req.query.type));
    });
    api.get('/recipients/:id', async (req) => {
      const s = need(req);
      if (s.role === 'donor' || (s.role === 'recipient' && s.identityId !== req.params.id)) throw new HttpError(403, 'FORBIDDEN', 'Not your record');
      const r = (await snapshot()).data.recipients.find((x) => x.id === req.params.id);
      if (!r) throw new HttpError(404, 'RECIPIENT_NOT_FOUND', 'That recipient could not be found');
      return r;
    });

    api.get('/donations', async (req, reply) => {
      const s = need(req);
      if (s.role === 'recipient') throw new HttpError(403, 'FORBIDDEN', 'Not allowed');
      const snap = (await snapshot()).data;
      reply.header('Cache-Control', 'no-store');
      let rows = snap.donations.filter((d) => (s.role === 'coordinator' || d.donorId === s.identityId)
        && (!req.query.status || d.status === req.query.status) && (!req.query.food || d.food === req.query.food)
        && (!req.query.donor || d.donorId === req.query.donor) && (!req.query.q || d.id.includes(String(req.query.q).toUpperCase())));
      if (req.query.sort === 'urgency') {
        const q = (await engineOk('read.queue')).data.order.map((o) => o.donationId);
        const rank = new Map(q.map((id, i) => [id, i]));
        rows = rows.filter((d) => rank.has(d.id)).sort((a, b) => rank.get(a.id) - rank.get(b.id));
      }
      return rows;
    });
    api.get('/donations/:id', async (req) => {
      const s = need(req);
      const d = (await snapshot()).data.donations.find((x) => x.id === req.params.id);
      if (!d) throw new HttpError(404, 'DONATION_NOT_FOUND', 'That donation could not be found');
      if (s.role === 'recipient' || (s.role === 'donor' && d.donorId !== s.identityId)) throw new HttpError(403, 'FORBIDDEN', 'Not your donation');
      return d;
    });
    api.get('/donations/:id/match', async (req, reply) => { needRole(req, 'coordinator'); return read(reply, 'match.build', { donationId: req.params.id }); });
    api.get('/donations/:id/compare', async (req, reply) => { needRole(req, 'coordinator'); return read(reply, 'match.compare', { donationId: req.params.id }); });

    api.get('/network/field', async (req, reply) => {
      needRole(req, 'coordinator');
      const source = Number(req.query.source);
      if (!Number.isInteger(source)) throw bad('source must be a node number', { field: 'source' });
      return read(reply, 'net.field', { source });
    });
    api.get('/network/route', async (req, reply) => {
      need(req);
      const from = Number(req.query.from), to = Number(req.query.to);
      if (!Number.isInteger(from) || !Number.isInteger(to)) throw bad('from and to must be node numbers');
      return read(reply, 'net.route', { from, to });
    });

    async function visibleOffers(s) {
      const snap = (await snapshot()).data;
      const donorOf = new Map(snap.donations.map((d) => [d.id, d.donorId]));
      return { snap, donorOf, offers: snap.offers.filter((o) => s.role === 'coordinator' || (s.role === 'donor' ? donorOf.get(o.donationId) === s.identityId : o.recipientId === s.identityId)) };
    }
    api.get('/offers', async (req, reply) => {
      const s = need(req);
      const { snap, offers } = await visibleOffers(s);
      reply.header('Cache-Control', 'no-store');
      return offers.map((o) => withContext(snap, o)).filter((o) => (!req.query.status || o.status === req.query.status) && (!req.query.donation || o.donationId === req.query.donation) && (!req.query.recipient || o.recipientId === req.query.recipient));
    });
    api.get('/offers/:id/check', async (req, reply) => {
      const s = need(req);
      if (s.role === 'donor') throw new HttpError(403, 'FORBIDDEN', 'Not allowed');
      if (s.role === 'recipient') {
        const o = (await snapshot()).data.offers.find((x) => x.id === req.params.id);
        if (o && o.recipientId !== s.identityId) throw new HttpError(403, 'FORBIDDEN', 'Not your offer');
      }
      return read(reply, 'offer.check', { offerId: req.params.id });
    });
    api.get('/transactions', async (req, reply) => {
      const s = need(req);
      const snap = (await snapshot()).data;
      const donorOf = new Map(snap.donations.map((d) => [d.id, d.donorId]));
      reply.header('Cache-Control', 'no-store');
      return snap.transactions.filter((t) => (s.role === 'coordinator' || (s.role === 'donor' ? donorOf.get(t.donationId) === s.identityId : t.recipientId === s.identityId))
        && (!req.query.donation || t.donationId === req.query.donation) && (!req.query.recipient || t.recipientId === req.query.recipient)
        && (!req.query.donor || donorOf.get(t.donationId) === req.query.donor));
    });

    // -- decision support
    api.post('/match/preview', async (req, reply) => {
      const s = needRole(req, 'coordinator', 'donor');
      const b = bodyObject(req.body, ['donorId', 'node', 'food', 'quantity', 'usableMinutes'], ['food', 'quantity', 'usableMinutes']);
      const args = { food: vFood(b.food), quantity: vInt(b.quantity, 'quantity', 1, MAX_QTY), usableMinutes: vInt(b.usableMinutes, 'usableMinutes', 1, MAX_MIN) };
      if (s.role === 'donor') {
        if (b.node !== undefined || (b.donorId !== undefined && b.donorId !== s.identityId)) throw new HttpError(403, 'FORBIDDEN', 'A donor can only preview for its own location');
        args.donorId = s.identityId;
      } else if (b.donorId !== undefined) args.donorId = vId(b.donorId, 'donorId');
      else if (b.node !== undefined) args.node = vInt(b.node, 'node', 0, 1_000_000);
      else throw bad('Give donorId or node');
      const r = await engineOk('match.preview', args);
      reply.header('Cache-Control', 'no-store');
      if (s.role === 'coordinator') return r.data;
      const byReason = {};
      for (const x of r.data.rejected) byReason[x.reason] = (byReason[x.reason] || 0) + 1;
      return {
        feasibleCount: r.data.candidates.length,
        bestTravelMinutes: r.data.candidates.length ? Math.min(...r.data.candidates.map((c) => c.travelMinutes)) : null,
        byReason,
      };
    });

    // -- mutations
    api.post('/donations', async (req, reply) => {
      const s = needRole(req, 'coordinator', 'donor');
      const b = bodyObject(req.body, ['donorId', 'food', 'quantity', 'usableMinutes'], ['food', 'quantity', 'usableMinutes']);
      let donorId;
      if (s.role === 'donor') {
        if (b.donorId !== undefined && b.donorId !== s.identityId) throw new HttpError(403, 'FORBIDDEN', 'A donor can only add donations for itself');
        donorId = s.identityId;
      } else donorId = vId(b.donorId, 'donorId');
      return mutate(req, reply, 'donation.create', { donorId, food: vFood(b.food), quantity: vInt(b.quantity, 'quantity', 1, MAX_QTY), usableMinutes: vInt(b.usableMinutes, 'usableMinutes', 1, MAX_MIN) }, 201);
    });
    api.post('/donations/with-new-donor', async (req, reply) => {
      needRole(req, 'coordinator');
      const b = bodyObject(req.body, ['donorName', 'donorType', 'node', 'food', 'quantity', 'usableMinutes'], ['donorName', 'donorType', 'node', 'food', 'quantity', 'usableMinutes']);
      return mutate(req, reply, 'donation.createWithNewDonor', {
        donorName: vName(b.donorName, 'donorName'), donorType: vType(b.donorType, 'donorType'), node: vInt(b.node, 'node', 0, 1_000_000),
        food: vFood(b.food), quantity: vInt(b.quantity, 'quantity', 1, MAX_QTY), usableMinutes: vInt(b.usableMinutes, 'usableMinutes', 1, MAX_MIN),
      }, 201);
    });
    api.post('/donations/:id/close', async (req, reply) => {
      needRole(req, 'coordinator');
      const b = bodyObject(req.body, ['confirm']);
      if (b.confirm !== true) throw bad('Closing a donation needs {"confirm":true}', { field: 'confirm' });
      return mutate(req, reply, 'donation.close', { donationId: vId(req.params.id, 'id') });
    });
    api.post('/offers', async (req, reply) => {
      needRole(req, 'coordinator');
      const b = bodyObject(req.body, ['donationId', 'recipientId', 'quantity'], ['donationId', 'recipientId', 'quantity']);
      return mutate(req, reply, 'offer.send', { donationId: vId(b.donationId, 'donationId'), recipientId: vId(b.recipientId, 'recipientId'), quantity: vInt(b.quantity, 'quantity', 0, MAX_QTY) }, 201);
    });
    async function ownOffer(req, id) {
      const s = needRole(req, 'recipient');
      vId(id, 'id');
      const o = (await snapshot()).data.offers.find((x) => x.id === id);
      if (!o) throw new HttpError(404, 'OFFER_NOT_FOUND', 'That offer could not be found');
      if (o.recipientId !== s.identityId) throw new HttpError(403, 'FORBIDDEN', 'This offer is for another recipient');
    }
    api.post('/offers/:id/accept', async (req, reply) => {
      await ownOffer(req, req.params.id);
      bodyObject(req.body, []);
      return mutate(req, reply, 'offer.accept', { offerId: req.params.id });
    });
    api.post('/offers/:id/decline', async (req, reply) => {
      await ownOffer(req, req.params.id);
      const b = bodyObject(req.body, ['confirm']);
      if (b.confirm !== true) throw bad('Declining an offer needs {"confirm":true}', { field: 'confirm' });
      return mutate(req, reply, 'offer.decline', { offerId: req.params.id });
    });
    api.post('/offers/:id/withdraw', async (req, reply) => {
      needRole(req, 'coordinator');
      const b = bodyObject(req.body, ['confirm']);
      if (b.confirm !== true) throw bad('Withdrawing an offer needs {"confirm":true}', { field: 'confirm' });
      return mutate(req, reply, 'offer.withdraw', { offerId: vId(req.params.id, 'id') });
    });
    api.post('/donors', async (req, reply) => {
      needRole(req, 'coordinator');
      const b = bodyObject(req.body, ['name', 'type', 'node'], ['name', 'type', 'node']);
      return mutate(req, reply, 'donor.create', { name: vName(b.name, 'name'), type: vType(b.type, 'type'), node: vInt(b.node, 'node', 0, 1_000_000) }, 201);
    });
    api.post('/recipients', async (req, reply) => {
      needRole(req, 'coordinator');
      const b = bodyObject(req.body, ['name', 'type', 'foods', 'capacity', 'accepting', 'pickup', 'node'], ['name', 'type', 'foods', 'capacity', 'accepting', 'pickup', 'node']);
      if (!Array.isArray(b.foods) || b.foods.length < 1) throw new HttpError(422, 'NO_FOOD_TYPES', 'Please choose at least one kind of food');
      return mutate(req, reply, 'recipient.create', {
        name: vName(b.name, 'name'), type: vType(b.type, 'type'), foods: b.foods.map((f) => vFood(f, 'foods')), capacity: vInt(b.capacity, 'capacity', 0, MAX_MIN),
        accepting: vBool(b.accepting, 'accepting'), pickup: vBool(b.pickup, 'pickup'), node: vInt(b.node, 'node', 0, 1_000_000),
      }, 201);
    });
    api.patch('/recipients/:id', async (req, reply) => {
      const s = needRole(req, 'coordinator', 'recipient');
      const id = vId(req.params.id, 'id');
      if (s.role === 'recipient' && s.identityId !== id) throw new HttpError(403, 'FORBIDDEN', 'You can only change your own details');
      const b = bodyObject(req.body, ['capacity', 'accepting', 'pickup']);
      const args = { recipientId: id };
      if (b.capacity !== undefined) args.capacity = vInt(b.capacity, 'capacity', 0, MAX_MIN);
      if (b.accepting !== undefined) args.accepting = vBool(b.accepting, 'accepting');
      if (b.pickup !== undefined) args.pickup = vBool(b.pickup, 'pickup');
      if (Object.keys(args).length === 1) throw bad('Give at least one of capacity, accepting or pickup');
      return mutate(req, reply, 'recipient.update', args);
    });
    api.post('/admin/reload', async (req, reply) => {
      needRole(req, 'coordinator');
      const r = await engineOk('sys.reload', {}, req.session);
      sup.bootId = r.data.bootId;
      snapCache = null; netCache = null;
      hub.publish('resync', { reason: 'reload' }, {}, true);
      reply.header('ETag', etagOf(r.rev));
      return { revision: r.rev, etag: etagOf(r.rev), data: r.data };
    });

    // -- realtime
    api.get('/events', async (req, reply) => {
      const s = need(req);
      reply.hijack();
      hub.attach(req.raw, reply.raw, s);
    });

    api.setNotFoundHandler((req, reply) => {
      reply.code(404).header('Content-Type', 'application/problem+json').send({
        type: 'https://communitybridge.local/problems/NOT_FOUND', code: 'NOT_FOUND', title: 'Not found', detail: `No endpoint ${req.method} ${req.url.split('?')[0]}`, status: 404, requestId: req.requestId,
      });
    });
  }, { prefix: '/api/v1' });

  // ---------------------------------------------------------------------------------------------- static files + SPA fallback
  const TYPES = { '.html': 'text/html; charset=utf-8', '.js': 'text/javascript; charset=utf-8', '.mjs': 'text/javascript; charset=utf-8', '.css': 'text/css; charset=utf-8', '.json': 'application/json; charset=utf-8', '.svg': 'image/svg+xml', '.png': 'image/png', '.jpg': 'image/jpeg', '.jpeg': 'image/jpeg', '.webp': 'image/webp', '.ico': 'image/x-icon', '.woff2': 'font/woff2', '.woff': 'font/woff', '.txt': 'text/plain; charset=utf-8', '.map': 'application/json', '.pmtiles': 'application/octet-stream' };
  app.setNotFoundHandler((req, reply) => {
    const pathname = decodeURIComponent((req.url.split('?')[0]) || '/');
    if ((req.method !== 'GET' && req.method !== 'HEAD') || pathname.startsWith('/api/')) {
      reply.code(404).header('Content-Type', 'application/problem+json').send({ type: 'https://communitybridge.local/problems/NOT_FOUND', code: 'NOT_FOUND', title: 'Not found', detail: 'No such resource', status: 404, requestId: req.requestId });
      return;
    }
    const root = config.webDist;
    if (!fs.existsSync(root)) {
      reply.code(404).type('text/plain; charset=utf-8').send('The web app has not been built yet (web/dist is missing). The API is at /api/v1/health.');
      return;
    }
    let file = path.resolve(root, '.' + path.posix.normalize('/' + pathname));
    const inside = file === root || file.startsWith(root + path.sep);
    let st = inside ? (fs.statSync(file, { throwIfNoEntry: false }) ?? null) : null;
    if (st?.isDirectory()) { file = path.join(file, 'index.html'); st = fs.statSync(file, { throwIfNoEntry: false }) ?? null; }
    if (!st || !st.isFile()) {
      if (path.extname(pathname)) { reply.code(404).type('text/plain; charset=utf-8').send('Not found'); return; }
      file = path.join(root, 'index.html');            // single-page-app fallback
      if (!fs.existsSync(file)) { reply.code(404).send('Not found'); return; }
    }
    const ext = path.extname(file).toLowerCase();
    reply.header('Content-Type', TYPES[ext] || 'application/octet-stream');
    reply.header('Cache-Control', file.includes(`${path.sep}assets${path.sep}`) ? 'public, max-age=31536000, immutable' : 'no-cache');
    reply.send(fs.createReadStream(file));
  });

  return {
    app, sup, hub,
    async start() {
      sup.start();
      // wait until the engine is ready or has failed to boot, so start-up problems show at once
      await new Promise((resolve) => {
        const done = () => { sup.off('status', onStatus); resolve(); };
        const onStatus = (s) => { if (s.state === 'READY' || s.state === 'FAILED_BOOT') done(); };
        if (sup.state === 'READY' || sup.state === 'FAILED_BOOT') done(); else sup.on('status', onStatus);
      });
      await app.listen({ port: config.port, host: config.bind });
      return app.server.address();
    },
    async close() {
      hub.closeAll();
      await sup.stop();
      await app.close();
    },
  };
}
