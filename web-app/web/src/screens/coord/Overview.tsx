import { useMemo } from 'react';
import { MapPanel } from '../../components/MapPanel';
import type { MapScene } from '../../components/NetworkMap';
import { Empty, FoodTag, Page } from '../../components/ui';
import { useDeliveryRows, useDonationGroups, useLookups, useOfferRows } from '../../lib/derive';
import { fmt, plural } from '../../lib/format';
import { useRoutes } from '../../lib/routes';
import { ago, deliveryClock, useNow } from '../../lib/simclock';
import { useStore } from '../../store';
import { DeliveryLine, isUrgent, timeLeft } from '../common';

const TOP = 5;

export default function Overview() {
  const loaded = useStore((s) => s.loadedOnce);
  const speed = useStore((s) => s.simSpeed);
  const groups = useDonationGroups();
  const offers = useOfferRows();
  const deliveries = useDeliveryRows();
  const L = useLookups();
  const now = useNow(1000);

  const pending = useMemo(() => offers.filter((r) => r.o.status === 'PENDING').sort((a, b) => (a.sentAt ?? 0) - (b.sentAt ?? 0)), [offers]);
  const onWay = deliveries.filter((r) => deliveryClock(r.startAt, r.t.travelMinutes, now, speed).phase === 'onway');
  const onWayKey = onWay.map((r) => r.t.id).join(',');
  const pairs = useMemo(() => onWay.flatMap((r) => (r.donorNode !== undefined && r.recipientNode !== undefined ? [[r.donorNode, r.recipientNode] as [number, number]] : [])), [onWayKey]); // eslint-disable-line react-hooks/exhaustive-deps
  const routes = useRoutes(pairs);

  const waiting = useMemo(() => {
    const byNode = new Map<number, boolean>();
    for (const d of groups.assign) { const n = L.donorNode(d.donorId); if (n !== undefined) byNode.set(n, (byNode.get(n) ?? false) || isUrgent(d.usableMinutes)); }
    return [...byNode].map(([node, urgent]) => ({ id: String(node), node, urgent }));
  }, [groups.assign, L]);
  const pendingLines = useMemo(() => pending.flatMap((r) => (r.donorNode !== undefined && r.recipientNode !== undefined ? [{ id: r.o.id, from: r.donorNode, to: r.recipientNode }] : [])), [pending]);
  const moving = useMemo(() => onWay.flatMap((r) => {
    const rt = r.donorNode !== undefined && r.recipientNode !== undefined ? routes.get(`${r.donorNode}-${r.recipientNode}`) : undefined;
    return rt && r.startAt ? [{ id: r.t.id, route: rt, startAt: r.startAt, travel: r.t.travelMinutes }] : [];
  }), [onWayKey, routes]); // eslint-disable-line react-hooks/exhaustive-deps
  const scene = useMemo<MapScene>(() => ({ waiting, pending: pendingLines, deliveries: moving }), [waiting, pendingLines, moving]);

  const urgent = groups.assign.slice(0, TOP);
  const summary = loaded
    ? `${plural(groups.assign.length, 'donation')} to assign · ${plural(pending.length, 'offer')} waiting for an answer · ${plural(onWay.length, 'delivery', 'deliveries')} on the way`
    : 'Loading…';

  return (
    <Page title="Overview" summary={summary}
      aside={<>
        <MapPanel scene={scene} label="Live operations map: donors with food to assign, offers waiting (dashed) and deliveries on the way (moving)" />
        <p className="map-key small muted"><span className="k k-wait" /> food to assign <span className="k k-pend" /> offer waiting <span className="k k-dv" /> on the way (simulated clock)</p>
      </>}>
      <section className="sect" aria-labelledby="now-h">
        <h2 id="now-h">Needs you now</h2>
        {!loaded ? <div className="skeleton" style={{ height: 200 }} /> : urgent.length === 0 ? (
          <Empty title="Every donation has been assigned.">New donations appear here as donors add them.</Empty>
        ) : (
          <ul className="rows">
            {urgent.map((d) => (
              <li key={d.id}>
                <a className="row row-link" href={`#/c/donations/${d.id}`}>
                  <span className="row-main"><span className="line1"><strong className="num">{fmt(d.effectiveRemaining)}</strong> <FoodTag food={d.food} /> <span className="muted">from {L.donorName(d.donorId)}</span></span></span>
                  <span className={`row-side ${isUrgent(d.usableMinutes) ? 'tone-warn' : 'muted'}`}>{timeLeft(d.usableMinutes)}</span>
                </a>
              </li>
            ))}
          </ul>
        )}
        {groups.assign.length > TOP && <p className="more"><a href="#/c/donations">All {groups.assign.length} donations to assign →</a></p>}
      </section>

      <section className="sect" aria-labelledby="wait-h">
        <h2 id="wait-h">Offers waiting longest</h2>
        {pending.length === 0 ? <Empty title="No offer is waiting for an answer." /> : (
          <ul className="rows">
            {pending.slice(0, TOP).map((r) => (
              <li key={r.o.id}>
                <a className="row row-link" href={`#/c/offers/${r.o.id}`}>
                  <span className="row-main"><span className="line1"><strong className="num">{fmt(r.o.quantity)}</strong> <FoodTag food={r.food} /> <span className="muted">to {r.recipientName}</span></span></span>
                  <span className="row-side muted">{r.sentAt ? `waiting ${ago(r.sentAt, now)}` : 'sent earlier'}</span>
                </a>
              </li>
            ))}
          </ul>
        )}
        {pending.length > TOP && <p className="more"><a href="#/c/offers">All {pending.length} waiting offers →</a></p>}
      </section>

      {onWay.length > 0 && (
        <section className="sect" aria-labelledby="dv-h">
          <h2 id="dv-h">On the way</h2>
          <ul className="rows">{onWay.slice(0, TOP).map((r) => <DeliveryLine key={r.t.id} r={r} now={now} speed={speed} href={`#/c/deliveries/${r.t.id}`} />)}</ul>
          {onWay.length > TOP && <p className="more"><a href="#/c/deliveries">All deliveries →</a></p>}
        </section>
      )}
    </Page>
  );
}
