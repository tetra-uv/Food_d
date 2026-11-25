import { memo, useMemo, useState } from 'react';
import type { Candidate, Gate, Reason, Recipient, Rejection } from '../api/types';
import { fmt, isNoExpiry } from '../lib/format';
import { REASON_META, REASON_ORDER, SEPARATED_TEXT } from '../lib/meta';
import { Glyph } from './ui';

type RecMap = Map<string, Recipient>;
const nm = (m: RecMap, id: string) => m.get(id)?.name ?? id;

// ============================================================ Rules: one compact line per rule
export function RuleSummary({ gates, survivors, total }: { gates: Gate[]; survivors: number; total: number }) {
  return (
    <ol className="rules">
      {gates.map((g) => (
        <li key={g.gate}>
          <span>{REASON_META[g.reason].gateName}</span>
          <span className="muted num">{g.rejected === 0 ? 'all pass' : `${fmt(g.rejected)} left out`}</span>
        </li>
      ))}
      <li className="rules-end"><span>Can take it</span><span className="num">{fmt(survivors)} of {fmt(total)}</span></li>
    </ol>
  );
}

// ============================================================ Best recipients
function decidedBy(c: Candidate, prev: Candidate | undefined): string | null {
  if (!prev || !c.separatedFromPrevious) return null;
  switch (c.separatedFromPrevious) {
    case 'ACCEPT': return `Ranked lower: can take less (${fmt(c.accept)} vs ${fmt(prev.accept)}).`;
    case 'TRAVEL': return `Ranked lower: same amount, but further away (${c.travelMinutes} vs ${prev.travelMinutes} min).`;
    case 'UNUSED': return `Ranked lower: same amount and distance; decided by ${SEPARATED_TEXT.UNUSED} (${fmt(c.unusedCapacity)} vs ${fmt(prev.unusedCapacity)}).`;
    default: return 'Ranked lower: tied on everything; decided by a fixed order.';
  }
}

/** Ranked recipients from match.build, in the engine's order, with plain reasons. A recipient that already has an offer
 *  waiting on this donation shows "Offer waiting" instead of an Assign button (one waiting offer per pair). */
export const RankingList = memo(function RankingList({ candidates, recs, usable, disabled, waiting, onAssign, onHover }: {
  candidates: Candidate[]; recs: RecMap; usable: number; disabled?: boolean; waiting: Map<string, string>;
  onAssign: (c: Candidate) => void; onHover: (id: string | null) => void;
}) {
  return (
    <ol className="rows ranking">
      {candidates.map((c, i) => {
        const note = decidedBy(c, candidates[i - 1]);
        const timing = isNoExpiry(usable) ? 'the food keeps' : c.onBoundary ? 'arrives exactly at the limit (allowed)' : `arrives with ${c.marginMinutes} min to spare`;
        const wait = waiting.get(c.recipientId);
        return (
          <li key={c.recipientId} className="row" onMouseEnter={() => onHover(c.recipientId)} onMouseLeave={() => onHover(null)}>
            <span className="rank-n" aria-label={`Rank ${c.rank}`}>{c.rank}</span>
            <div className="row-main">
              <strong>{nm(recs, c.recipientId)}</strong>
              <div className="muted small">{c.travelMinutes} min away · can take {fmt(c.accept)} · {timing}</div>
              {note && <div className="muted small">{note}</div>}
            </div>
            {wait
              ? <a className="row-state" href={`#/c/offers/${wait}`}>Offer waiting</a>
              : <button type="button" className={`btn ${i === 0 ? 'primary' : ''}`} disabled={disabled} onClick={() => onAssign(c)} onFocus={() => onHover(c.recipientId)} onBlur={() => onHover(null)}>Assign…</button>}
          </li>
        );
      })}
    </ol>
  );
});

// ============================================================ Why not others
export function RejectedList({ rejected, recs }: { rejected: Rejection[]; recs: RecMap }) {
  const [filter, setFilter] = useState<Reason | ''>('');
  const [q, setQ] = useState('');
  const counts = useMemo(() => { const c: Partial<Record<Reason, number>> = {}; for (const r of rejected) c[r.reason] = (c[r.reason] ?? 0) + 1; return c; }, [rejected]);
  const all = rejected.filter((r) => (!filter || r.reason === filter) && (!q || (nm(recs, r.recipientId) + r.recipientId).toLowerCase().includes(q.toLowerCase())));
  // big scenarios (delhi-real: 100+ rejections) render in pages to keep the list cheap
  const [limit, setLimit] = useState(40);
  const rows = all.slice(0, limit);
  const big = rejected.length > 8;
  return (
    <div className="rejected">
      <p className="small">{REASON_ORDER.filter((r) => counts[r]).map((r) => `${REASON_META[r].label} ${counts[r]}`).join(' · ')}</p>
      {big && (
        <div className="rej-tools">
          <label className="sr-only" htmlFor="rej-f">Filter by reason</label>
          <select id="rej-f" value={filter} onChange={(e) => setFilter(e.target.value as Reason | '')}>
            <option value="">All reasons</option>
            {REASON_ORDER.filter((r) => counts[r]).map((r) => <option key={r} value={r}>{REASON_META[r].label} ({counts[r]})</option>)}
          </select>
          <input type="search" placeholder="Search by name" value={q} onChange={(e) => setQ(e.target.value)} aria-label="Search recipients that were left out" />
        </div>
      )}
      <ul className="rows rej-list">
        {rows.map((r) => (
          <li key={r.recipientId} className="row">
            <div className="row-main">
              <span>{nm(recs, r.recipientId)}</span>
              <div className="small muted"><span className="tone-danger"><Glyph name={r.reason} size={13} /> {REASON_META[r.reason].label}</span>{' · '}{r.text}{r.reason === 'TOO_FAR' && r.travelMinutes !== null ? ` (${r.travelMinutes} min, ${r.overByMinutes ?? '?'} too many)` : ''}</div>
            </div>
          </li>
        ))}
        {rows.length === 0 && <li className="row muted">Nothing matches.</li>}
      </ul>
      {all.length > rows.length && <button type="button" className="link-btn" onClick={() => setLimit(limit + 80)}>Show {Math.min(80, all.length - rows.length)} more of {all.length - rows.length}</button>}
    </div>
  );
}
