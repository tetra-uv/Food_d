// Deliveries = accepted offers (engine transactions) on the simulated delivery clock. Used by the coordinator (all) and a
// recipient (its own). The clock and the "on the way" state are presentation only; the engine already recorded the delivery.
import { useMemo } from 'react';
import { MapPanel } from '../components/MapPanel';
import type { MapScene } from '../components/NetworkMap';
import { Empty, Page, Progress, Tabs, Timeline, useShowMore, type Step } from '../components/ui';
import { useDeliveryRows, type DeliveryRow } from '../lib/derive';
import { fmt } from '../lib/format';
import { useRoutes } from '../lib/routes';
import { SPEED_LABEL, clockTime, deliveryClock, useNow, type SimSpeed } from '../lib/simclock';
import { useStore } from '../store';
import { DeliveryLine } from './common';

const CLOCK_NOTE = (s: SimSpeed) => `Simulated delivery clock (${SPEED_LABEL[s].toLowerCase()}); change it in Settings.`;

function useMoving(rows: DeliveryRow[]) {
  const key = rows.map((r) => r.t.id).join(',');
  const pairs = useMemo(() => rows.flatMap((r) => (r.donorNode !== undefined && r.recipientNode !== undefined ? [[r.donorNode, r.recipientNode] as [number, number]] : [])), [key]); // eslint-disable-line react-hooks/exhaustive-deps
  const routes = useRoutes(pairs);
  return useMemo(() => rows.flatMap((r) => {
    const rt = r.donorNode !== undefined && r.recipientNode !== undefined ? routes.get(`${r.donorNode}-${r.recipientNode}`) : undefined;
    return rt && r.startAt ? [{ id: r.t.id, route: rt, startAt: r.startAt, travel: r.t.travelMinutes }] : [];
  }), [key, routes]); // eslint-disable-line react-hooks/exhaustive-deps
}

export default function Deliveries({ view, role }: { view: 'active' | 'done'; role: 'coordinator' | 'recipient' }) {
  const loaded = useStore((s) => s.loadedOnce);
  const speed = useStore((s) => s.simSpeed);
  const rows = useDeliveryRows();
  const now = useNow(1000);
  const active = rows.filter((r) => deliveryClock(r.startAt, r.t.travelMinutes, now, speed).phase === 'onway');
  const done = rows.filter((r) => deliveryClock(r.startAt, r.t.travelMinutes, now, speed).phase !== 'onway');
  const list = view === 'active' ? active : done;
  const { shown, more } = useShowMore(list, 30);
  const moving = useMoving(role === 'coordinator' ? active : []);
  const scene = useMemo<MapScene>(() => ({ deliveries: moving }), [moving]);
  const base = role === 'coordinator' ? '/c/deliveries' : '/r/deliveries';
  const words = role === 'coordinator' ? { active: 'On the way', done: 'Delivered' } : { active: 'On the way to you', done: 'Received' };
  const body = (
    <>
      <Tabs current={view === 'active' ? base : `${base}/done`} tabs={[{ href: base, label: words.active, count: active.length }, { href: `${base}/done`, label: words.done, count: done.length }]} />
      {!loaded ? <div className="skeleton" style={{ height: 200 }} /> : list.length === 0 ? (
        <Empty title={view === 'active' ? 'Nothing is on the way right now.' : 'Nothing delivered yet.'}>
          {role === 'coordinator' ? 'A delivery starts when a recipient accepts an offer.' : <a href="#/r">See offers waiting for your answer →</a>}
        </Empty>
      ) : (
        <>
          <ul className="rows">{shown.map((r) => <DeliveryLine key={r.t.id} r={r} now={now} speed={speed} perspective={role === 'recipient' ? 'recipient' : undefined} href={role === 'coordinator' ? `#/c/deliveries/${r.t.id}` : undefined} />)}</ul>
          {more}
        </>
      )}
    </>
  );
  return (
    <Page title="Deliveries" summary={CLOCK_NOTE(speed)}
      aside={role === 'coordinator' ? <MapPanel scene={scene} label="Deliveries on the way, moving along their road route" /> : undefined}>
      {body}
    </Page>
  );
}

export function deliverySteps(r: DeliveryRow, now: number, speed: SimSpeed): Step[] {
  const c = deliveryClock(r.startAt, r.t.travelMinutes, now, speed);
  const t = (x?: number | null) => (x ? clockTime(x) : undefined);
  return [
    { label: 'Offered', at: t(r.sentAt) ?? (c.phase === 'earlier' ? 'earlier' : undefined), state: 'done' },
    { label: 'Accepted', at: t(r.offer?.answeredAt ?? r.startAt), state: 'done' },
    { label: 'On the way', at: c.phase === 'onway' ? `${c.leftMin} min left` : t(r.startAt), state: c.phase === 'onway' ? 'now' : 'done' },
    { label: 'Delivered', at: c.phase === 'delivered' ? t(c.doneAt) : c.phase === 'earlier' ? 'earlier' : `about ${t(c.doneAt)}`, state: c.phase === 'onway' ? 'todo' : 'done' },
  ];
}

export function DeliveryDetail({ txnId }: { txnId: string }) {
  const loaded = useStore((s) => s.loadedOnce);
  const speed = useStore((s) => s.simSpeed);
  const rows = useDeliveryRows();
  const now = useNow(1000);
  const r = rows.find((x) => x.t.id === txnId);
  const one = useMemo(() => (r ? [r] : []), [r]);
  const c = r ? deliveryClock(r.startAt, r.t.travelMinutes, now, speed) : null;
  const moving = useMoving(c?.phase === 'onway' ? one : []);
  const pairs = useMemo<[number, number][]>(() => (r && r.donorNode !== undefined && r.recipientNode !== undefined ? [[r.donorNode, r.recipientNode]] : []), [r]);
  const route = useRoutes(pairs);
  const scene = useMemo<MapScene>(() => {
    const rt = r && pairs.length ? route.get(`${pairs[0][0]}-${pairs[0][1]}`) : undefined;
    return { source: r?.donorNode ?? null, selected: r?.recipientNode ?? null, deliveries: moving, routes: rt && !moving.length ? [{ id: 'r', route: rt, kind: 'selected' }] : [], focus: pairs[0] ?? [] };
  }, [r, pairs, route, moving]);
  if (!loaded) return <Page title="Delivery"><div className="skeleton" style={{ height: 200 }} /></Page>;
  if (!r || !c) return <Page title="Delivery not found" crumb={{ href: '#/c/deliveries', label: 'Deliveries' }}><Empty title={`There is no delivery ${txnId}.`} /></Page>;
  return (
    <Page crumb={{ href: c.phase === 'onway' ? '#/c/deliveries' : '#/c/deliveries/done', label: 'Deliveries' }} title={`Delivery ${r.t.id}`}
      summary={<>{fmt(r.t.quantity)} {r.food ? r.food.toLowerCase() : ''} portions · {r.donorName} → {r.recipientName} · {r.t.travelMinutes} min drive</>}
      aside={<MapPanel scene={scene} label="Route of this delivery" />}>
      <Timeline label="Delivery timeline" steps={deliverySteps(r, now, speed)} />
      {c.phase === 'onway' && <p className="eta"><Progress frac={c.frac} label="Delivery progress" /> <span className="num">{c.leftMin} min left</span> · arrives about {clockTime(c.doneAt!)}</p>}
      <dl className="facts wide">
        <div><dt>Donation</dt><dd><a href={`#/c/donations/${r.t.donationId}`}>{r.t.donationId}</a></dd></div>
        {r.offer && <div><dt>Offer</dt><dd><a href={`#/c/offers/${r.offer.o.id}`}>{r.offer.o.id}</a></dd></div>}
        <div><dt>Recorded</dt><dd>by the engine as transaction {r.t.id} when the offer was accepted</dd></div>
      </dl>
      <p className="small muted">{CLOCK_NOTE(speed)} {c.phase === 'earlier' ? 'This delivery happened before the activity log started, so it has no times.' : ''}</p>
    </Page>
  );
}
