// Pure rendering math for the map (no Leaflet types): schematic layout, edge paths, partial polylines.
// Nothing here decides business results; it only places things on screen.
import type { Network, NetNode, Route } from '../api/types';

export type LL = [number, number]; // [lat, lng] (Leaflet order)
export const SCALE = 40; // schematic km -> CRS.Simple units

export interface Inset {
  componentId: number; minX: number; minY: number; maxX: number; maxY: number; // in km-ish schematic units
  trueKm: number; anchorMain: { x: number; y: number }; anchorIsland: { x: number; y: number };
}
export interface Frame {
  mode: 'geo' | 'schematic';
  nodeLL: (id: number) => LL | null;
  edgePath: (u: number, v: number) => LL[] | null; // oriented u -> v
  insets: Inset[];
  mainId: number;
  mainNodeIds: number[];
  allNodeIds: number[];
}

export function mainComponent(net: Network): number {
  let best = net.components[0];
  for (const c of net.components) if (c.nodeIds.length > best.nodeIds.length) best = c;
  return best ? best.id : 0;
}

export function buildFrame(net: Network, mode: 'geo' | 'schematic'): Frame {
  const mainId = mainComponent(net);
  const mainNodeIds = net.components.find((c) => c.id === mainId)?.nodeIds ?? [];
  const allNodeIds = net.nodes.map((n) => n.id);
  const byId = new Map<number, NetNode>(net.nodes.map((n) => [n.id, n]));

  if (mode === 'geo' && net.geo) {
    const pos = new Map<number, LL>();
    for (const n of net.geo.nodes) pos.set(n.id, [n.lat, n.lng]);
    const geom = new Map<string, LL[]>();
    for (const e of net.geo.edges) {
      if (e.geometry && e.geometry.length >= 2) geom.set(`${e.u}-${e.v}`, e.geometry.map(([lng, lat]) => [lat, lng] as LL));
    }
    return {
      mode, mainId, mainNodeIds, allNodeIds, insets: [],
      nodeLL: (id) => pos.get(id) ?? null,
      edgePath: (u, v) => {
        const a = pos.get(u), b = pos.get(v);
        if (!a || !b) return null;
        const g = geom.get(`${u}-${v}`);
        if (g) return g;
        const r = geom.get(`${v}-${u}`);
        if (r) return [...r].reverse();
        return [a, b];
      },
    };
  }

  // ----- schematic: main cluster at true x/y (y is south/down -> negate), islands as insets nearer the main cluster -----
  const pts = new Map<number, { x: number; y: number }>();
  const mainNodes = mainNodeIds.map((i) => byId.get(i)!).filter(Boolean);
  const bb = (ns: { x: number; y: number }[]) => ({
    minX: Math.min(...ns.map((n) => n.x)), maxX: Math.max(...ns.map((n) => n.x)),
    minY: Math.min(...ns.map((n) => n.y)), maxY: Math.max(...ns.map((n) => n.y)),
  });
  const mainPts = mainNodes.map((n) => ({ x: n.xKm, y: n.yKm }));
  for (const n of mainNodes) pts.set(n.id, { x: n.xKm, y: n.yKm });
  const mb = mainPts.length ? bb(mainPts) : { minX: 0, maxX: 0, minY: 0, maxY: 0 };
  const mc = { x: (mb.minX + mb.maxX) / 2, y: (mb.minY + mb.maxY) / 2 };
  let spacing = Infinity;
  for (const e of net.edges) {
    const a = byId.get(e.u), b = byId.get(e.v);
    if (a && b && a.componentId === mainId && b.componentId === mainId) spacing = Math.min(spacing, Math.hypot(a.xKm - b.xKm, a.yKm - b.yKm));
  }
  if (!isFinite(spacing) || spacing <= 0) spacing = 3.5;
  const gap = spacing * 1.2;
  const cursor: Record<string, number> = {};
  const insets: Inset[] = [];
  const islands = net.components.filter((c) => c.id !== mainId).sort((a, b) => b.nodeIds.length - a.nodeIds.length);
  for (const c of islands) {
    const ns = c.nodeIds.map((i) => byId.get(i)!).filter(Boolean);
    const ib = bb(ns.map((n) => ({ x: n.xKm, y: n.yKm })));
    const ic = { x: (ib.minX + ib.maxX) / 2, y: (ib.minY + ib.maxY) / 2 };
    const dx = ic.x - mc.x, dy = ic.y - mc.y;
    const horizontal = Math.abs(dx) >= Math.abs(dy);
    const w = ib.maxX - ib.minX, h = ib.maxY - ib.minY;
    let ox: number, oy: number; // new min corner
    if (horizontal) {
      const side = dx >= 0 ? 'R' : 'L';
      oy = Math.min(Math.max(ic.y - h / 2, mb.minY), mb.maxY - h);
      if (cursor[side] !== undefined) oy = Math.max(oy, cursor[side] + gap);
      ox = side === 'R' ? mb.maxX + gap : mb.minX - gap - w;
      cursor[side] = oy + h;
    } else {
      const side = dy >= 0 ? 'B' : 'T';
      ox = Math.min(Math.max(ic.x - w / 2, mb.minX), mb.maxX - w);
      if (cursor[side] !== undefined) ox = Math.max(ox, cursor[side] + gap);
      oy = side === 'B' ? mb.maxY + gap : mb.minY - gap - h;
      cursor[side] = ox + w;
    }
    for (const n of ns) pts.set(n.id, { x: n.xKm - ib.minX + ox, y: n.yKm - ib.minY + oy });
    // anchor points for the "no road" break line: nearest main node to the island box
    const ibx = { x: ox + w / 2, y: oy + h / 2 };
    let near = mainPts[0] ?? mc, nd = Infinity;
    for (const p of mainPts) { const d = Math.hypot(p.x - ibx.x, p.y - ibx.y); if (d < nd) { nd = d; near = p; } }
    insets.push({
      componentId: c.id, minX: ox, minY: oy, maxX: ox + w, maxY: oy + h, trueKm: Math.hypot(dx, dy),
      anchorMain: near, anchorIsland: ibx,
    });
  }
  const ll = (p: { x: number; y: number }): LL => [-p.y * SCALE, p.x * SCALE];
  return {
    mode, mainId, mainNodeIds, allNodeIds, insets,
    nodeLL: (id) => { const p = pts.get(id); return p ? ll(p) : null; },
    edgePath: (u, v) => { const a = pts.get(u), b = pts.get(v); return a && b ? [ll(a), ll(b)] : null; },
  };
}

export const insetLL = (p: { x: number; y: number }): LL => [-p.y * SCALE, p.x * SCALE];

export function routePath(frame: Frame, route: Route): LL[] {
  const out: LL[] = [];
  for (const leg of route.legs) {
    const p = frame.edgePath(leg.u, leg.v);
    if (!p) continue;
    for (let i = 0; i < p.length; i++) if (!(out.length && i === 0)) out.push(p[i]);
  }
  if (!out.length && route.nodes.length) for (const n of route.nodes) { const p = frame.nodeLL(n); if (p) out.push(p); }
  return out;
}

const seg = (a: LL, b: LL) => Math.hypot(a[0] - b[0], a[1] - b[1]);
export function pathLength(p: LL[]): number { let s = 0; for (let i = 1; i < p.length; i++) s += seg(p[i - 1], p[i]); return s; }

/** Point at fraction f (0..1) along polyline p. */
export function pointAt(p: LL[], f: number): LL {
  if (p.length === 1) return p[0];
  const total = pathLength(p), target = Math.min(Math.max(f, 0), 1) * total;
  let acc = 0;
  for (let i = 1; i < p.length; i++) {
    const l = seg(p[i - 1], p[i]);
    if (acc + l >= target || i === p.length - 1) {
      const k = l === 0 ? 0 : (target - acc) / l;
      return [p[i - 1][0] + (p[i][0] - p[i - 1][0]) * k, p[i - 1][1] + (p[i][1] - p[i - 1][1]) * k];
    }
    acc += l;
  }
  return p[p.length - 1];
}

/** Sub-polyline between fractions f0..f1 (by length). */
export function subPath(p: LL[], f0: number, f1: number): LL[] {
  if (f1 <= f0) return [];
  const total = pathLength(p), a = f0 * total, b = f1 * total;
  const out: LL[] = [pointAt(p, f0)];
  let acc = 0;
  for (let i = 1; i < p.length; i++) {
    acc += seg(p[i - 1], p[i]);
    if (acc > a && acc < b) out.push(p[i]);
  }
  out.push(pointAt(p, f1));
  return out;
}

export const smoothstep = (a: number, b: number, x: number) => { const t = Math.min(Math.max((x - a) / (b - a || 1), 0), 1); return t * t * (3 - 2 * t); };
export const escapeHtml = (s: string) => s.replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]!));
