import type { AggregatePreview, PreviewResponse, Reason } from '../api/types';

export const NO_EXPIRY_MIN = 1_000_000;
const nf = new Intl.NumberFormat('en-US');
export const fmt = (n: number | null | undefined): string => (n === null || n === undefined || Number.isNaN(n) ? '–' : nf.format(n));
export const isNoExpiry = (m: number) => m >= NO_EXPIRY_MIN;
/** "12 min", or the practically-no-expiry label for huge values. */
export const fmtMin = (m: number | null | undefined): string => {
  if (m === null || m === undefined) return '–';
  return isNoExpiry(m) ? 'No expiry (practically)' : `${nf.format(m)} min`;
};
export const plural = (n: number, one: string, many = one + 's') => `${nf.format(n)} ${n === 1 ? one : many}`;

/** Normalise a preview answer (a donor gets an aggregate; a full MatchView is reduced to counts). */
export function aggregateOf(p: PreviewResponse): AggregatePreview {
  if ('candidates' in p) {
    const byReason: Partial<Record<Reason, number>> = {};
    for (const r of p.rejected) byReason[r.reason] = (byReason[r.reason] ?? 0) + 1;
    return {
      feasibleCount: p.candidates.length,
      bestTravelMinutes: p.candidates.length ? Math.min(...p.candidates.map((c) => c.travelMinutes)) : null,
      byReason, totalRecipients: p.totals?.recipients,
    };
  }
  return p;
}

