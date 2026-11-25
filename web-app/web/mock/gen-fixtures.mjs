// DEV-ONLY fixture generator for the mock gateway. Run: npm run fixtures
//
// This is throw-away tooling: it reads the sample CSVs (data/scenarios/lab) and writes STATIC JSON into
// mock/fixtures/. The mock server only replays those files. The numbers it produces for DON001 are checked against the
// oracle-verified facts (gate counts 26,23,17,15,13,11 -> 2, net.field from node 0). Nothing here ships to the product.
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const lab = path.resolve(here, '../../data/scenarios/lab');
const out = path.join(here, 'fixtures');

const rows = (f) => {
  const [head, ...lines] = fs.readFileSync(path.join(lab, f), 'utf8').trim().split(/\r?\n/);
  const keys = head.split(',');
  return lines.map((l) => Object.fromEntries(l.split(',').map((v, i) => [keys[i], v])));
};

// ---------- entities ----------
const nodesCsv = rows('graph_nodes.csv');
const edgesCsv = rows('graph_edges.csv');
const donors = rows('donors.csv').map((r) => ({ id: r.donor_id, name: r.name, type: r.type, node: +r.node_id }));
const recipients = rows('recipients.csv').map((r) => ({
  id: r.recipient_id, name: r.name, type: r.type, availableCapacity: +r.available_capacity,
  acceptedTypes: r.accepted_types.split(';'), accepting: r.accepting === '1', canPickup: r.can_pickup === '1', node: +r.node_id,
}));
const txCsv = rows('transactions.csv');

const N = nodesCsv.length;
const adj = Array.from({ length: N }, () => []);
for (const e of edgesCsv) { adj[+e.u].push([+e.v, +e.minutes]); adj[+e.v].push([+e.u, +e.minutes]); }
const edges = edgesCsv.map((e) => ({ u: +e.u, v: +e.v, minutes: +e.minutes }));
const wOf = (a, b) => adj[a].find((x) => x[0] === b)[1];

// ---------- graph helpers (fixture production only) ----------
function field(src) {
  const dist = Array(N).fill(Infinity), prev = Array(N).fill(-1), done = Array(N).fill(false), order = [];
  dist[src] = 0;
  for (;;) {
    let b = -1;
    for (let i = 0; i < N; i++) if (!done[i] && dist[i] < Infinity && (b < 0 || dist[i] < dist[b])) b = i;
    if (b < 0) break;
    done[b] = true; order.push(b);
    for (const [v, w] of adj[b]) if (dist[b] + w < dist[v]) { dist[v] = dist[b] + w; prev[v] = b; }
  }
  const ord = [...order].sort((a, b) => dist[a] - dist[b] || a - b);
  return { source: src, dist: dist.map((d) => (d === Infinity ? null : d)), prev, order: ord, reachableCount: ord.length };
}
const fields = Array.from({ length: N }, (_, i) => field(i));
function route(from, to) {
  const f = fields[from];
  if (f.dist[to] === null) return null;
  const nodes = [];
  for (let c = to; c !== -1; c = f.prev[c]) nodes.unshift(c);
  const legs = [];
  for (let i = 0; i + 1 < nodes.length; i++) legs.push({ u: nodes[i], v: nodes[i + 1], minutes: wOf(nodes[i], nodes[i + 1]) });
  return { nodes, legs };
}

// components
const comp = Array(N).fill(-1);
const components = [];
for (let s = 0; s < N; s++) {
  if (comp[s] >= 0) continue;
  const id = components.length, st = [s], ids = [];
  comp[s] = id;
  while (st.length) { const x = st.pop(); ids.push(x); for (const [v] of adj[x]) if (comp[v] < 0) { comp[v] = id; st.push(v); } }
  ids.sort((a, b) => a - b);
  components.push({ id, nodeIds: ids });
}
for (const c of components) {
  c.hasDonor = donors.some((d) => c.nodeIds.includes(d.node));
  c.hasRecipient = recipients.some((r) => c.nodeIds.includes(r.node));
}
const network = {
  nodes: nodesCsv.map((n) => ({
    id: +n.node_id, label: n.label, xKm: +n.x_km, yKm: +n.y_km, componentId: comp[+n.node_id],
    donors: donors.filter((d) => d.node === +n.node_id).map((d) => d.id),
    recipients: recipients.filter((r) => r.node === +n.node_id).map((r) => r.id),
  })),
  edges, components,
};
const label = (id) => nodesCsv[id].label;

// ---------- donations ----------
const donationsBase = rows('donations.csv').map((r) => ({
  id: r.donation_id, donorId: r.donor_id, food: r.food_type, original: +r.original_quantity, remaining: +r.remaining_quantity,
  usableMinutes: +r.usable_minutes, status: r.status, pendingOffered: 0, effectiveRemaining: +r.remaining_quantity,
}));
const transactions = txCsv.map((r) => ({ id: r.transaction_id, donationId: r.donation_id, recipientId: r.recipient_id, quantity: +r.quantity, travelMinutes: +r.travel_minutes }));

// ---------- match (mock-only mirror of the verified rules, used to PRODUCE static fixtures) ----------
const REASON_TEXT = {
  NOT_ACCEPTING: () => 'is not accepting food at the moment',
  FOOD_INCOMPATIBLE: (f) => `does not take ${f.toLowerCase()} food`,
  NO_CAPACITY: () => 'has no room left',
  NO_PICKUP: () => 'cannot collect the food',
  UNREACHABLE: () => 'has no road from the donor',
  TOO_FAR: (f, t, u) => `needs ${t} minutes but the food stays usable for ${u}`,
};
const GATES = ['NOT_ACCEPTING', 'FOOD_INCOMPATIBLE', 'NO_CAPACITY', 'NO_PICKUP', 'UNREACHABLE', 'TOO_FAR'];

function buildMatch({ id, food, effective, remaining, original, status, pendingOffered, usable, donorId, node, hypothetical }) {
  const f = fields[node];
  const rejected = [], cands = [];
  for (const r of recipients) {
    let reason = null;
    const dist = f.dist[r.node];
    if (!r.accepting) reason = 'NOT_ACCEPTING';
    else if (!r.acceptedTypes.includes(food)) reason = 'FOOD_INCOMPATIBLE';
    else if (r.availableCapacity <= 0) reason = 'NO_CAPACITY';
    else if (!r.canPickup) reason = 'NO_PICKUP';
    else if (dist === null) reason = 'UNREACHABLE';
    else if (dist > usable) reason = 'TOO_FAR';
    if (reason) {
      const e = {
        recipientId: r.id, reason, gate: GATES.indexOf(reason) + 1,
        text: `${r.name} ${REASON_TEXT[reason](food, dist, usable)}`,
        travelMinutes: dist, reachable: dist !== null, route: dist === null ? null : route(node, r.node),
      };
      if (reason === 'TOO_FAR') e.overByMinutes = dist - usable;
      rejected.push(e);
    } else {
      const accept = Math.min(effective, r.availableCapacity);
      cands.push({ r, accept, travel: dist, unused: r.availableCapacity - accept });
    }
  }
  cands.sort((a, b) => b.accept - a.accept || a.travel - b.travel || a.unused - b.unused || a.r.id.localeCompare(b.r.id));
  const candidates = cands.map((c, i) => {
    let sep = null;
    if (i > 0) {
      const p = cands[i - 1];
      sep = p.accept !== c.accept ? 'ACCEPT' : p.travel !== c.travel ? 'TRAVEL' : p.unused !== c.unused ? 'UNUSED' : 'ID';
    }
    const rt = route(node, c.r.node);
    return {
      rank: i + 1, recipientId: c.r.id, accept: c.accept, travelMinutes: c.travel, unusedCapacity: c.unused,
      marginMinutes: usable - c.travel, onBoundary: usable === c.travel, separatedFromPrevious: sep, route: rt,
      routeText: rt.nodes.map(label).join(' -> '),
    };
  });
  let entering = recipients.length;
  const gates = GATES.map((reason, i) => {
    const rej = rejected.filter((x) => x.reason === reason).length;
    const g = { gate: i + 1, reason, entering, rejected: rej };
    entering -= rej;
    return g;
  });
  return {
    donation: { id, food, original, remaining, effectiveRemaining: effective, pendingOffered, usableMinutes: usable, status, donorId, donorNode: node, hypothetical: !!hypothetical },
    totals: { recipients: recipients.length, feasible: candidates.length, rejected: rejected.length, declinedEarlier: 0 },
    candidates, rejected: rejected.sort((a, b) => a.gate - b.gate || a.recipientId.localeCompare(b.recipientId)), declined: [], gates, survivors: candidates.length,
    rankingKeys: ['ACCEPT_DESC', 'TRAVEL_ASC', 'UNUSED_ASC', 'ID_ASC'],
  };
}

const donorNode = (id) => donors.find((d) => d.id === id).node;
const matchOf = (d, effective, extra = {}) => buildMatch({
  id: d.id, food: d.food, effective, remaining: d.remaining, original: d.original, status: d.status,
  pendingOffered: d.remaining - effective, usable: d.usableMinutes, donorId: d.donorId, node: donorNode(d.donorId), ...extra,
});

// ---------- seed offers (mock only) ----------
const seedOffers = [
  { id: 'OFF001', donationId: 'DON005', recipientId: 'R013', quantity: 20, status: 'PENDING', txnId: null },
  { id: 'OFF002', donationId: 'DON021', recipientId: 'R012', quantity: 30, status: 'DECLINED', txnId: null },
  { id: 'OFF003', donationId: 'DON009', recipientId: 'R010', quantity: 30, status: 'WITHDRAWN', txnId: null },
].map((o) => {
  const d = donationsBase.find((x) => x.id === o.donationId);
  const r = recipients.find((x) => x.id === o.recipientId);
  return { ...o, travelMinutes: fields[donorNode(d.donorId)].dist[r.node] };
});
const donations = donationsBase.map((d) => {
  const pend = seedOffers.filter((o) => o.donationId === d.id && o.status === 'PENDING').reduce((s, o) => s + o.quantity, 0);
  return { ...d, pendingOffered: pend, effectiveRemaining: d.remaining - pend };
});
const status = (d) => (['PENDING', 'PARTIAL'].includes(d.status) ? d : null);

// ---------- matches ----------
const matches = {};
for (const d of donations) {
  if (!status(d)) continue;
  matches[d.id] = matchOf(d, d.effectiveRemaining);
}
// extra variants a demo flow reaches: partial offers on DON001 (effective 20 after offering 40; 40 after offering 20)
const d1 = donations.find((d) => d.id === 'DON001');
for (const eff of [20, 40]) matches[`DON001@${eff}`] = matchOf(d1, eff);
// fresh DON005 (seed offer withdrawn/declined -> effective 30) so withdraw/refresh looks sane
const d5 = donations.find((d) => d.id === 'DON005');
matches['DON005@30'] = matchOf({ ...d5, remaining: 30 }, 30);

// ---------- oracle assertions ----------
const m1 = matches.DON001;
const g = m1.gates.map((x) => x.entering).concat(m1.survivors).join(',');
const expectGates = '26,23,17,15,13,11,2';
const expectDist = [0, 24, 8, 20, 6, 25, 15, 22, 12, 36, 14, 38, 30, 42, 36, 31, null, null, null, null];
const ok = {
  gates: g === expectGates,
  dist: JSON.stringify(fields[0].dist) === JSON.stringify(expectDist),
  order: JSON.stringify(fields[0].order) === JSON.stringify([0, 4, 2, 8, 10, 6, 3, 7, 1, 5, 12, 15, 9, 14, 11, 13]),
  cands: m1.candidates.map((c) => `${c.recipientId}:${c.accept}/${c.travelMinutes}/${c.unusedCapacity}`).join() === 'R013:60/8/190,R004:60/12/20',
  rejected24: m1.rejected.length === 24,
  boundary: m1.candidates[1].onBoundary === true && m1.candidates[1].marginMinutes === 0,
  prev: JSON.stringify(fields[0].prev) === JSON.stringify([-1, 3, 0, 6, 0, 2, 4, 2, 0, 5, 4, 12, 8, 9, 10, 6, -1, -1, -1, -1]),
};
console.log('oracle checks', ok);
if (!Object.values(ok).every(Boolean)) {
  console.error('gates', g);
  process.exit(1);
}

// ---------- preview variants: COOKED, quantity 60, from node 0 (DON001-like), by usable minutes ----------
const keys = [];
for (let u = 1; u <= 60; u++) keys.push(u);
keys.push(90, 120, 240, 1000000);
const preview = {};
for (const u of keys) {
  preview[u] = buildMatch({ id: 'WHAT-IF', food: 'COOKED', effective: 60, remaining: 60, original: 60, status: 'PENDING', pendingOffered: 0, usable: u, donorId: 'DN001', node: 0, hypothetical: true });
}
console.log('preview 12 ->', preview[12].candidates.map((c) => c.recipientId).join(), '| 11 ->', preview[11].candidates.map((c) => c.recipientId).join(), '| 7 ->', preview[7].candidates.length);

// ---------- routes ----------
const routes = {};
for (let a = 0; a < N; a++) for (let b = 0; b < N; b++) {
  const r = route(a, b);
  routes[`${a}-${b}`] = r ? { ...r, minutes: fields[a].dist[b], reachable: true } : { nodes: [], legs: [], minutes: null, reachable: false };
}

const w = (name, obj) => fs.writeFileSync(path.join(out, name), JSON.stringify(obj));
fs.mkdirSync(out, { recursive: true });
w('network.json', network);
w('entities.json', { donors, recipients, donations, transactions, offers: seedOffers });
w('matches.json', matches);
w('preview.json', preview);
w('fields.json', fields);
w('routes.json', routes);
console.log('fixtures written to', out);
