// Lazy wrapper around the map chunk (Leaflet is loaded only by pages that show a map). The fallback reserves the same box,
// so nothing shifts when the map arrives.
import { forwardRef, lazy, Suspense, useMemo } from 'react';
import type { MapHandle, MapScene, OrgInfo } from './NetworkMap';
import { useStore } from '../store';
import { fmt } from '../lib/format';

const NetworkMap = lazy(() => import('./NetworkMap'));

export function useOrgs(): Map<string, OrgInfo> {
  const identities = useStore((s) => s.identities);
  const recipients = useStore((s) => s.recipients);
  return useMemo(() => {
    const m = new Map<string, OrgInfo>();
    for (const d of identities?.donors ?? []) m.set(d.id, { id: d.id, name: d.name, kind: 'donor' });
    for (const r of identities?.recipients ?? []) m.set(r.id, { id: r.id, name: r.name, kind: 'recipient' });
    for (const r of recipients) m.set(r.id, { id: r.id, name: r.name, kind: 'recipient', extra: `${r.accepting ? 'accepting' : 'not accepting'}, room ${fmt(r.availableCapacity)}` });
    return m;
  }, [identities, recipients]);
}

export const MapPanel = forwardRef<MapHandle, { scene: MapScene; label: string; small?: boolean }>(function MapPanel({ scene, label, small }, ref) {
  const network = useStore((s) => s.network);
  const speed = useStore((s) => s.simSpeed);
  const orgs = useOrgs();
  const cls = small ? 'map-small' : '';
  if (!network) return <div className={`mapwrap loading ${cls}`} />;
  return (
    <Suspense fallback={<div className={`mapbox ${cls}`}><div className="mapwrap loading">Loading map…</div><p className="map-foot">&nbsp;</p></div>}>
      <NetworkMap ref={ref} network={network} orgs={orgs} scene={scene} speed={speed} ariaLabel={label} className={cls} />
    </Suspense>
  );
});
