// Route geometry for drawing (donor -> recipient), asked from the engine once per pair and cached for this tab.
import { useEffect, useState } from 'react';
import { api } from '../api/client';
import type { RouteResult } from '../api/types';

const cache = new Map<string, Promise<RouteResult | null>>();
export function routeOf(from: number, to: number): Promise<RouteResult | null> {
  const k = `${from}-${to}`;
  let p = cache.get(k);
  if (!p) { p = api.route(from, to).then((r) => (r.reachable ? r : null)).catch(() => null); cache.set(k, p); }
  return p;
}

/** Routes for a list of [from,to] pairs; the map entry is missing until it arrives (or when there is no road). */
export function useRoutes(pairs: [number, number][]): Map<string, RouteResult> {
  const key = pairs.map(([a, b]) => `${a}-${b}`).sort().join('|');
  const [got, setGot] = useState<Map<string, RouteResult>>(new Map());
  useEffect(() => {
    let on = true;
    const ks = key ? key.split('|') : [];
    void Promise.all(ks.map(async (k) => { const [a, b] = k.split('-').map(Number); return [k, await routeOf(a, b)] as const; })).then((res) => {
      if (!on) return;
      const m = new Map<string, RouteResult>();
      for (const [k, r] of res) if (r) m.set(k, r);
      setGot(m);
    });
    return () => { on = false; };
  }, [key]);
  return got;
}
