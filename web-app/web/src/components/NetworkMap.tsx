// The one map component (lazy chunk with Leaflet). Base layers (roads, organisation markers) are built once per network;
// everything on top is a keyed list of items diffed by id + signature and updated imperatively, so unrelated React state
// changes never touch Leaflet. Deliveries move along their real route geometry on a requestAnimationFrame loop that is
// throttled and stops while the tab is hidden or nothing is on the way.
import { forwardRef, useCallback, useEffect, useImperativeHandle, useMemo, useRef } from 'react';
import L from 'leaflet';
import '../vendor/leaflet.css';
import type { Network, Route } from '../api/types';
import { TILE_ATTRIBUTION, TILE_URL } from '../config';
import { buildFrame, escapeHtml, insetLL, pointAt, routePath, type Frame, type LL } from './mapGeometry';
import { deliveryClock, type SimSpeed } from '../lib/simclock';
import { reduced } from '../lib/motion';

export interface OrgInfo { id: string; name: string; kind: 'donor' | 'recipient'; extra?: string }
export interface MapScene {
  /** donor of the donation in focus (square) */
  source?: number | null;
  routes?: { id: string; route: Route; kind: 'selected' | 'ghost' }[];
  /** offers waiting for an answer: dashed line donor -> recipient */
  pending?: { id: string; from: number; to: number }[];
  /** donations waiting for assignment: marker at the donor */
  waiting?: { id: string; node: number; urgent: boolean }[];
  candidates?: { node: number; rank: number }[];
  highlight?: number | null;
  selected?: number | null;
  /** deliveries on the way (simulated clock) */
  deliveries?: { id: string; route: Route; startAt: number; travel: number }[];
  /** nodes to fit the view on (re-fits when the list changes); empty = main network */
  focus?: number[];
}
export interface MapHandle { fit: () => void }
interface Props { network: Network; orgs: Map<string, OrgInfo>; scene: MapScene; speed?: SimSpeed; ariaLabel?: string; className?: string }

// warm, light, no blue: green = can take it / arriving, ink = selected (urgent markers are styled in CSS)
const INK = '#1F1B16', ROAD = '#B3AA9B', MUTED = '#8A8276', GREEN = '#2F6B3F', PAPER = '#FFFFFF', CREAM = '#FAF7F0';

interface Item { key: string; sig: string; build: () => L.Layer[] }

/** Leaflet 1.9 canvas renderer that ignores a redraw arriving after it was destroyed (a late redraw would otherwise throw
 *  "Cannot read properties of undefined (reading 'save'/'clearRect')" when a map page is left mid-update). */
function safeCanvas(): L.Renderer {
  const c = L.canvas({ padding: 0.3 }) as L.Renderer & { _redraw: () => void; _ctx?: unknown; _redrawRequest: unknown };
  const redraw = c._redraw;
  c._redraw = function (this: typeof c) { if (this._ctx) redraw.call(this); else this._redrawRequest = null; };
  return c;
}

const NetworkMap = forwardRef<MapHandle, Props>(function NetworkMap({ network, orgs, scene, speed = 'fast', ariaLabel, className }, ref) {
  const hasGeo = !!network.geo;
  const big = hasGeo && (network.nodes.length > 300 || network.edges.length > 500);
  const realMap = big || network.geo?.scenario === 'delhi-real';
  const frame = useMemo<Frame>(() => buildFrame(network, hasGeo ? 'geo' : 'schematic'), [network, hasGeo]);

  const host = useRef<HTMLDivElement>(null);
  const mapRef = useRef<L.Map | null>(null);
  const renderer = useRef<L.Renderer | undefined>(undefined);
  const overlay = useRef<L.LayerGroup | null>(null);
  const live = useRef(new Map<string, { sig: string; layers: L.Layer[] }>());
  const orgsRef = useRef(orgs);
  orgsRef.current = orgs;
  const speedRef = useRef(speed);
  speedRef.current = speed;

  const fitNodes = useCallback((ids: number[] | undefined) => {
    const map = mapRef.current;
    if (!map) return;
    const use = ids && ids.length ? ids : frame.mode === 'schematic' ? frame.allNodeIds : frame.mainNodeIds;
    const pts: LL[] = [];
    for (const id of use) { const p = frame.nodeLL(id); if (p) pts.push(p); }
    if (frame.mode === 'schematic' && !(ids && ids.length)) for (const i of frame.insets) pts.push(insetLL({ x: i.minX, y: i.minY }), insetLL({ x: i.maxX, y: i.maxY }));
    if (!pts.length) return;
    map.invalidateSize();
    if (pts.length === 1) map.setView(pts[0], frame.mode === 'geo' ? 14 : 1, { animate: false });
    else map.fitBounds(L.latLngBounds(pts), { padding: [36, 36], animate: false, maxZoom: frame.mode === 'geo' ? 15 : ids && ids.length ? 1 : 3 });
  }, [frame]);
  const focusRef = useRef(scene.focus);
  focusRef.current = scene.focus;
  useImperativeHandle(ref, () => ({ fit: () => fitNodes(focusRef.current) }), [fitNodes]);

  // ------------------------------------------------------------------ base map: once per network
  useEffect(() => {
    const el = host.current;
    if (!el) return;
    const geo = frame.mode === 'geo';
    const map = L.map(el, {
      crs: geo ? L.CRS.EPSG3857 : L.CRS.Simple, zoomControl: true, attributionControl: geo, zoomSnap: 0.25, zoomDelta: 0.5,
      minZoom: geo ? 3 : -4, maxZoom: geo ? 18 : 5, preferCanvas: big,
    });
    if (geo) {
      const osrm = network.geo?.minutesSource === 'osrm' ? ' · routing: <a href="https://project-osrm.org/" target="_blank" rel="noreferrer">OSRM</a>' : '';
      L.tileLayer(TILE_URL, { attribution: TILE_ATTRIBUTION + osrm, maxZoom: 19 }).addTo(map);
      map.attributionControl.setPrefix('<a href="https://leafletjs.com" target="_blank" rel="noreferrer">Leaflet</a>');
    }
    mapRef.current = map;
    // one canvas renderer for every vector layer on a big network; SVG (Leaflet default) on the small schematic
    renderer.current = big ? safeCanvas() : undefined;
    const rd = renderer.current;
    const base = L.layerGroup().addTo(map);
    const popupFor = (n: Network['nodes'][number]) => () => {
      const box = document.createElement('div'); box.className = 'pop';
      const h = document.createElement('strong'); h.textContent = n.label || `Junction ${n.id}`; box.appendChild(h);
      const add = (title: string, ids: string[]) => {
        if (!ids.length) return;
        const t = document.createElement('div'); t.className = 'pop-h'; t.textContent = title; box.appendChild(t);
        const ul = document.createElement('ul');
        for (const id of ids) { const o = orgsRef.current.get(id); const li = document.createElement('li'); li.textContent = o ? `${o.name}${o.extra ? ' · ' + o.extra : ''}` : id; ul.appendChild(li); }
        box.appendChild(ul);
      };
      add(n.donors.length === 1 ? 'Donor' : `Donors (${n.donors.length})`, n.donors);
      add(n.recipients.length === 1 ? 'Recipient' : `Recipients (${n.recipients.length})`, n.recipients);
      return box;
    };
    const orgNodes = network.nodes.filter((n) => n.donors.length + n.recipients.length > 0);
    let cleanupExtra = () => {};

    if (big) {
      const paths: LL[][] = [];
      for (const e of network.edges) { const p = frame.edgePath(e.u, e.v); if (p) paths.push(p); }
      L.polyline(paths, { renderer: rd, color: ROAD, weight: 1.2, opacity: 0.75, interactive: false, smoothFactor: 1.5 }).addTo(base);
      const markers: { mk: L.CircleMarker; count: number }[] = [];
      for (const n of orgNodes) {
        const p = frame.nodeLL(n.id);
        if (!p) continue;
        const d = n.donors.length, r = n.recipients.length;
        const mk = L.circleMarker(p, { renderer: rd, radius: 4, color: d ? INK : MUTED, weight: 1.3, fillColor: d && r ? CREAM : d ? INK : PAPER, fillOpacity: 1 });
        mk.bindTooltip(`${escapeHtml(n.label || 'Place ' + n.id)} · ${d + r} organisation${d + r > 1 ? 's' : ''}`, { className: 'tip', direction: 'top' });
        mk.bindPopup(popupFor(n), { className: 'cb-popup', maxWidth: 280 });
        mk.addTo(base);
        markers.push({ mk, count: d + r });
      }
      const sizeByZoom = () => {
        const z = map.getZoom(), r0 = z >= 15 ? 7 : z >= 13 ? 5.5 : z >= 11 ? 4 : 3;
        for (const m of markers) m.mk.setRadius(r0 + Math.min(3, Math.sqrt(m.count) - 1));
      };
      map.on('zoomend', sizeByZoom);
      sizeByZoom();
    } else {
      for (const i of frame.insets) {
        const pad = 1.1;
        const b = L.latLngBounds(insetLL({ x: i.minX - pad, y: i.minY - pad }), insetLL({ x: i.maxX + pad, y: i.maxY + pad }));
        L.rectangle(b, { color: MUTED, weight: 1, dashArray: '6 5', fillColor: CREAM, fillOpacity: 0.6, interactive: false }).addTo(base);
        L.marker(insetLL({ x: (i.minX + i.maxX) / 2, y: i.maxY + pad }), { interactive: false, icon: L.divIcon({ className: 'map-note', iconSize: [200, 30], iconAnchor: [100, -2], html: '<span>No road to the main network</span>' }) }).addTo(base);
      }
      for (const e of network.edges) {
        const p = frame.edgePath(e.u, e.v);
        if (p) L.polyline(p, { color: ROAD, weight: 2.5, opacity: 0.9, lineCap: 'round' }).bindTooltip(`${e.minutes} min`, { sticky: true, className: 'tip' }).addTo(base);
      }
      for (const n of network.nodes) {
        const p = frame.nodeLL(n.id);
        if (!p) continue;
        const d = n.donors.length, r = n.recipients.length, count = d + r;
        const kind = d && r ? 'hybrid' : d ? 'donor' : r ? 'recipient' : 'junction';
        const html = `<div class="nm ${kind}"><span class="nm-shape"></span>${count > 1 ? `<span class="nm-count">${count}</span>` : ''}<span class="nm-label">${escapeHtml(n.label)}</span></div>`;
        const mk = L.marker(p, { icon: L.divIcon({ className: 'nm-wrap', html, iconSize: [26, 26], iconAnchor: [13, 13] }), keyboard: count > 0, alt: `${n.label}, ${d} donors, ${r} recipients`, title: n.label });
        if (count) mk.bindPopup(popupFor(n), { className: 'cb-popup', maxWidth: 280 });
        mk.addTo(base);
      }
      const labels = () => el.classList.toggle('labels-on', map.getZoom() >= 1.5);
      map.on('zoomend', labels);
      cleanupExtra = () => map.off('zoomend', labels);
    }
    overlay.current = L.layerGroup().addTo(map);
    live.current = new Map();
    const ro = new ResizeObserver(() => map.invalidateSize());
    ro.observe(el);
    fitNodes(focusRef.current);
    if (import.meta.env.DEV) (window as unknown as Record<string, unknown>).__cbMap = map;
    return () => {
      cleanupExtra(); ro.disconnect();
      // Remove vector layers while the canvas renderer is still alive: removing them during map.remove() (after the
      // renderer was destroyed) schedules a canvas redraw on a deleted context (Leaflet 1.9 "reading 'save'" error).
      overlay.current?.clearLayers(); base.clearLayers();
      map.remove(); mapRef.current = null; overlay.current = null; live.current = new Map();
    };
  }, [frame, network, big, fitNodes]);

  // ------------------------------------------------------------------ overlay items, diffed by key + signature
  const items = useMemo<Item[]>(() => {
    const out: Item[] = [];
    const rd = () => renderer.current;
    const div = (cls: string, html: string, size: [number, number], anchor: [number, number]) => L.divIcon({ className: cls, html, iconSize: size, iconAnchor: anchor });
    for (const w of scene.waiting ?? []) {
      out.push({ key: 'w:' + w.id, sig: `${w.node}:${w.urgent}`, build: () => {
        const p = frame.nodeLL(w.node); if (!p) return [];
        return [L.marker(p, { interactive: false, keyboard: false, icon: div(`wait-mk${w.urgent ? ' urgent' : ''}`, '<span></span>', [12, 12], [6, 6]) })];
      } });
    }
    for (const po of scene.pending ?? []) {
      out.push({ key: 'p:' + po.id, sig: `${po.from}-${po.to}`, build: () => {
        const a = frame.nodeLL(po.from), b = frame.nodeLL(po.to); if (!a || !b) return [];
        return [L.polyline([a, b], { renderer: rd(), color: INK, weight: 1.4, dashArray: '5 6', opacity: 0.65, interactive: false })];
      } });
    }
    const rts = [...(scene.routes ?? [])].sort((a, b) => (a.kind === 'selected' ? 1 : 0) - (b.kind === 'selected' ? 1 : 0));
    for (const r of rts) {
      out.push({ key: `r:${r.kind}:${r.id}`, sig: r.route.nodes.join('.'), build: () => {
        const path = routePath(frame, r.route); if (path.length < 2) return [];
        return r.kind === 'selected'
          ? [L.polyline(path, { renderer: rd(), color: PAPER, weight: 7, opacity: 0.9, interactive: false }), L.polyline(path, { renderer: rd(), color: INK, weight: 3.2, interactive: false })]
          : [L.polyline(path, { renderer: rd(), color: MUTED, weight: 2.2, opacity: 0.55, dashArray: '2 6', interactive: false })];
      } });
    }
    for (const c of scene.candidates ?? []) {
      out.push({ key: 'c:' + c.node, sig: String(c.rank), build: () => {
        const p = frame.nodeLL(c.node); if (!p) return [];
        return [L.circleMarker(p, { renderer: rd(), radius: big ? 9 : 16, color: GREEN, weight: 2, fill: false, interactive: false }),
          L.marker(p, { interactive: false, keyboard: false, icon: div('rank-chip', `<span>${c.rank}</span>`, [18, 18], [24, 26]) })];
      } });
    }
    for (const [k, n, r] of [['h', scene.highlight, big ? 13 : 21], ['s', scene.selected, big ? 14 : 23]] as const) {
      if (n === null || n === undefined) continue;
      out.push({ key: k, sig: String(n), build: () => { const p = frame.nodeLL(n); return p ? [L.circleMarker(p, { renderer: rd(), radius: r, color: INK, weight: 2, fill: false, interactive: false })] : []; } });
    }
    if (scene.source !== null && scene.source !== undefined) {
      const n = scene.source;
      out.push({ key: 'src', sig: String(n), build: () => {
        const p = frame.nodeLL(n); if (!p) return [];
        return [L.marker(p, { interactive: false, keyboard: false, icon: div('src-sq', '<span></span>', [14, 14], [7, 7]) })];
      } });
    }
    for (const d of scene.deliveries ?? []) {
      out.push({ key: 'dv:' + d.id, sig: d.route.nodes.join('.'), build: () => {
        const path = routePath(frame, d.route); if (path.length < 2) return [];
        return [L.polyline(path, { renderer: rd(), color: GREEN, weight: 2.6, opacity: 0.55, interactive: false })];
      } });
    }
    return out;
  }, [scene.waiting, scene.pending, scene.routes, scene.candidates, scene.highlight, scene.selected, scene.source, scene.deliveries, frame, big]);

  useEffect(() => {
    const g = overlay.current;
    if (!g) return;
    const cur = live.current, seen = new Set<string>();
    for (const it of items) {
      seen.add(it.key);
      const had = cur.get(it.key);
      if (had && had.sig === it.sig) continue;
      if (had) for (const l of had.layers) g.removeLayer(l);
      const layers = it.build();
      for (const l of layers) g.addLayer(l);
      cur.set(it.key, { sig: it.sig, layers });
    }
    for (const [k, v] of cur) if (!seen.has(k)) { for (const l of v.layers) g.removeLayer(l); cur.delete(k); }
  }, [items, frame]);

  // re-fit when the focus set changes
  const focusSig = (scene.focus ?? []).join(',');
  useEffect(() => { fitNodes(scene.focus); /* eslint-disable-next-line react-hooks/exhaustive-deps */ }, [focusSig, fitNodes]);

  // ------------------------------------------------------------------ moving delivery markers (rAF, throttled, paused when hidden)
  const deliveries = scene.deliveries;
  useEffect(() => {
    const g = overlay.current;
    if (!g || !deliveries?.length) return;
    // per delivery: legs weighted by their engine minutes, so the marker moves at the route's own pace
    const tracks = deliveries.map((d) => {
      const legs: { path: LL[]; m0: number; m1: number }[] = [];
      let acc = 0;
      for (const l of d.route.legs) { const p = frame.edgePath(l.u, l.v); if (p && p.length > 1) { legs.push({ path: p, m0: acc, m1: acc + l.minutes }); } acc += l.minutes; }
      const mk = legs.length ? L.marker(legs[0].path[0], { interactive: false, keyboard: false, zIndexOffset: 500, icon: L.divIcon({ className: 'dv-mk', html: '<span></span>', iconSize: [14, 14], iconAnchor: [7, 7] }) }).addTo(g) : null;
      return { d, legs, total: acc, mk };
    });
    const place = () => {
      const now = Date.now();
      for (const t of tracks) {
        if (!t.mk) continue;
        const c = deliveryClock(t.d.startAt, t.d.travel, now, speedRef.current);
        const m = c.frac * t.total;
        const leg = t.legs.find((l) => m <= l.m1) ?? t.legs[t.legs.length - 1];
        t.mk.setLatLng(pointAt(leg.path, leg.m1 > leg.m0 ? (m - leg.m0) / (leg.m1 - leg.m0) : 1));
      }
    };
    let raf = 0, last = 0;
    const tick = (ts: number) => {
      if (ts - last >= 66) { last = ts; place(); } // ~15 updates per second is smooth enough and cheap
      raf = requestAnimationFrame(tick);
    };
    const start = () => { cancelAnimationFrame(raf); if (reduced()) { place(); return; } raf = requestAnimationFrame(tick); };
    const vis = () => { if (document.hidden) cancelAnimationFrame(raf); else start(); };
    place(); start();
    // a slow tick as well: keeps markers right where the browser starves rAF (background/occluded windows), and is the
    // only update under reduced motion (jumps every 2 s instead of gliding)
    const slow = setInterval(() => { if (!document.hidden) place(); }, reduced() ? 2000 : 1000);
    document.addEventListener('visibilitychange', vis);
    return () => { cancelAnimationFrame(raf); clearInterval(slow); document.removeEventListener('visibilitychange', vis); for (const t of tracks) if (t.mk) g.removeLayer(t.mk); };
  }, [deliveries, frame]);

  const foot = realMap ? 'Imitation of Delhi: roads and places from OpenStreetMap; names, capacities and donations are simulated.'
    : hasGeo ? 'Imitation of Delhi: approximate places; names, capacities, donations and travel minutes are simulated.'
      : 'Sample network: names, capacities, donations and travel times are simulated.';
  return (
    <div className={`mapbox ${className ?? ''}`}>
      <div className={`mapwrap ${hasGeo ? 'map-geo' : 'map-schem'}`}>
        <div ref={host} className="mapcanvas" role="region" aria-label={ariaLabel ?? 'Map'} />
        <button type="button" className="map-btn" onClick={() => fitNodes(focusRef.current)}>Re-centre</button>
      </div>
      <p className="map-foot">{foot}</p>
    </div>
  );
});

export default NetworkMap;
