import { memo, useMemo, useState } from 'react';
import { api } from '../../api/client';
import { ConfirmDialog, Empty, FoodTag, Page, StatusChip, Tabs, Timeline, useShowMore, type Step } from '../../components/ui';
import { useOfferRows, type OfferRow } from '../../lib/derive';
import { fmt } from '../../lib/format';
import { ago, clockTime, useNow } from '../../lib/simclock';
import { errorText, runMutation, useStore } from '../../store';

function useWithdraw() {
  const [id, setId] = useState<string | null>(null);
  const [busy, setBusy] = useState(false);
  const [err, setErr] = useState<string | null>(null);
  const run = async () => {
    if (!id) return;
    setBusy(true); setErr(null);
    const r = await runMutation(() => api.withdraw(id), { own: () => [id] });
    setBusy(false);
    if (r.ok) setId(null); else setErr(errorText(r.error));
  };
  const dialog = <ConfirmDialog open={!!id} title={`Withdraw offer ${id ?? ''}?`} confirmLabel="Withdraw offer" danger busy={busy} error={err} onClose={() => setId(null)} onConfirm={() => void run()}
    body={<p>The recipient can no longer accept it. The food goes back to “Needs assignment”.</p>} />;
  return { ask: (x: string) => { setErr(null); setId(x); }, dialog };
}

const WaitingRow = memo(function WaitingRow({ r, now, onWithdraw }: { r: OfferRow; now: number; onWithdraw: (id: string) => void }) {
  return (
    <li className="row">
      <a className="row-main plain" href={`#/c/offers/${r.o.id}`}>
        <span className="line1"><strong className="num">{fmt(r.o.quantity)}</strong> <FoodTag food={r.food} /> <span className="muted">{r.donorName} → {r.recipientName}</span></span>
        <span className="line2 small muted">{r.o.id} · donation {r.o.donationId} · {r.o.travelMinutes} min drive · {r.sentAt ? `sent ${clockTime(r.sentAt)}, waiting ${ago(r.sentAt, now)}` : 'sent earlier'}</span>
      </a>
      <span className="row-side"><button type="button" className="link-btn" onClick={() => onWithdraw(r.o.id)}>Withdraw</button></span>
    </li>
  );
});

const HistoryRow = memo(function HistoryRow({ r }: { r: OfferRow }) {
  return (
    <li>
      <a className="row row-link" href={`#/c/offers/${r.o.id}`}>
        <span className="row-main">
          <span className="line1"><strong className="num">{fmt(r.o.quantity)}</strong> <FoodTag food={r.food} /> <span className="muted">{r.donorName} → {r.recipientName}</span></span>
          <span className="line2 small muted">{r.o.id} · donation {r.o.donationId}{r.answeredAt ? ` · answered ${clockTime(r.answeredAt)}` : ''}{r.o.txnId ? ` · delivery ${r.o.txnId}` : ''}</span>
        </span>
        <span className="row-side"><StatusChip kind="offer" status={r.o.status} /></span>
      </a>
    </li>
  );
});

export default function Offers({ view }: { view: 'waiting' | 'history' }) {
  const loaded = useStore((s) => s.loadedOnce);
  const rows = useOfferRows();
  const now = useNow(30_000);
  const wd = useWithdraw();
  const waiting = useMemo(() => rows.filter((r) => r.o.status === 'PENDING').sort((a, b) => (a.sentAt ?? 0) - (b.sentAt ?? 0)), [rows]);
  const history = useMemo(() => rows.filter((r) => r.o.status !== 'PENDING').sort((a, b) => (b.answeredAt ?? 0) - (a.answeredAt ?? 0) || b.o.id.localeCompare(a.o.id)), [rows]);
  const list = view === 'waiting' ? waiting : history;
  const { shown, more } = useShowMore(list, 30);
  return (
    <Page title="Offers" summary={view === 'waiting' ? 'Offers sent to recipients and waiting for their answer, longest waiting first.' : 'Answered and withdrawn offers, newest first.'}>
      <Tabs current={view === 'waiting' ? '/c/offers' : '/c/offers/history'} tabs={[{ href: '/c/offers', label: 'Waiting for an answer', count: waiting.length }, { href: '/c/offers/history', label: 'History', count: history.length }]} />
      {!loaded ? <div className="skeleton" style={{ height: 240 }} /> : list.length === 0 ? (
        <Empty title={view === 'waiting' ? 'No offer is waiting for an answer.' : 'No answered offers yet.'}><a href="#/c/donations">Assign a donation →</a></Empty>
      ) : (
        <>
          <ul className="rows multi">{view === 'waiting' ? shown.map((r) => <WaitingRow key={r.o.id} r={r} now={now} onWithdraw={wd.ask} />) : shown.map((r) => <HistoryRow key={r.o.id} r={r} />)}</ul>
          {more}
        </>
      )}
      {wd.dialog}
    </Page>
  );
}

export function offerSteps(r: OfferRow): Step[] {
  const sent: Step = { label: 'Sent', at: r.sentAt ? clockTime(r.sentAt) : 'earlier', state: 'done' };
  const at = r.answeredAt ? clockTime(r.answeredAt) : undefined;
  switch (r.o.status) {
    case 'PENDING': return [sent, { label: 'Waiting for the answer', state: 'now' }];
    case 'ACCEPTED': return [sent, { label: 'Accepted', at, state: 'done' }, { label: 'Delivery', state: 'now' }];
    case 'DECLINED': return [sent, { label: 'Declined', at, state: 'stop' }];
    default: return [sent, { label: 'Withdrawn', at, state: 'stop' }];
  }
}

export function OfferDetail({ offerId }: { offerId: string }) {
  const loaded = useStore((s) => s.loadedOnce);
  const rows = useOfferRows();
  const now = useNow(30_000);
  const wd = useWithdraw();
  const r = rows.find((x) => x.o.id === offerId);
  if (!loaded) return <Page title="Offer"><div className="skeleton" style={{ height: 200 }} /></Page>;
  if (!r) return <Page title="Offer not found" crumb={{ href: '#/c/offers', label: 'Offers' }}><Empty title={`There is no offer ${offerId}.`} /></Page>;
  return (
    <Page crumb={{ href: r.o.status === 'PENDING' ? '#/c/offers' : '#/c/offers/history', label: 'Offers' }} title={`Offer ${r.o.id}`}
      summary={<>{fmt(r.o.quantity)} {r.food ? r.food.toLowerCase() : ''} portions · {r.donorName} → {r.recipientName} · <StatusChip kind="offer" status={r.o.status} /></>}>
      <Timeline label="Offer timeline" steps={offerSteps(r)} />
      <dl className="facts wide">
        <div><dt>Donation</dt><dd><a href={`#/c/donations/${r.o.donationId}`}>{r.o.donationId}</a></dd></div>
        <div><dt>Recipient</dt><dd>{r.recipientName} ({r.o.recipientId})</dd></div>
        <div><dt>Travel</dt><dd>{r.o.travelMinutes} min by road</dd></div>
        <div><dt>Sent</dt><dd>{r.sentAt ? `${clockTime(r.sentAt)}${r.o.status === 'PENDING' ? ` · waiting ${ago(r.sentAt, now)}` : ''}` : 'before this browser’s activity log'}</dd></div>
        {r.o.txnId && <div><dt>Delivery</dt><dd><a href={`#/c/deliveries/${r.o.txnId}`}>{r.o.txnId}</a></dd></div>}
      </dl>
      {r.o.status === 'PENDING' && <p className="quiet-actions"><button type="button" className="btn" onClick={() => wd.ask(r.o.id)}>Withdraw offer…</button></p>}
      {wd.dialog}
    </Page>
  );
}
