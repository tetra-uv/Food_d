import { memo, useEffect, useMemo, useState } from 'react';
import { api, ApiError } from '../api/client';
import type { AggregatePreview, Donation, Donor as DonorT, Food } from '../api/types';
import { FOODS } from '../api/types';
import { Empty, Glyph, NumberField, Page, Timeline, useDebounced, go, type Step } from '../components/ui';
import { PREVIEW_DEBOUNCE_MS } from '../config';
import { aggregateOf, fmt, fmtMin, plural } from '../lib/format';
import { useDeliveryRows, useOfferRows, type DeliveryRow, type OfferRow } from '../lib/derive';
import { FOOD_META, REASON_META, REASON_ORDER } from '../lib/meta';
import { clockTime, deliveryClock, useNow, type SimSpeed } from '../lib/simclock';
import { errorText, runMutation, useStore } from '../store';
import { DeliveryLine } from './common';

const PRESETS = [15, 30, 60, 120, 240];

/** waiting -> offered -> accepted -> on the way -> delivered, or closed with waste (from the engine's records + the activity log) */
function donationSteps(d: Donation, offers: OfferRow[], dels: DeliveryRow[], createdAt: number | undefined, closedAt: number | undefined, now: number, speed: SimSpeed): Step[] {
  const t = (x?: number) => (x ? clockTime(x) : undefined);
  const firstSent = offers.map((o) => o.sentAt).filter((x): x is number => !!x).sort()[0];
  const accepted = offers.filter((o) => o.o.status === 'ACCEPTED');
  const firstAcc = accepted.map((o) => o.answeredAt).filter((x): x is number => !!x).sort()[0];
  const clocks = dels.map((r) => deliveryClock(r.startAt, r.t.travelMinutes, now, speed));
  const onway = clocks.some((c) => c.phase === 'onway');
  const anyDel = dels.length > 0;
  const lastDone = clocks.filter((c) => c.phase === 'delivered').map((c) => c.doneAt ?? 0).sort().pop();
  const st = (reached: boolean, current: boolean): Step['state'] => (current ? 'now' : reached ? 'done' : 'todo');
  const stage = d.status === 'CLOSED' ? -1 : onway ? 3 : anyDel ? 4 : accepted.length ? 2 : offers.length ? 1 : 0;
  const steps: Step[] = [
    { label: 'Waiting', at: t(createdAt), state: st(true, stage === 0) },
    { label: 'Offered', at: t(firstSent), state: st(offers.length > 0, stage === 1) },
    { label: 'Accepted', at: t(firstAcc), state: st(accepted.length > 0 || anyDel, stage === 2) },
    { label: 'On the way', state: st(anyDel, stage === 3) },
  ];
  if (d.status === 'CLOSED') steps.push({ label: `Closed, ${fmt(d.remaining)} wasted`, at: t(closedAt), state: 'stop' });
  else steps.push({ label: d.status !== 'COMPLETED' && anyDel ? `Part delivered, ${fmt(d.remaining)} still to place` : 'Delivered', at: t(lastDone), state: stage === 4 ? (d.status === 'COMPLETED' ? 'done' : 'now') : 'todo' });
  return steps;
}

const DonationItem = memo(function DonationItem({ d, offers, dels, createdAt, closedAt, now, speed }: { d: Donation; offers: OfferRow[]; dels: DeliveryRow[]; createdAt?: number; closedAt?: number; now: number; speed: SimSpeed }) {
  const given = d.original - d.remaining;
  const waiting = offers.filter((o) => o.o.status === 'PENDING');
  return (
    <li className="donation-item">
      <p className="line1"><strong className="num">{fmt(d.original)}</strong> {FOOD_META[d.food].label.toLowerCase()} portions <span className="muted">· good for {fmtMin(d.usableMinutes)} · {d.id}</span></p>
      <p className="small muted">{d.status === 'CLOSED' ? `${fmt(given)} given, ${fmt(d.remaining)} wasted` : `${fmt(given)} given, ${fmt(d.remaining)} left`}{waiting.length > 0 && ` · offered to ${waiting.map((o) => o.recipientName).join(', ')} (waiting for the answer)`}</p>
      <Timeline label={`Status of donation ${d.id}`} steps={donationSteps(d, offers, dels, createdAt, closedAt, now, speed)} />
      {dels.length > 0 && <ul className="rows sub">{dels.map((r) => <DeliveryLine key={r.t.id} r={r} now={now} speed={speed} perspective="donor" />)}</ul>}
    </li>
  );
});

export function DonorHome() {
  const me = useStore((s) => s.me) as DonorT | null;
  const donations = useStore((s) => s.donations);
  const act = useStore((s) => s.activity.donations);
  const speed = useStore((s) => s.simSpeed);
  const loaded = useStore((s) => s.loadedOnce);
  const offers = useOfferRows();
  const dels = useDeliveryRows();
  const now = useNow(1000);
  const byDonation = useMemo(() => {
    const o = new Map<string, OfferRow[]>(), t = new Map<string, DeliveryRow[]>();
    for (const r of offers) o.set(r.o.donationId, [...(o.get(r.o.donationId) ?? []), r]);
    for (const r of dels) t.set(r.t.donationId, [...(t.get(r.t.donationId) ?? []), r]);
    return { o, t };
  }, [offers, dels]);
  const list = useMemo(() => [...donations].reverse(), [donations]);
  return (
    <Page title="My donations" summary={me ? `${me.name} · ${plural(donations.length, 'donation')}` : 'Loading…'}>
      <p><a className="btn primary" href="#/d/new">Add a donation</a></p>
      {!loaded ? <div className="skeleton" style={{ height: 160 }} /> : list.length === 0
        ? <Empty title="No donations yet."><a href="#/d/new">Add one</a> and see right away how many places could take it.</Empty>
        : <ul className="rows donations">{list.map((d) => <DonationItem key={d.id} d={d} offers={byDonation.o.get(d.id) ?? []} dels={byDonation.t.get(d.id) ?? []} createdAt={act[d.id]?.createdAt} closedAt={act[d.id]?.closedAt} now={now} speed={speed} />)}</ul>}
    </Page>
  );
}

function Feasibility({ agg, pending, err, usable }: { agg: AggregatePreview | null; pending: boolean; err: string | null; usable: number }) {
  if (err) return <p className="form-error" role="alert">{err}</p>;
  if (!agg) return <p className="feas muted" aria-live="polite">Checking…</p>;
  const n = agg.feasibleCount;
  const reasons = REASON_ORDER.filter((r) => agg.byReason[r]).sort((a, b) => (agg.byReason[b] ?? 0) - (agg.byReason[a] ?? 0));
  return (
    <div className="feas" aria-live="polite">
      <p className={`feas-main ${n === 0 ? 'tone-danger' : 'tone-ok'}`}>
        <Glyph name={n === 0 ? 'WARN' : 'OK'} size={20} />
        <span><b className="num">{n === 0 ? 'No places' : plural(n, 'place')}</b> can take it{agg.bestTravelMinutes !== null ? <>, the nearest {agg.bestTravelMinutes} min away</> : ''}{n === 0 ? ` within ${fmtMin(usable)}` : ''}.</span>
        {pending && <span className="spin" aria-label="Updating" />}
      </p>
      {n <= 1 && reasons.length > 0 && (
        <p className="small muted">Most others: {reasons.slice(0, 2).map((r) => `${REASON_META[r].label.toLowerCase()} (${agg.byReason[r]})`).join(', ')}.</p>
      )}
      <p className="small muted">Counts only; names stay private until a coordinator sends an offer.</p>
    </div>
  );
}

export function DonorNew() {
  const me = useStore((s) => s.me) as DonorT | null;
  const [food, setFood] = useState<Food>('COOKED');
  const [qty, setQty] = useState(50);
  const [usable, setUsable] = useState(30);
  const [agg, setAgg] = useState<AggregatePreview | null>(null);
  const [pending, setPending] = useState(false);
  const [pErr, setPErr] = useState<string | null>(null);
  const [busy, setBusy] = useState(false);
  const [err, setErr] = useState<string | null>(null);
  const [done, setDone] = useState<string | null>(null);
  const dUsable = useDebounced(usable, PREVIEW_DEBOUNCE_MS);
  const dQty = useDebounced(qty, PREVIEW_DEBOUNCE_MS);

  useEffect(() => {
    if (!me) return;
    const ac = new AbortController();
    setPending(true);
    api.preview({ donorId: me.id, food, quantity: dQty, usableMinutes: dUsable }, ac.signal)
      .then((p) => { setAgg(aggregateOf(p)); setPErr(null); })
      .catch((e) => { if ((e as Error).name !== 'AbortError') setPErr(e instanceof ApiError ? e.message : String(e)); })
      .finally(() => { if (!ac.signal.aborted) setPending(false); });
    return () => ac.abort();
  }, [me, food, dQty, dUsable]);

  const submit = async () => {
    if (!me) return;
    setBusy(true); setErr(null);
    const r = await runMutation(() => api.createDonation({ donorId: me.id, food, quantity: qty, usableMinutes: usable }), { own: (x) => [x.data.donation.id] });
    setBusy(false);
    if (r.ok) setDone(r.data.data.donation.id); else setErr(errorText(r.error));
  };

  if (done) return (
    <Page title="Donation added" summary={`${done} is now with the coordinator, who will assign it to the best place.`}>
      <p className="actions"><a className="btn primary" href="#/d">Follow it in My donations</a> <button type="button" className="btn" onClick={() => setDone(null)}>Add another</button></p>
    </Page>
  );
  return (
    <Page title="Add a donation" summary="Tell us what you have; you see right away how many places could take it in time.">
      <form className="form" onSubmit={(e) => { e.preventDefault(); void submit(); }}>
        <fieldset className="field">
          <legend>What kind of food?</legend>
          <div className="radio-row">
            {FOODS.map((f) => (
              <label key={f} className={`radio ${food === f ? 'on' : ''}`}>
                <input type="radio" name="food" value={f} checked={food === f} onChange={() => setFood(f)} />
                <Glyph name={f} size={18} /> {FOOD_META[f].label}
              </label>
            ))}
          </div>
        </fieldset>
        <NumberField id="qty" label="How much?" value={qty} min={1} max={1_000_000} onChange={setQty} suffix="portions" />
        <div>
          <NumberField id="usable-n" label="How long does it stay good?" value={usable} min={1} max={100_000_000} onChange={setUsable} suffix="minutes" />
          <p className="presets small" role="group" aria-label="Quick choices">{PRESETS.map((p) => (
            <button key={p} type="button" className="link-btn" aria-pressed={usable === p} onClick={() => setUsable(p)}>{p < 60 ? `${p} min` : `${p / 60} h`}</button>
          ))}</p>
        </div>
        <Feasibility agg={agg} pending={pending} err={pErr} usable={dUsable} />
        {err && <p className="form-error" role="alert">{err}</p>}
        <div className="actions"><button type="submit" className="btn primary" disabled={busy || !me}>{busy ? 'Adding…' : `Add ${fmt(qty)} ${FOOD_META[food].label.toLowerCase()} portions`}</button><button type="button" className="link-btn" onClick={() => go('/d')}>Cancel</button></div>
      </form>
    </Page>
  );
}
