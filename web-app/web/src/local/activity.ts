// Local activity log: wall-clock timestamps for things the engine records without a clock (offer sent / answered,
// delivery started, donation added / closed). It is presentation state only: it is saved in the same IndexedDB record as
// the engine's CSVs (one put, under the same write lock) and is NEVER fed back into the engine. Rows that existed before
// this log (pristine data) simply have no entry; the UI shows them as "earlier".

export interface OfferActivity { sentAt?: number; answeredAt?: number; answer?: string }
export interface DeliveryActivity { startAt: number; offerId?: string | null }
export interface DonationActivity { createdAt?: number; closedAt?: number }
export interface Activity {
  offers: Record<string, OfferActivity>;
  deliveries: Record<string, DeliveryActivity>;
  donations: Record<string, DonationActivity>;
}

export const emptyActivity = (): Activity => ({ offers: {}, deliveries: {}, donations: {} });

interface Row { id: string; before?: Record<string, unknown>; after?: Record<string, unknown> }
interface Effects { offers?: Row[]; transactions?: Row[]; donations?: Row[] }

/** Returns a new Activity with the timestamps implied by one engine write's `effects`. */
export function applyEffects(prev: Activity, effects: Effects | null | undefined, now: number): Activity {
  if (!effects) return prev;
  const a: Activity = { offers: { ...prev.offers }, deliveries: { ...prev.deliveries }, donations: { ...prev.donations } };
  for (const e of effects.offers ?? []) {
    if (!e.before) a.offers[e.id] = { sentAt: now };
    else if (e.before.status === 'PENDING' && typeof e.after?.status === 'string' && e.after.status !== 'PENDING') {
      a.offers[e.id] = { ...a.offers[e.id], answeredAt: now, answer: e.after.status };
    }
  }
  for (const t of effects.transactions ?? []) {
    if (t.before) continue;
    const offerId = (effects.offers ?? []).find((o) => o.after?.txnId === t.id)?.id ?? null;
    a.deliveries[t.id] = { startAt: now, offerId };
  }
  for (const d of effects.donations ?? []) {
    if (!d.before) a.donations[d.id] = { createdAt: now };
    else if (d.after?.status === 'CLOSED') a.donations[d.id] = { ...a.donations[d.id], closedAt: now };
  }
  return a;
}
