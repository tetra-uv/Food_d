import { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import { api, ApiError, uuid } from '../../api/client';
import type { Candidate, MatchView, Reason } from '../../api/types';
import { MapPanel } from '../../components/MapPanel';
import type { MapScene } from '../../components/NetworkMap';
import { RankingList, RejectedList, RuleSummary } from '../../components/studio';
import { Banner, ConfirmDialog, Empty, Modal, NumberField, Page, StatusChip, go } from '../../components/ui';
import { useLookups, useOfferRows } from '../../lib/derive';
import { fmt, fmtMin, isNoExpiry } from '../../lib/format';
import { FOOD_META } from '../../lib/meta';
import { ago, clockTime, useNow } from '../../lib/simclock';
import { errorText, runMutation, useStore } from '../../store';

// ---------------------------------------------------------------- assign dialog (creates an offer)
function AssignDialog({ cand, view, recName, onClose, onSent }: { cand: Candidate | null; view: MatchView; recName: string; onClose: () => void; onSent: (offerId: string) => void }) {
  const [qty, setQty] = useState(cand?.accept ?? 1);
  const [busy, setBusy] = useState(false);
  const [err, setErr] = useState<string | null>(null);
  // one Idempotency-Key per opening of the dialog: a double click or a retry cannot create a second offer
  const key = useMemo(() => (cand ? uuid() : ''), [cand]);
  const sending = useRef(false);
  useEffect(() => { setQty(cand?.accept ?? 1); setErr(null); setBusy(false); sending.current = false; }, [cand]);
  const max = cand?.accept ?? 1;
  const left = view.donation.effectiveRemaining;
  const send = async () => {
    if (!cand || sending.current) return;
    sending.current = true; setBusy(true); setErr(null);
    const r = await runMutation(() => api.sendOffer({ donationId: view.donation.id, recipientId: cand.recipientId, quantity: qty }, key), { own: (x) => [x.data.offer.id], silent: true });
    sending.current = false; setBusy(false);
    if (r.ok) onSent(r.data.data.offer.id); else setErr(errorText(r.error));
  };
  return (
    <Modal open={!!cand} onClose={onClose} title={`Assign to ${recName}`}>
      {cand && (
        <form onSubmit={(e) => { e.preventDefault(); void send(); }}>
          <dl className="facts">
            <div><dt>Food</dt><dd>{FOOD_META[view.donation.food].label}, {fmt(left)} left to assign</dd></div>
            <div><dt>Travel</dt><dd className="num">{cand.travelMinutes} min{isNoExpiry(view.donation.usableMinutes) ? '' : ` of ${fmt(view.donation.usableMinutes)} usable`}</dd></div>
            <div><dt>Arrival</dt><dd>{cand.onBoundary ? 'exactly at the limit (allowed)' : isNoExpiry(view.donation.usableMinutes) ? 'the food keeps' : `with ${cand.marginMinutes} min to spare`}</dd></div>
            <div><dt>Route</dt><dd className="small">{cand.routeText}</dd></div>
          </dl>
          <NumberField id="assign-qty" label="Quantity" value={qty} min={1} max={max} onChange={setQty} suffix="portions"
            hint={`They can take up to ${fmt(max)}. ${fmt(Math.max(left - qty, 0))} stay to assign to others.`} />
          <p className="small muted">The recipient gets an offer to accept or decline. Nothing is reserved until they accept.</p>
          {err && <p className="form-error" role="alert">{err}</p>}
          <div className="modal-actions">
            <button type="button" className="btn" onClick={onClose}>Cancel</button>
            <button type="submit" className="btn primary" disabled={busy} data-qty={qty}>{busy ? 'Sending…' : `Send offer for ${fmt(qty)}`}</button>
          </div>
        </form>
      )}
    </Modal>
  );
}

const SUGGEST: Record<Reason, string> = {
  TOO_FAR: 'Everyone else is further away than the food stays good.',
  NOT_ACCEPTING: 'Many recipients are not accepting at the moment.',
  FOOD_INCOMPATIBLE: 'Most recipients do not take this kind of food.',
  NO_CAPACITY: 'Recipients that take this food have no room left.',
  NO_PICKUP: 'Recipients that could take it cannot collect.',
  UNREACHABLE: 'There is no road from the donor to most recipients.',
};

// ---------------------------------------------------------------- page
export default function DonationPage({ donationId }: { donationId: string }) {
  const network = useStore((s) => s.network);
  const recipients = useStore((s) => s.recipients);
  const loaded = useStore((s) => s.loadedOnce);
  const eventSeq = useStore((s) => s.eventSeq);
  const L = useLookups();
  const offerRows = useOfferRows();
  const now = useNow(30_000);
  const d = L.donation.get(donationId);
  const open = !!d && (d.status === 'PENDING' || d.status === 'PARTIAL');
  const recs = useMemo(() => new Map(recipients.map((r) => [r.id, r])), [recipients]);

  const [match, setMatch] = useState<MatchView | null>(null);
  const [loadErr, setLoadErr] = useState<string | null>(null);
  const [hover, setHover] = useState<string | null>(null);
  const [whyOpen, setWhyOpen] = useState(false); // the left-out list is rendered only when opened
  const [assign, setAssign] = useState<Candidate | null>(null);
  const [closing, setClosing] = useState(false);
  const [closeBusy, setCloseBusy] = useState(false);
  const [closeErr, setCloseErr] = useState<string | null>(null);

  const load = useCallback(async () => {
    try { setMatch(await api.match(donationId)); setLoadErr(null); }
    catch (e) { setLoadErr(e instanceof ApiError ? `${e.message} (${e.code})` : String(e)); }
  }, [donationId]);
  // the ranking is the engine's answer for what is left now: refetch after any change (debounced)
  useEffect(() => { if (open) void load(); else setMatch(null); }, [open, load]);
  useEffect(() => { if (!eventSeq || !open) return; const t = setTimeout(() => void load(), 250); return () => clearTimeout(t); }, [eventSeq, open, load]);

  const mine = useMemo(() => offerRows.filter((r) => r.o.donationId === donationId).reverse(), [offerRows, donationId]);
  const waiting = useMemo(() => new Map(mine.filter((r) => r.o.status === 'PENDING').map((r) => [r.o.recipientId, r.o.id])), [mine]);

  const donorNode = L.donorNode(d?.donorId);
  const cands = open && match ? match.candidates : [];
  const scene = useMemo<MapScene>(() => {
    const sel = hover ?? cands[0]?.recipientId;
    return {
      source: donorNode ?? null,
      routes: cands.map((c) => ({ id: c.recipientId, route: c.route, kind: c.recipientId === sel ? 'selected' as const : 'ghost' as const })),
      candidates: cands.flatMap((c) => { const n = recs.get(c.recipientId)?.node; return n === undefined ? [] : [{ node: n, rank: c.rank }]; }),
      highlight: hover ? recs.get(hover)?.node ?? null : null,
      pending: mine.filter((r) => r.o.status === 'PENDING' && r.donorNode !== undefined && r.recipientNode !== undefined).map((r) => ({ id: r.o.id, from: r.donorNode!, to: r.recipientNode! })),
      focus: donorNode === undefined ? [] : [donorNode, ...cands.slice(0, 6).flatMap((c) => { const n = recs.get(c.recipientId)?.node; return n === undefined ? [] : [n]; })],
    };
  }, [cands, hover, donorNode, recs, mine]);

  if (!loaded) return <Page title="Donation"><div className="skeleton" style={{ height: 320 }} /></Page>;
  if (!d) return <Page title="Donation not found" crumb={{ href: '#/c/donations', label: 'Donations' }}><Empty title={`There is no donation ${donationId}.`}><a href="#/c/donations">Back to donations</a></Empty></Page>;

  const place = donorNode !== undefined ? network?.nodes[donorNode]?.label : undefined;
  const others = match ? match.rejected.length + match.declined.length : 0;
  const dominant = match ? [...match.gates].sort((a, b) => b.rejected - a.rejected)[0] : undefined;
  const doClose = async () => {
    setCloseBusy(true); setCloseErr(null);
    const r = await runMutation(() => api.closeDonation(donationId));
    setCloseBusy(false);
    if (r.ok) { setClosing(false); go('/c/donations'); } else setCloseErr(errorText(r.error));
  };
  const status = d.status === 'COMPLETED' ? 'Done: everything was given out.' : d.status === 'CLOSED' ? `Closed: ${fmt(d.remaining)} recorded as wasted.` : d.effectiveRemaining === 0 ? 'Fully offered: waiting for answers.' : null;

  return (
    <Page crumb={{ href: '#/c/donations', label: 'Donations' }}
      title={<><span className="num">{fmt(open ? d.effectiveRemaining : d.remaining)}</span> of {fmt(d.original)} {FOOD_META[d.food].label.toLowerCase()} portions {open ? 'left' : d.status === 'CLOSED' ? 'wasted' : 'left'}</>}
      summary={<>{fmtMin(d.usableMinutes)} usable · from {L.donorName(d.donorId)}{place ? ` · ${place}` : ''} · {d.id}{d.pendingOffered > 0 ? ` · ${fmt(d.pendingOffered)} in offers` : ''}</>}
      aside={<MapPanel scene={scene} label="Map focused on the donor with routes to the recipients that can take it" />}>
      {status && <Banner tone={d.status === 'CLOSED' ? 'warn' : 'info'}>{status}</Banner>}
      {loadErr && <Banner tone="error" action={<button className="btn" onClick={() => void load()}>Retry</button>}>Could not load the recommendations: {loadErr}</Banner>}

      {open && d.effectiveRemaining > 0 && (
        <section className="sect" aria-labelledby="rec-h">
          <h2 id="rec-h">Recommended recipients</h2>
          {!match ? <div className="skeleton" style={{ height: 180 }} /> : (
            <>
              <p className="small muted sect-note">{fmt(match.totals.recipients)} checked by the engine · {fmt(match.candidates.length)} can take it in time, best first</p>
              {match.candidates.length === 0
                ? <Empty title={`No recipient can take this donation${dominant && dominant.rejected > 0 ? `: ${SUGGEST[dominant.reason]}` : '.'}`}>You can close it below to record the food as wasted.</Empty>
                : <RankingList candidates={match.candidates} recs={recs} usable={match.donation.usableMinutes} waiting={waiting} onAssign={setAssign} onHover={setHover} />}
              {others > 0 && (
                <details className="disclosure" onToggle={(e) => setWhyOpen((e.target as HTMLDetailsElement).open)}>
                  <summary>Why not others ({fmt(others)})</summary>
                  {whyOpen && <div className="disclosure-in">
                    <RuleSummary gates={match.gates} survivors={match.survivors} total={match.totals.recipients} />
                    {match.declined.length > 0 && <p className="small muted">Declined this donation earlier: {match.declined.map((x) => recs.get(x.recipientId)?.name ?? x.recipientId).join(', ')}.</p>}
                    <RejectedList rejected={match.rejected} recs={recs} />
                  </div>}
                </details>
              )}
            </>
          )}
        </section>
      )}

      <section className="sect" aria-labelledby="off-h">
        <h2 id="off-h">Offers on this donation</h2>
        {mine.length === 0 ? <Empty title="No offers yet.">{open ? 'Assign it to a recommended recipient above.' : null}</Empty> : (
          <ul className="rows">
            {mine.map((r) => (
              <li key={r.o.id}>
                <a className="row row-link" href={r.o.txnId ? `#/c/deliveries/${r.o.txnId}` : `#/c/offers/${r.o.id}`}>
                  <span className="row-main">
                    <span className="line1"><strong className="num">{fmt(r.o.quantity)}</strong> to {r.recipientName}</span>
                    <span className="line2 small muted">{r.o.id} · {r.o.travelMinutes} min away{r.sentAt ? ` · sent ${clockTime(r.sentAt)}` : ''}{r.o.status === 'PENDING' && r.sentAt ? ` · waiting ${ago(r.sentAt, now)}` : ''}{r.o.txnId ? ` · delivery ${r.o.txnId}` : ''}</span>
                  </span>
                  <span className="row-side"><StatusChip kind="offer" status={r.o.status} /></span>
                </a>
              </li>
            ))}
          </ul>
        )}
      </section>

      {open && (
        <p className="quiet-actions">
          <button type="button" className="link-btn tone-danger" onClick={() => setClosing(true)}>Close donation…</button>
          <span className="small muted"> records what is left ({fmt(d.remaining)}) as wasted</span>
        </p>
      )}

      {match && <AssignDialog cand={assign} view={match} recName={assign ? recs.get(assign.recipientId)?.name ?? assign.recipientId : ''} onClose={() => setAssign(null)}
        onSent={(id) => { setAssign(null); useStore.getState().toast({ tone: 'ok', text: `Offer ${id} sent; waiting for the answer`, action: { label: 'Undo', run: () => { void runMutation(() => api.withdraw(id), { own: () => [id] }); } } }); void load(); }} />}
      <ConfirmDialog open={closing} title="Close this donation?" danger confirmLabel={`Close and record ${fmt(d.remaining)} as wasted`} busy={closeBusy} error={closeErr} onClose={() => setClosing(false)} onConfirm={doClose}
        body={<><p><strong>{fmt(d.remaining)} {FOOD_META[d.food].label.toLowerCase()} portions will be recorded as wasted.</strong> This cannot be undone.</p>{d.pendingOffered > 0 && <p>{fmt(d.pendingOffered)} portions are in offers waiting for an answer; those offers will be withdrawn.</p>}</>} />
    </Page>
  );
}

