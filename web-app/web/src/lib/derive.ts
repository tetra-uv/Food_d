// Memoised selectors: join the engine's records for display (names, nodes, timestamps). No business decisions here:
// eligibility, ranking and allocation come from the engine; this only looks things up and groups by the engine's status.
import { useMemo } from 'react';
import type { Donation, Food, Offer, Transaction } from '../api/types';
import { useStore } from '../store';

export interface Lookups {
  donation: Map<string, Donation>;
  donorName: (id: string | undefined | null) => string;
  donorNode: (id: string | undefined | null) => number | undefined;
  recName: (id: string | undefined | null) => string;
  recNode: (id: string | undefined | null) => number | undefined;
}

export function useLookups(): Lookups {
  const donations = useStore((s) => s.donations);
  const donors = useStore((s) => s.donors);
  const recipients = useStore((s) => s.recipients);
  const identities = useStore((s) => s.identities);
  return useMemo(() => {
    const donation = new Map(donations.map((d) => [d.id, d]));
    const dn = new Map<string, string>(), dnode = new Map<string, number>(), rn = new Map<string, string>(), rnode = new Map<string, number>();
    for (const d of identities?.donors ?? []) dn.set(d.id, d.name);
    for (const r of identities?.recipients ?? []) rn.set(r.id, r.name);
    for (const d of donors) { dn.set(d.id, d.name); dnode.set(d.id, d.node); }
    for (const r of recipients) { rn.set(r.id, r.name); rnode.set(r.id, r.node); }
    return {
      donation,
      donorName: (id) => (id ? dn.get(id) ?? id : '–'),
      donorNode: (id) => (id ? dnode.get(id) : undefined),
      recName: (id) => (id ? rn.get(id) ?? id : '–'),
      recNode: (id) => (id ? rnode.get(id) : undefined),
    };
  }, [donations, donors, recipients, identities]);
}

/** One offer joined with what the UI shows next to it. */
export interface OfferRow {
  o: Offer; food: Food | null; donorId: string | null; donorName: string; recipientName: string;
  donorNode: number | undefined; recipientNode: number | undefined; sentAt?: number; answeredAt?: number;
}
export function useOfferRows(): OfferRow[] {
  const offers = useStore((s) => s.offers);
  const act = useStore((s) => s.activity.offers);
  const L = useLookups();
  return useMemo(() => offers.map((o) => {
    const d = L.donation.get(o.donationId), c = o.context;
    const donorId = c?.donorId ?? d?.donorId ?? null;
    return {
      o, food: c?.food ?? d?.food ?? null, donorId,
      donorName: c?.donorName ?? L.donorName(donorId), recipientName: c?.recipientName ?? L.recName(o.recipientId),
      donorNode: c?.donorNode ?? L.donorNode(donorId), recipientNode: c?.recipientNode ?? L.recNode(o.recipientId),
      sentAt: act[o.id]?.sentAt, answeredAt: act[o.id]?.answeredAt,
    };
  }), [offers, act, L]);
}

/** A delivery = a transaction (an accepted offer recorded by the engine), with the simulated-clock start time. */
export interface DeliveryRow {
  t: Transaction; offer: OfferRow | null; food: Food | null; donorName: string; recipientName: string;
  donorNode: number | undefined; recipientNode: number | undefined; startAt?: number; sentAt?: number;
}
export function useDeliveryRows(): DeliveryRow[] {
  const transactions = useStore((s) => s.transactions);
  const act = useStore((s) => s.activity.deliveries);
  const rows = useOfferRows();
  const L = useLookups();
  return useMemo(() => {
    const byTxn = new Map(rows.filter((r) => r.o.txnId).map((r) => [r.o.txnId!, r]));
    return transactions.map((t) => {
      const of = byTxn.get(t.id) ?? null, d = L.donation.get(t.donationId);
      return {
        t, offer: of, food: of?.food ?? d?.food ?? null,
        donorName: of?.donorName ?? (d ? L.donorName(d.donorId) : '–'), recipientName: of?.recipientName ?? L.recName(t.recipientId),
        donorNode: of?.donorNode ?? (d ? L.donorNode(d.donorId) : undefined), recipientNode: of?.recipientNode ?? L.recNode(t.recipientId),
        startAt: act[t.id]?.startAt, sentAt: of?.sentAt,
      };
    }).reverse();
  }, [transactions, act, rows, L]);
}

/** Donation groups by the engine's own fields (status, effectiveRemaining, pendingOffered) and its urgency queue order. */
export interface DonationGroups { assign: Donation[]; offered: Donation[]; done: Donation[]; closed: Donation[] }
export function useDonationGroups(): DonationGroups {
  const donations = useStore((s) => s.donations);
  const queue = useStore((s) => s.queue);
  const act = useStore((s) => s.activity.donations);
  return useMemo(() => {
    const byId = new Map(donations.map((d) => [d.id, d]));
    const assign = (queue?.order ?? []).map((q) => byId.get(q.donationId)).filter((d): d is Donation => !!d && d.effectiveRemaining > 0);
    const inQueue = new Set(assign.map((d) => d.id));
    const open = (d: Donation) => d.status === 'PENDING' || d.status === 'PARTIAL';
    return {
      assign,
      offered: donations.filter((d) => open(d) && !inQueue.has(d.id)),
      done: donations.filter((d) => d.status === 'COMPLETED').reverse(),
      // most recently closed first (activity log), then newest id
      closed: donations.filter((d) => d.status === 'CLOSED').sort((a, b) => (act[b.id]?.closedAt ?? 0) - (act[a.id]?.closedAt ?? 0) || b.id.localeCompare(a.id)),
    };
  }, [donations, queue, act]);
}
