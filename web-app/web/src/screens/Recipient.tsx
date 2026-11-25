import { memo, useEffect, useMemo, useState } from 'react';
import { api } from '../api/client';
import type { OfferCheck, Recipient as Rec } from '../api/types';
import { MapPanel } from '../components/MapPanel';
import type { MapScene } from '../components/NetworkMap';
import { ConfirmDialog, Empty, FoodTag, NumberField, Page, StatusChip, useShowMore } from '../components/ui';
import { useOfferRows, type OfferRow } from '../lib/derive';
import { fmt, isNoExpiry } from '../lib/format';
import { FOOD_META } from '../lib/meta';
import { useRoutes } from '../lib/routes';
import { ago, clockTime, useNow } from '../lib/simclock';
import { errorText, runMutation, useStore } from '../store';

function RoutePreview({ from, to }: { from: number; to: number }) {
  const pairs = useMemo<[number, number][]>(() => [[from, to]], [from, to]);
  const got = useRoutes(pairs);
  const rt = got.get(`${from}-${to}`);
  const scene = useMemo<MapScene>(() => ({ source: from, selected: to, routes: rt ? [{ id: 'r', route: rt, kind: 'selected' }] : [], focus: [from, to] }), [from, to, rt]);
  return <div className="mini-map"><MapPanel small scene={scene} label="Road route from the donor to you" /></div>;
}

const IncomingOffer = memo(function IncomingOffer({ r, room, now }: { r: OfferRow; room: number | undefined; now: number }) {
  const o = r.o, ctx = o.context ?? null;
  const [check, setCheck] = useState<OfferCheck | null>(null);
  const [showMap, setShowMap] = useState(false);
  const [busy, setBusy] = useState(false);
  const [err, setErr] = useState<string | null>(null);
  const [declining, setDeclining] = useState(false);
  const [declineErr, setDeclineErr] = useState<string | null>(null);
  useEffect(() => { let on = true; api.offerCheck(o.id).then((c) => on && setCheck(c)).catch(() => undefined); return () => { on = false; }; }, [o.id, room]);
  const margin = ctx ? ctx.usableMinutes - o.travelMinutes : null;
  const accept = async () => {
    if (busy) return;
    setBusy(true); setErr(null);
    const res = await runMutation(() => api.accept(o.id), { own: (x) => [o.id, x.data.transaction?.id], silent: true });
    setBusy(false);
    if (!res.ok) setErr(errorText(res.error));
    else useStore.getState().toast({ tone: 'ok', text: `Accepted. Delivery ${res.data.data.transaction?.id ?? ''} is on the way to you.`, action: { label: 'Follow it', run: () => { location.hash = '#/r/deliveries'; } } });
  };
  const decline = async () => {
    setBusy(true); setDeclineErr(null);
    const res = await runMutation(() => api.decline(o.id), { own: () => [o.id] });
    setBusy(false);
    if (res.ok) setDeclining(false); else setDeclineErr(errorText(res.error));
  };
  const timing = !ctx ? null : isNoExpiry(ctx.usableMinutes) ? 'the food keeps' : margin === 0 ? 'arrives exactly at the limit (allowed)'
    : margin !== null && margin < 0 ? `would arrive ${-margin} min too late` : `arrives with ${margin} min to spare`;
  return (
    <li className="row offer-row">
      <div className="row-main">
        <p className="line1"><strong className="num">{fmt(o.quantity)}</strong> {ctx ? FOOD_META[ctx.food].label.toLowerCase() : ''} portions from <strong>{r.donorName}</strong></p>
        <p className="small muted">{o.travelMinutes} min away{timing && <> · <span className={margin !== null && margin < 0 ? 'tone-danger' : ''}>{timing}</span></>}</p>
        <p className="small muted">{o.id} · {r.sentAt ? `offered ${clockTime(r.sentAt)} (${ago(r.sentAt, now)} ago)` : 'offered earlier'}</p>
        {check && !check.wouldSucceed && <p className="tone-warn small" role="status">No longer fits: {check.message ?? check.code}</p>}
        {err && <p className="form-error" role="alert">{err}</p>}
        <div className="actions">
          <button type="button" className="btn primary" disabled={busy} onClick={accept}>{busy ? 'Working…' : 'Accept'}</button>
          <button type="button" className="btn" disabled={busy} onClick={() => setDeclining(true)}>Decline…</button>
          {ctx && <button type="button" className="link-btn" aria-expanded={showMap} onClick={() => setShowMap(!showMap)}>{showMap ? 'Hide route' : 'Show route'}</button>}
        </div>
        {showMap && ctx && <RoutePreview from={ctx.donorNode} to={ctx.recipientNode} />}
      </div>
      <ConfirmDialog open={declining} title="Decline this offer?" confirmLabel="Decline offer" danger busy={busy} error={declineErr} onClose={() => setDeclining(false)} onConfirm={decline}
        body={<p>You will not be offered this donation again. The {fmt(o.quantity)} portions go back to the coordinator.</p>} />
    </li>
  );
});

function Details({ me }: { me: Rec }) {
  const [cap, setCap] = useState(me.availableCapacity);
  const [accepting, setAccepting] = useState(me.accepting);
  const [pickup, setPickup] = useState(me.canPickup);
  const [busy, setBusy] = useState(false);
  const [err, setErr] = useState<string | null>(null);
  const [off, setOff] = useState(false);
  useEffect(() => { setCap(me.availableCapacity); setAccepting(me.accepting); setPickup(me.canPickup); }, [me.availableCapacity, me.accepting, me.canPickup]);
  const save = async (body: { capacity?: number; accepting?: boolean; pickup?: boolean }) => {
    setBusy(true); setErr(null);
    const r = await runMutation(() => api.patchRecipient(me.id, body), { own: () => [me.id] });
    setBusy(false);
    if (!r.ok) setErr(errorText(r.error));
    return r.ok;
  };
  return (
    <section className="details" aria-label="My details">
      <p className="muted">{me.type.toLowerCase().replace(/_/g, ' ')} · takes {me.acceptedTypes.map((f) => FOOD_META[f].label.toLowerCase()).join(', ')}</p>
      <div className="cap-row">
        <NumberField id="cap" label="Room available now" value={cap} min={0} max={1_000_000} onChange={setCap} suffix="portions" hint="Goes down when you accept an offer." />
        <button type="button" className="btn" disabled={busy || cap === me.availableCapacity} onClick={() => void save({ capacity: cap })}>Save</button>
      </div>
      <div className="switches">
        <label className="switch"><input type="checkbox" checked={accepting} disabled={busy} onChange={(e) => { if (!e.target.checked) setOff(true); else { setAccepting(true); void save({ accepting: true }); } }} /> <span>Accepting food</span></label>
        <label className="switch"><input type="checkbox" checked={pickup} disabled={busy} onChange={(e) => { setPickup(e.target.checked); void save({ pickup: e.target.checked }).then((ok) => { if (!ok) setPickup(!e.target.checked); }); }} /> <span>We can collect</span></label>
      </div>
      {err && <p className="form-error" role="alert">{err}</p>}
      <ConfirmDialog open={off} title="Stop accepting food?" confirmLabel="Stop accepting" danger onClose={() => setOff(false)}
        onConfirm={() => { setAccepting(false); setOff(false); void save({ accepting: false }); }} body={<p>You will not be matched with new donations until you turn this back on. Offers already waiting are not affected.</p>} />
    </section>
  );
}

export function RecipientDetails() {
  const me = useStore((s) => s.me) as Rec | null;
  return (
    <Page title="My details" summary={me ? `${me.name} · used by the matching for every new donation` : undefined}>
      {me ? <Details me={me} /> : <div className="skeleton" style={{ height: 200 }} />}
    </Page>
  );
}

export default function Incoming() {
  const me = useStore((s) => s.me) as Rec | null;
  const loaded = useStore((s) => s.loadedOnce);
  const rows = useOfferRows();
  const now = useNow(30_000);
  const incoming = useMemo(() => rows.filter((r) => r.o.status === 'PENDING'), [rows]);
  const past = useMemo(() => rows.filter((r) => r.o.status !== 'PENDING').reverse(), [rows]);
  const { shown, more } = useShowMore(past, 10);
  return (
    <Page title="Incoming offers" summary={me ? `${me.name} · room for ${fmt(me.availableCapacity)} portions now${me.accepting ? '' : ' · not accepting food'}` : 'Loading…'}>
      <section className="sect" aria-labelledby="in-h">
        <h2 id="in-h">Waiting for your answer</h2>
        {!loaded ? <div className="skeleton" style={{ height: 140 }} /> : incoming.length === 0
          ? <Empty title="No offer is waiting for you.">New offers appear here as soon as the coordinator sends them. {me && !me.accepting && <a href="#/r/profile">You are not accepting food right now →</a>}</Empty>
          : <ul className="rows">{incoming.map((r) => <IncomingOffer key={r.o.id} r={r} room={me?.availableCapacity} now={now} />)}</ul>}
      </section>
      {past.length > 0 && (
        <section className="sect" aria-labelledby="past-h">
          <h2 id="past-h">Answered earlier</h2>
          <ul className="rows multi">
            {shown.map((r) => (
              <li key={r.o.id} className="row">
                <span className="row-main"><span className="line1"><span className="num">{fmt(r.o.quantity)}</span> <FoodTag food={r.food} /> <span className="muted">from {r.donorName}</span></span>
                  <span className="line2 small muted">{r.o.id}{r.answeredAt ? ` · ${clockTime(r.answeredAt)}` : ''}{r.o.txnId ? <> · <a href="#/r/deliveries">delivery {r.o.txnId}</a></> : null}</span></span>
                <span className="row-side"><StatusChip kind="offer" status={r.o.status} /></span>
              </li>
            ))}
          </ul>
          {more}
        </section>
      )}
    </Page>
  );
}
