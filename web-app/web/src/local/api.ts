// Local transport: answers the same /api/v1 endpoints with the same shapes as the Node gateway (server/src/app.js), but
// in this browser, using the WebAssembly engine (engineHost.ts). This is a straight port of the gateway's route ->
// engine-command mapping, role checks, validation, offer `context` enrichment and event payloads.
//
// SECURITY NOTE (demo-grade): the "session" is just {role, identityId} in sessionStorage (per tab, so a coordinator tab and
// a recipient tab can coexist). Anyone can pick any role; the role checks below only keep the UI honest, they protect
// nothing - all data is in the visitor's own browser anyway. Do not reuse this as an access-control model.
import { ApiError } from '../api/errors';
import type { ApiEvent } from '../api/types';
import type { ConnState } from '../api/events';
import { getEngine, type EngineResponse, type LocalEngine, type LocalEventMsg } from './engineHost';

type Role = 'coordinator' | 'donor' | 'recipient';
interface Sess { role: Role; identityId: string | null }

// ---------------------------------------------------------------------------------------------- errors (as app.js)
const STATUS: Record<string, number> = {
  BAD_ARGS: 400, BAD_REQUEST: 400, VALIDATION_FAILED: 400, UNKNOWN_COMMAND: 400, PROTOCOL_VERSION: 500,
  INVALID_NAME: 422, INVALID_TYPE: 422, INVALID_NODE: 422, NO_FOOD_TYPES: 422, INVALID_CAPACITY: 422, INVALID_MINUTES: 422,
  BAD_QUANTITY: 422, QUANTITY_OUT_OF_RANGE: 422,
  UNAUTHENTICATED: 401, FORBIDDEN: 403,
  DONATION_NOT_FOUND: 404, RECIPIENT_NOT_FOUND: 404, DONOR_NOT_FOUND: 404, OFFER_NOT_FOUND: 404, NODE_NOT_FOUND: 404, NOT_FOUND: 404,
  OFFER_ALREADY_ANSWERED: 409, DONATION_NOT_OPEN: 409, NOT_A_CANDIDATE: 409, EXCEEDS_REMAINING: 409, EXCEEDS_CAPACITY: 409,
  NOT_FEASIBLE: 409, OPERATION_REFUSED: 409, DUPLICATE_PENDING_OFFER: 409,
  BUSY: 429, SAVE_FAILED: 503, ENGINE_UNAVAILABLE: 503, OUTCOME_UNKNOWN: 504, INTERNAL: 500, DATA_INVALID: 500, LINE_TOO_LONG: 400,
};
const err = (status: number, code: string, msg: string, details?: Record<string, unknown>) => new ApiError(status, code, msg, details);
const fromEngine = (e: NonNullable<EngineResponse['error']>) => err(STATUS[e.code] ?? 500, e.code, e.message, e.details);

// ---------------------------------------------------------------------------------------------- validation (as app.js)
const ID_RE = /^[A-Z]{1,4}\d{1,9}$/;
const FOODS = ['COOKED', 'PACKAGED', 'PRODUCE', 'BAKERY'];
const bad = (msg: string, details?: Record<string, unknown>) => err(400, 'VALIDATION_FAILED', msg, details);
type Body = Record<string, any>;
function bodyObject(body: unknown, allowed: string[], required: string[] = []): Body {
  if (body === undefined || body === null) body = {};
  if (typeof body !== 'object' || Array.isArray(body)) throw bad('The body must be a JSON object');
  const b = body as Body;
  for (const k of Object.keys(b)) if (b[k] !== undefined && !allowed.includes(k)) throw bad(`Unknown field ${k}`, { field: k });
  for (const k of required) if (b[k] === undefined) throw bad(`Missing field ${k}`, { field: k });
  return b;
}
function vId(v: unknown, name: string): string { if (typeof v !== 'string' || !ID_RE.test(v)) throw bad(`${name} is not a valid id`, { field: name }); return v; }
function vInt(v: unknown, name: string, min: number, max: number): number {
  if (!Number.isInteger(v) || (v as number) < min || (v as number) > max) throw bad(`${name} must be a whole number from ${min} to ${max}`, { field: name, min, max });
  return v as number;
}
function vFood(v: unknown, name = 'food'): string { if (typeof v !== 'string' || !FOODS.includes(v)) throw bad(`${name} must be one of ${FOODS.join(', ')}`, { field: name }); return v; }
function vName(v: unknown, name: string): string {
  if (typeof v !== 'string') throw bad(`${name} must be text`, { field: name });
  const t = v.trim();
  if (t.length < 1 || t.length > 60 || /[,\u0000-\u001f\u007f]/.test(t)) throw err(422, 'INVALID_NAME', 'The name must be 1 to 60 characters and cannot contain a comma', { field: name });
  return t;
}
function vType(v: unknown, name: string): string {
  if (typeof v !== 'string') throw bad(`${name} must be text`, { field: name });
  const t = v.trim();
  if (t.length < 1 || t.length > 30 || !/^[A-Za-z0-9_ ]+$/.test(t)) throw err(422, 'INVALID_TYPE', 'The type must be 1 to 30 characters: letters, digits, spaces and underscores', { field: name });
  return t;
}
function vBool(v: unknown, name: string): boolean { if (typeof v !== 'boolean') throw bad(`${name} must be true or false`, { field: name }); return v; }
const MAX_QTY = 1_000_000, MAX_MIN = 100_000_000;

// ---------------------------------------------------------------------------------------------- session (sessionStorage, per tab)
let eng: LocalEngine | null = null;
const sessKey = () => `cb-session-${eng?.scenario.name ?? 'x'}`;
function getSession(): Sess | null {
  try {
    const raw = sessionStorage.getItem(sessKey());
    if (!raw) return null;
    const s = JSON.parse(raw) as Sess;
    return ['coordinator', 'donor', 'recipient'].includes(s.role) ? s : null;
  } catch { return null; }
}
function setSession(s: Sess | null) {
  try { if (s) sessionStorage.setItem(sessKey(), JSON.stringify(s)); else sessionStorage.removeItem(sessKey()); } catch { memSession = s; }
  memSession = s;
}
let memSession: Sess | null = null;
const currentSession = () => getSession() ?? memSession;

const need = (s: Sess | null): Sess => { if (!s) throw err(401, 'UNAUTHENTICATED', 'Choose a role first'); return s; };
const needRole = (s: Sess | null, ...roles: Role[]): Sess => {
  const x = need(s);
  if (!roles.includes(x.role)) throw err(403, 'FORBIDDEN', `The ${x.role} role cannot do this`);
  return x;
};

// ---------------------------------------------------------------------------------------------- engine helpers
async function engineOk(cmd: string, args: Record<string, unknown> = {}, signal?: AbortSignal): Promise<EngineResponse> {
  let r: EngineResponse;
  try { r = await eng!.read(cmd, args, signal); }
  catch (e) { if ((e as Error).name === 'AbortError') throw e; throw err(503, 'ENGINE_UNAVAILABLE', `The in-browser engine failed: ${(e as Error).message}`); }
  if (!r.ok) throw fromEngine(r.error!);
  return r;
}
let snapCache: { key: number; data: any; rev: number } | null = null;
async function snapshot(): Promise<{ data: any; rev: number }> {
  const key = eng!.stateKey;
  if (snapCache && snapCache.key === key) return snapCache;
  const r = await engineOk('read.snapshot');
  snapCache = { key, data: r.data, rev: r.rev ?? 0 };
  return snapCache;
}
let netCache: { key: number; value: any } | null = null;
async function network(): Promise<any> {
  // topology never changes through the API except donor/recipient creation, which changes the org lists: key on stateKey
  if (!netCache || netCache.key !== eng!.stateKey) {
    const r = await engineOk('read.network');
    netCache = { key: eng!.stateKey, value: eng!.geo ? { ...r.data, geo: eng!.geo } : r.data };
  }
  return netCache.value;
}
const bootId = () => String((eng?.ready?.bootId as string | undefined) ?? 'local');
const etagOf = (rev: number) => `"${bootId()}:${rev}"`;

function offerContext(snap: any, offer: any) {
  const d = snap.donations.find((x: any) => x.id === offer.donationId);
  const donor = d ? snap.donors.find((x: any) => x.id === d.donorId) : null;
  const rec = snap.recipients.find((x: any) => x.id === offer.recipientId);
  return {
    food: d?.food ?? null, usableMinutes: d?.usableMinutes ?? null,
    donorId: donor?.id ?? d?.donorId ?? null, donorName: donor?.name ?? null, donorNode: donor?.node ?? null,
    recipientName: rec?.name ?? null, recipientNode: rec?.node ?? null,
  };
}
const withContext = (snap: any, offer: any) => ({ ...offer, context: offerContext(snap, offer) });

// ---------------------------------------------------------------------------------------------- events (as publishEffects in app.js)
function buildEvents(effects: any, rev: number, session: Sess | null, snap: any): LocalEventMsg[] {
  const out: LocalEventMsg[] = [];
  if (!effects || !snap) return out;
  const pub = (type: string, data: Record<string, unknown>, audience: LocalEventMsg['audience'] = {}, broadcast = false) => out.push({ type, data, audience, broadcast });
  const donationDonor = new Map<string, string>(snap.donations.map((d: any) => [d.id, d.donorId]));
  const offerById = new Map<string, any>(snap.offers.map((o: any) => [o.id, o]));
  const base = () => ({ rev, bootId: bootId(), at: new Date().toISOString(), actor: { role: session?.role ?? 'system', id: session?.identityId ?? null } });
  const aud = (donationId: string, recipientId?: string) => ({ donors: donationDonor.has(donationId) ? [donationDonor.get(donationId)!] : [], recipients: recipientId ? [recipientId] : [] });

  for (const e of effects.offers || []) {
    const o = offerById.get(e.id);
    if (!o) continue;
    let type: string | null = null;
    if (!e.before) type = o.status === 'PENDING' ? 'offer.sent' : null;
    else if (e.before.status === 'PENDING' && e.after.status) type = ({ ACCEPTED: 'offer.accepted', DECLINED: 'offer.declined', WITHDRAWN: 'offer.withdrawn' } as Record<string, string>)[e.after.status] ?? null;
    if (!type) continue;
    pub(type, {
      ...base(), entity: { type: 'offer', id: o.id }, offerId: o.id, donationId: o.donationId, recipientId: o.recipientId,
      quantity: o.quantity, travelMinutes: o.travelMinutes, status: o.status, txnId: o.txnId, context: offerContext(snap, o),
    }, aud(o.donationId, o.recipientId));
  }
  for (const t of effects.transactions || []) {
    const a = t.after;
    pub('transaction.created', { ...base(), entity: { type: 'transaction', id: t.id }, txnId: t.id, ...a }, aud(a.donationId, a.recipientId));
  }
  for (const d of effects.donations || []) {
    const donorId = donationDonor.get(d.id);
    const audience = { donors: donorId ? [donorId] : [], recipients: [] };
    if (!d.before) pub('donation.created', { ...base(), entity: { type: 'donation', id: d.id }, donation: snap.donations.find((x: any) => x.id === d.id) }, audience);
    else if (d.after.status === 'CLOSED') pub('donation.closed', { ...base(), entity: { type: 'donation', id: d.id }, donationId: d.id, remaining: snap.donations.find((x: any) => x.id === d.id)?.remaining }, audience);
    else if ('remaining' in d.after || 'status' in d.after) pub('donation.updated', { ...base(), entity: { type: 'donation', id: d.id }, donationId: d.id, changes: d.after }, audience);
  }
  for (const r of effects.recipients || []) {
    if (!r.before) pub('recipient.created', { ...base(), entity: { type: 'recipient', id: r.id } });
    else pub('recipient.updated', { ...base(), entity: { type: 'recipient', id: r.id }, recipientId: r.id, changes: r.after }, { recipients: [r.id] });
  }
  for (const d of effects.donors || []) if (!d.before) pub('donor.created', { ...base(), entity: { type: 'donor', id: d.id } });
  return out;
}

/** Coordinator sees everything; a donor/recipient sees events whose audience lists its identity (events.js canSee). */
function canSee(s: Sess | null, ev: LocalEventMsg): boolean {
  if (!s) return false;
  if (s.role === 'coordinator' || ev.broadcast) return true;
  if (s.role === 'donor') return (ev.audience.donors || []).includes(s.identityId ?? '');
  return (ev.audience.recipients || []).includes(s.identityId ?? '');
}

async function mutate(session: Sess, cmd: string, args: Record<string, unknown>, precheck?: (snap: any) => EngineResponse['error'] | null) {
  let out: { resp: EngineResponse; snap: any };
  try { out = await eng!.write(cmd, args, precheck); }
  catch (e) { throw err(504, 'OUTCOME_UNKNOWN', `The in-browser engine failed during this change: ${(e as Error).message}. Reload the page to check.`); }
  const { resp, snap } = out;
  const effects = resp.ok ? resp.effects : resp.error?.details?.effects;
  if (snap) { snapCache = { key: eng!.stateKey, data: snap, rev: resp.rev ?? 0 }; eng!.publish(buildEvents(effects, resp.rev ?? 0, session, snap)); }
  if (!resp.ok) throw fromEngine(resp.error!);
  let data = resp.data;
  if (data && data.offer && snap) data = { ...data, offer: withContext(snap, data.offer) };
  return { revision: resp.rev ?? 0, etag: etagOf(resp.rev ?? 0), message: resp.message ?? null, effects: resp.effects, data };
}

// ---------------------------------------------------------------------------------------------- routes
type Q = URLSearchParams;
type Handler = (ctx: { s: Sess | null; p: string[]; q: Q; body: unknown; signal?: AbortSignal }) => Promise<unknown>;
const routes: { method: string; re: RegExp; h: Handler }[] = [];
const route = (method: string, pattern: string, h: Handler) => {
  routes.push({ method, re: new RegExp('^' + pattern.replace(/:[a-z]+/g, '([^/]+)') + '$'), h });
};

route('GET', '/health', async () => ({ state: 'READY', engine: { pid: null, uptimeSec: 0, restarts: 0 }, rev: (await snapshot()).rev, bootId: bootId(), scenario: eng!.scenario.name }));
route('GET', '/meta', async ({ s }) => {
  const hello = (await engineOk('sys.hello')).data;
  const warnings = (eng!.ready?.warnings as string[] | undefined) ?? [];
  return {
    engineVersion: hello.engineVersion, backendManifest: hello.backendManifest, counts: hello.counts, bootId: hello.bootId, rev: hello.rev,
    warnings: s?.role === 'coordinator' ? warnings : [],
    scenario: eng!.scenario.name, simulated: true, capabilities: { geo: eng!.geo !== null }, backend: 'local',
  };
});
route('GET', '/identities', async () => {
  const s = (await snapshot()).data;
  return { donors: s.donors.map((d: any) => ({ id: d.id, name: d.name })), recipients: s.recipients.map((r: any) => ({ id: r.id, name: r.name })) };
});
route('POST', '/session', async ({ body }) => {
  const b = bodyObject(body, ['role', 'identityId', 'pin'], ['role']);
  if (!['coordinator', 'donor', 'recipient'].includes(b.role)) throw bad('role must be coordinator, donor or recipient', { field: 'role' });
  let identityId: string | null = null, name: string | null = null;
  if (b.role !== 'coordinator') {
    identityId = vId(b.identityId, 'identityId');
    const s = (await snapshot()).data;
    const found = (b.role === 'donor' ? s.donors : s.recipients).find((x: any) => x.id === identityId);
    if (!found) throw err(404, b.role === 'donor' ? 'DONOR_NOT_FOUND' : 'RECIPIENT_NOT_FOUND', 'No such organisation');
    name = found.name;
  }
  setSession({ role: b.role, identityId });
  return { role: b.role, identityId, name };
});
route('GET', '/session', async ({ s }) => {
  const x = need(s);
  let name: string | null = null;
  if (x.identityId) {
    const snap = (await snapshot()).data;
    name = (x.role === 'donor' ? snap.donors : snap.recipients).find((o: any) => o.id === x.identityId)?.name ?? null;
    if (!name) { setSession(null); throw err(401, 'UNAUTHENTICATED', 'That organisation no longer exists; choose a role again'); }
  }
  return { role: x.role, identityId: x.identityId, name };
});
route('DELETE', '/session', async () => { setSession(null); return null; });

route('GET', '/snapshot', async ({ s }) => {
  needRole(s, 'coordinator');
  const { data, rev } = await snapshot();
  return { etag: etagOf(rev), revision: rev, bootId: bootId(), ...data, offers: data.offers.map((o: any) => withContext(data, o)) };
});
route('GET', '/network', async ({ s }) => {
  const x = need(s);
  const net = await network();
  if (x.role === 'coordinator') return net;
  const snap = (await snapshot()).data;
  const donorOf = new Map<string, string>(snap.donations.map((d: any) => [d.id, d.donorId]));
  const showDonors = new Set<string>(), showRecipients = new Set<string>();
  if (x.role === 'donor') {
    showDonors.add(x.identityId!);
    for (const o of snap.offers) if (donorOf.get(o.donationId) === x.identityId) showRecipients.add(o.recipientId);
  } else {
    showRecipients.add(x.identityId!);
    for (const o of snap.offers) if (o.recipientId === x.identityId) showDonors.add(donorOf.get(o.donationId)!);
  }
  return { ...net, nodes: net.nodes.map((n: any) => ({ ...n, donors: n.donors.filter((d: string) => showDonors.has(d)), recipients: n.recipients.filter((r: string) => showRecipients.has(r)) })) };
});
route('GET', '/queue', async ({ s }) => { needRole(s, 'coordinator'); return (await engineOk('read.queue')).data; });
route('GET', '/summary', async ({ s }) => { needRole(s, 'coordinator'); const d = (await snapshot()).data; return { ...d.summary, stats: d.coordinatorStats }; });
route('GET', '/stats/coordinator', async ({ s }) => { needRole(s, 'coordinator'); return (await engineOk('read.stats', { role: 'coordinator' })).data; });
route('GET', '/stats/donor/:id', async ({ s, p }) => {
  const x = need(s);
  if (x.role === 'recipient' || (x.role === 'donor' && x.identityId !== p[0])) throw err(403, 'FORBIDDEN', 'Not your record');
  return (await engineOk('read.stats', { role: 'donor', id: p[0] })).data;
});
route('GET', '/stats/recipient/:id', async ({ s, p }) => {
  const x = need(s);
  if (x.role === 'donor' || (x.role === 'recipient' && x.identityId !== p[0])) throw err(403, 'FORBIDDEN', 'Not your record');
  return (await engineOk('read.stats', { role: 'recipient', id: p[0] })).data;
});
route('GET', '/donors', async ({ s, q }) => {
  const x = need(s);
  if (x.role === 'recipient') throw err(403, 'FORBIDDEN', 'Not allowed');
  const snap = (await snapshot()).data;
  const n = String(q.get('q') || '').toLowerCase();
  return snap.donors.filter((d: any) => (x.role === 'coordinator' || d.id === x.identityId) && (!n || d.name.toLowerCase().includes(n)));
});
route('GET', '/donors/:id', async ({ s, p }) => {
  const x = need(s);
  if (x.role === 'recipient' || (x.role === 'donor' && x.identityId !== p[0])) throw err(403, 'FORBIDDEN', 'Not your record');
  const d = (await snapshot()).data.donors.find((o: any) => o.id === p[0]);
  if (!d) throw err(404, 'DONOR_NOT_FOUND', 'That donor could not be found');
  return d;
});
route('GET', '/recipients', async ({ s, q }) => {
  const x = need(s);
  if (x.role === 'donor') throw err(403, 'FORBIDDEN', 'Not allowed');
  const snap = (await snapshot()).data;
  const n = String(q.get('q') || '').toLowerCase();
  return snap.recipients.filter((r: any) => (x.role === 'coordinator' || r.id === x.identityId)
    && (!n || r.name.toLowerCase().includes(n))
    && (q.get('accepting') === null || String(r.accepting) === q.get('accepting'))
    && (!q.get('food') || r.acceptedTypes.includes(q.get('food')))
    && (!q.get('type') || r.type === q.get('type')));
});
route('GET', '/recipients/:id', async ({ s, p }) => {
  const x = need(s);
  if (x.role === 'donor' || (x.role === 'recipient' && x.identityId !== p[0])) throw err(403, 'FORBIDDEN', 'Not your record');
  const r = (await snapshot()).data.recipients.find((o: any) => o.id === p[0]);
  if (!r) throw err(404, 'RECIPIENT_NOT_FOUND', 'That recipient could not be found');
  return r;
});
route('GET', '/donations', async ({ s, q }) => {
  const x = need(s);
  if (x.role === 'recipient') throw err(403, 'FORBIDDEN', 'Not allowed');
  const snap = (await snapshot()).data;
  let rows = snap.donations.filter((d: any) => (x.role === 'coordinator' || d.donorId === x.identityId)
    && (!q.get('status') || d.status === q.get('status')) && (!q.get('food') || d.food === q.get('food'))
    && (!q.get('donor') || d.donorId === q.get('donor')) && (!q.get('q') || d.id.includes(String(q.get('q')).toUpperCase())));
  if (q.get('sort') === 'urgency') {
    const order: string[] = (await engineOk('read.queue')).data.order.map((o: any) => o.donationId);
    const rank = new Map(order.map((id, i) => [id, i]));
    rows = rows.filter((d: any) => rank.has(d.id)).sort((a: any, b: any) => rank.get(a.id)! - rank.get(b.id)!);
  }
  return rows;
});
route('GET', '/donations/:id', async ({ s, p }) => {
  const x = need(s);
  const d = (await snapshot()).data.donations.find((o: any) => o.id === p[0]);
  if (!d) throw err(404, 'DONATION_NOT_FOUND', 'That donation could not be found');
  if (x.role === 'recipient' || (x.role === 'donor' && d.donorId !== x.identityId)) throw err(403, 'FORBIDDEN', 'Not your donation');
  return d;
});
route('GET', '/donations/:id/match', async ({ s, p }) => { needRole(s, 'coordinator'); return (await engineOk('match.build', { donationId: p[0] })).data; });
route('GET', '/donations/:id/compare', async ({ s, p }) => { needRole(s, 'coordinator'); return (await engineOk('match.compare', { donationId: p[0] })).data; });
route('GET', '/network/field', async ({ s, q, signal }) => {
  needRole(s, 'coordinator');
  const source = Number(q.get('source'));
  if (!Number.isInteger(source)) throw bad('source must be a node number', { field: 'source' });
  return (await engineOk('net.field', { source }, signal)).data;
});
route('GET', '/network/route', async ({ s, q }) => {
  need(s);
  const from = Number(q.get('from')), to = Number(q.get('to'));
  if (!Number.isInteger(from) || !Number.isInteger(to)) throw bad('from and to must be node numbers');
  return (await engineOk('net.route', { from, to })).data;
});
async function visibleOffers(x: Sess) {
  const snap = (await snapshot()).data;
  const donorOf = new Map<string, string>(snap.donations.map((d: any) => [d.id, d.donorId]));
  return { snap, offers: snap.offers.filter((o: any) => x.role === 'coordinator' || (x.role === 'donor' ? donorOf.get(o.donationId) === x.identityId : o.recipientId === x.identityId)) };
}
route('GET', '/offers', async ({ s, q }) => {
  const x = need(s);
  const { snap, offers } = await visibleOffers(x);
  return offers.map((o: any) => withContext(snap, o)).filter((o: any) => (!q.get('status') || o.status === q.get('status')) && (!q.get('donation') || o.donationId === q.get('donation')) && (!q.get('recipient') || o.recipientId === q.get('recipient')));
});
route('GET', '/offers/:id/check', async ({ s, p }) => {
  const x = need(s);
  if (x.role === 'donor') throw err(403, 'FORBIDDEN', 'Not allowed');
  if (x.role === 'recipient') {
    const o = (await snapshot()).data.offers.find((y: any) => y.id === p[0]);
    if (o && o.recipientId !== x.identityId) throw err(403, 'FORBIDDEN', 'Not your offer');
  }
  return (await engineOk('offer.check', { offerId: p[0] })).data;
});
route('GET', '/transactions', async ({ s, q }) => {
  const x = need(s);
  const snap = (await snapshot()).data;
  const donorOf = new Map<string, string>(snap.donations.map((d: any) => [d.id, d.donorId]));
  return snap.transactions.filter((t: any) => (x.role === 'coordinator' || (x.role === 'donor' ? donorOf.get(t.donationId) === x.identityId : t.recipientId === x.identityId))
    && (!q.get('donation') || t.donationId === q.get('donation')) && (!q.get('recipient') || t.recipientId === q.get('recipient'))
    && (!q.get('donor') || donorOf.get(t.donationId) === q.get('donor')));
});

route('POST', '/match/preview', async ({ s, body, signal }) => {
  const x = needRole(s, 'coordinator', 'donor');
  const b = bodyObject(body, ['donorId', 'node', 'food', 'quantity', 'usableMinutes'], ['food', 'quantity', 'usableMinutes']);
  const args: Record<string, unknown> = { food: vFood(b.food), quantity: vInt(b.quantity, 'quantity', 1, MAX_QTY), usableMinutes: vInt(b.usableMinutes, 'usableMinutes', 1, MAX_MIN) };
  if (x.role === 'donor') {
    if (b.node !== undefined || (b.donorId !== undefined && b.donorId !== x.identityId)) throw err(403, 'FORBIDDEN', 'A donor can only preview for its own location');
    args.donorId = x.identityId;
  } else if (b.donorId !== undefined) args.donorId = vId(b.donorId, 'donorId');
  else if (b.node !== undefined) args.node = vInt(b.node, 'node', 0, 1_000_000);
  else throw bad('Give donorId or node');
  const r = await engineOk('match.preview', args, signal);
  if (x.role === 'coordinator') return r.data;
  const byReason: Record<string, number> = {};
  for (const y of r.data.rejected) byReason[y.reason] = (byReason[y.reason] || 0) + 1;
  return {
    feasibleCount: r.data.candidates.length,
    bestTravelMinutes: r.data.candidates.length ? Math.min(...r.data.candidates.map((c: any) => c.travelMinutes)) : null,
    byReason,
  };
});
route('POST', '/donations', async ({ s, body }) => {
  const x = needRole(s, 'coordinator', 'donor');
  const b = bodyObject(body, ['donorId', 'food', 'quantity', 'usableMinutes'], ['food', 'quantity', 'usableMinutes']);
  let donorId: string;
  if (x.role === 'donor') {
    if (b.donorId !== undefined && b.donorId !== x.identityId) throw err(403, 'FORBIDDEN', 'A donor can only add donations for itself');
    donorId = x.identityId!;
  } else donorId = vId(b.donorId, 'donorId');
  return mutate(x, 'donation.create', { donorId, food: vFood(b.food), quantity: vInt(b.quantity, 'quantity', 1, MAX_QTY), usableMinutes: vInt(b.usableMinutes, 'usableMinutes', 1, MAX_MIN) });
});
route('POST', '/donations/with-new-donor', async ({ s, body }) => {
  const x = needRole(s, 'coordinator');
  const b = bodyObject(body, ['donorName', 'donorType', 'node', 'food', 'quantity', 'usableMinutes'], ['donorName', 'donorType', 'node', 'food', 'quantity', 'usableMinutes']);
  return mutate(x, 'donation.createWithNewDonor', {
    donorName: vName(b.donorName, 'donorName'), donorType: vType(b.donorType, 'donorType'), node: vInt(b.node, 'node', 0, 1_000_000),
    food: vFood(b.food), quantity: vInt(b.quantity, 'quantity', 1, MAX_QTY), usableMinutes: vInt(b.usableMinutes, 'usableMinutes', 1, MAX_MIN),
  });
});
route('POST', '/donations/:id/close', async ({ s, p, body }) => {
  const x = needRole(s, 'coordinator');
  const b = bodyObject(body, ['confirm']);
  if (b.confirm !== true) throw bad('Closing a donation needs {"confirm":true}', { field: 'confirm' });
  return mutate(x, 'donation.close', { donationId: vId(p[0], 'id') });
});
route('POST', '/offers', async ({ s, body }) => {
  const x = needRole(s, 'coordinator');
  const b = bodyObject(body, ['donationId', 'recipientId', 'quantity'], ['donationId', 'recipientId', 'quantity']);
  const donationId = vId(b.donationId, 'donationId'), recipientId = vId(b.recipientId, 'recipientId');
  // Workflow guard (web-only, see CONTRACT_DEVIATIONS): one waiting offer per donation + recipient. Checked inside the
  // cross-tab write lock on the current data, so two tabs or a double click cannot both pass it.
  const noDuplicate = (snap: any) => {
    const dup = snap.offers.find((o: any) => o.status === 'PENDING' && o.donationId === donationId && o.recipientId === recipientId);
    return dup ? { code: 'DUPLICATE_PENDING_OFFER', message: `${dup.id} is already waiting for this recipient's answer on ${donationId}. Withdraw it first or wait for the answer.`, details: { offerId: dup.id, donationId, recipientId } } : null;
  };
  return mutate(x, 'offer.send', { donationId, recipientId, quantity: vInt(b.quantity, 'quantity', 0, MAX_QTY) }, noDuplicate);
});
// Wall-clock activity (local-only endpoint): timestamps the engine does not keep, filtered like /offers.
route('GET', '/activity', async ({ s }) => {
  const x = need(s);
  const { snap, offers } = await visibleOffers(x);
  const a = eng!.activity;
  const offerIds = new Set<string>(offers.map((o: any) => o.id));
  const donorOf = new Map<string, string>(snap.donations.map((d: any) => [d.id, d.donorId]));
  const txnIds = new Set<string>(snap.transactions.filter((t: any) => x.role === 'coordinator' || (x.role === 'donor' ? donorOf.get(t.donationId) === x.identityId : t.recipientId === x.identityId)).map((t: any) => t.id));
  const pick = <T,>(rec: Record<string, T>, keep: (id: string) => boolean) => Object.fromEntries(Object.entries(rec).filter(([id]) => keep(id)));
  return {
    now: Date.now(),
    offers: pick(a.offers, (id) => offerIds.has(id)),
    deliveries: pick(a.deliveries, (id) => txnIds.has(id)),
    donations: x.role === 'recipient' ? {} : pick(a.donations, (id) => x.role === 'coordinator' || donorOf.get(id) === x.identityId),
  };
});
async function ownOffer(s: Sess | null, id: string): Promise<Sess> {
  const x = needRole(s, 'recipient');
  vId(id, 'id');
  const o = (await snapshot()).data.offers.find((y: any) => y.id === id);
  if (!o) throw err(404, 'OFFER_NOT_FOUND', 'That offer could not be found');
  if (o.recipientId !== x.identityId) throw err(403, 'FORBIDDEN', 'This offer is for another recipient');
  return x;
}
route('POST', '/offers/:id/accept', async ({ s, p, body }) => { const x = await ownOffer(s, p[0]); bodyObject(body, []); return mutate(x, 'offer.accept', { offerId: p[0] }); });
route('POST', '/offers/:id/decline', async ({ s, p, body }) => {
  const x = await ownOffer(s, p[0]);
  const b = bodyObject(body, ['confirm']);
  if (b.confirm !== true) throw bad('Declining an offer needs {"confirm":true}', { field: 'confirm' });
  return mutate(x, 'offer.decline', { offerId: p[0] });
});
route('POST', '/offers/:id/withdraw', async ({ s, p, body }) => {
  const x = needRole(s, 'coordinator');
  const b = bodyObject(body, ['confirm']);
  if (b.confirm !== true) throw bad('Withdrawing an offer needs {"confirm":true}', { field: 'confirm' });
  return mutate(x, 'offer.withdraw', { offerId: vId(p[0], 'id') });
});
route('POST', '/donors', async ({ s, body }) => {
  const x = needRole(s, 'coordinator');
  const b = bodyObject(body, ['name', 'type', 'node'], ['name', 'type', 'node']);
  return mutate(x, 'donor.create', { name: vName(b.name, 'name'), type: vType(b.type, 'type'), node: vInt(b.node, 'node', 0, 1_000_000) });
});
route('POST', '/recipients', async ({ s, body }) => {
  const x = needRole(s, 'coordinator');
  const b = bodyObject(body, ['name', 'type', 'foods', 'capacity', 'accepting', 'pickup', 'node'], ['name', 'type', 'foods', 'capacity', 'accepting', 'pickup', 'node']);
  if (!Array.isArray(b.foods) || b.foods.length < 1) throw err(422, 'NO_FOOD_TYPES', 'Please choose at least one kind of food');
  return mutate(x, 'recipient.create', {
    name: vName(b.name, 'name'), type: vType(b.type, 'type'), foods: b.foods.map((f: unknown) => vFood(f, 'foods')), capacity: vInt(b.capacity, 'capacity', 0, MAX_MIN),
    accepting: vBool(b.accepting, 'accepting'), pickup: vBool(b.pickup, 'pickup'), node: vInt(b.node, 'node', 0, 1_000_000),
  });
});
route('PATCH', '/recipients/:id', async ({ s, p, body }) => {
  const x = needRole(s, 'coordinator', 'recipient');
  const id = vId(p[0], 'id');
  if (x.role === 'recipient' && x.identityId !== id) throw err(403, 'FORBIDDEN', 'You can only change your own details');
  const b = bodyObject(body, ['capacity', 'accepting', 'pickup']);
  const args: Record<string, unknown> = { recipientId: id };
  if (b.capacity !== undefined) args.capacity = vInt(b.capacity, 'capacity', 0, MAX_MIN);
  if (b.accepting !== undefined) args.accepting = vBool(b.accepting, 'accepting');
  if (b.pickup !== undefined) args.pickup = vBool(b.pickup, 'pickup');
  if (Object.keys(args).length === 1) throw bad('Give at least one of capacity, accepting or pickup');
  return mutate(x, 'recipient.update', args);
});

// ---------------------------------------------------------------------------------------------- entry points
const PREFIX = '/api/v1';
// Idempotency-Key (per tab): a repeated non-GET with the same key gets the first answer (or waits for it) instead of
// running again. The UI reuses one key per dialog opening, so a double click cannot send twice.
const idem = new Map<string, Promise<unknown>>();
export function localRequest<T>(method: string, path: string, body?: unknown, signal?: AbortSignal, idemKey?: string): Promise<T> {
  if (!idemKey || method === 'GET') return run<T>(method, path, body, signal);
  const k = `${method} ${path} ${idemKey}`;
  const hit = idem.get(k);
  if (hit) return hit as Promise<T>;
  const p = run<T>(method, path, body, signal);
  idem.set(k, p);
  if (idem.size > 100) idem.delete(idem.keys().next().value!);
  p.catch(() => { if (idem.get(k) === p) idem.delete(k); }); // a refused request may be retried with the same key
  return p;
}
async function run<T>(method: string, path: string, body?: unknown, signal?: AbortSignal): Promise<T> {
  try { eng = await getEngine(); }
  catch (e) { throw err(503, 'ENGINE_UNAVAILABLE', `The in-browser engine could not start: ${(e as Error).message}`); }
  if (signal?.aborted) throw new DOMException('Aborted', 'AbortError');
  const url = new URL(PREFIX + path, 'http://local');
  const p = url.pathname.slice(PREFIX.length) || '/';
  // a JSON round trip keeps the semantics of the HTTP body (undefined fields dropped, no shared references)
  const b = body === undefined ? undefined : JSON.parse(JSON.stringify(body));
  for (const r of routes) {
    if (r.method !== method) continue;
    const m = r.re.exec(p);
    if (!m) continue;
    const out = await r.h({ s: currentSession(), p: m.slice(1).map(decodeURIComponent), q: url.searchParams, body: b, signal });
    if (signal?.aborted) throw new DOMException('Aborted', 'AbortError');
    // hand the UI its own copy so it can never mutate the cached snapshot/network
    return (out === null || out === undefined ? null : structuredCloneSafe(out)) as T;
  }
  throw err(404, 'NOT_FOUND', `No endpoint ${method} ${p}`);
}
function structuredCloneSafe<T>(v: T): T {
  try { return structuredClone(v); } catch { return JSON.parse(JSON.stringify(v)) as T; }
}

/** Local replacement for the SSE stream: events of this tab's and other tabs' writes, filtered for this tab's session. */
export function subscribeLocal(onEvent: (e: ApiEvent) => void, onState: (s: ConnState) => void): () => void {
  let off: (() => void) | null = null, stopped = false;
  void getEngine().then((e) => {
    if (stopped) return;
    eng = e;
    off = e.on((ev) => { if (canSee(currentSession(), ev)) onEvent({ type: ev.type, data: ev.data as ApiEvent['data'] }); });
    onState('live');
  }).catch(() => onState('reconnecting'));
  return () => { stopped = true; off?.(); };
}

// ---------------------------------------------------------------------------------------------- local-only controls (UI)
export async function localStatus() { return (await getEngine()).status(); }
export async function resetLocalData() { const e = await getEngine(); await e.reset(); snapCache = null; netCache = null; }
export async function exportLocalData(): Promise<{ name: string; files: Record<string, string> }> {
  const e = await getEngine();
  return { name: e.scenario.name, files: await e.currentFiles() };
}
