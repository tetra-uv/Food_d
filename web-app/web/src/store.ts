import { create } from 'zustand';
import { api, ApiError } from './api/client';
import { connectEvents, type ConnState } from './api/events';
import type { Activity, ApiEvent, Donation, Donor, Identities, Meta, Network, Offer, QueueResponse, Recipient, Session, Transaction } from './api/types';
import { applyMotion, initialMotion } from './lib/motion';
import { fmt } from './lib/format';
import { readSpeed, type SimSpeed } from './lib/simclock';

export interface Toast {
  id: number; tone: 'ok' | 'info' | 'error'; text: string; action?: { label: string; run: () => void };
}

const EMPTY_ACTIVITY: Activity = { now: 0, offers: {}, deliveries: {}, donations: {} };

interface Store {
  phase: 'boot' | 'ready' | 'fatal';
  fatal: string | null;
  meta: Meta | null;
  network: Network | null;
  identities: Identities | null;
  session: Session | null;
  donors: Donor[]; recipients: Recipient[]; donations: Donation[]; transactions: Transaction[]; offers: Offer[];
  queue: QueueResponse | null;
  activity: Activity;
  me: Donor | Recipient | null;
  conn: ConnState;
  engineOk: boolean;
  toasts: Toast[];
  /** bumps on every live event (pages that hold engine results, e.g. a match, refetch on it) */
  eventSeq: number;
  loadedOnce: boolean;
  refreshError: string | null;
  simSpeed: SimSpeed;

  init: () => Promise<void>;
  signIn: (role: Session['role'], identityId?: string) => Promise<void>;
  signOut: () => Promise<void>;
  /** Switch role/identity in place (top-bar menu): clears the previous role's data first. */
  switchTo: (role: Session['role'], identityId?: string) => Promise<void>;
  refresh: () => Promise<void>;
  toast: (t: Omit<Toast, 'id'>) => void;
  dismissToast: (id: number) => void;
  markOwn: (...ids: (string | undefined | null)[]) => void;
  setSimSpeed: (s: SimSpeed) => void;
}

let toastSeq = 0;
let stopEvents: (() => void) | null = null;
let refreshTimer: ReturnType<typeof setTimeout> | null = null;
let refreshing = false, refreshAgain = false;
const own = new Set<string>();
const CLEAR = { donors: [], recipients: [], donations: [], transactions: [], offers: [], queue: null, me: null, activity: EMPTY_ACTIVITY, loadedOnce: false };

/** The gateway (http mode) has no /activity endpoint: no timestamps then, everything shows as "earlier". */
const activityOrEmpty = () => api.activity().catch(() => EMPTY_ACTIVITY);

export const useStore = create<Store>((set, get) => ({
  phase: 'boot', fatal: null, meta: null, network: null, identities: null, session: null,
  donors: [], recipients: [], donations: [], transactions: [], offers: [], queue: null, activity: EMPTY_ACTIVITY, me: null,
  conn: 'connecting', engineOk: true, toasts: [], eventSeq: 0, loadedOnce: false, refreshError: null, simSpeed: readSpeed(),

  async init() {
    applyMotion(initialMotion());
    try {
      const [meta, identities] = await Promise.all([api.meta(), api.identities()]);
      set({ meta, identities });
      let session: Session | null = null;
      try { session = await api.session(); } catch (e) { if (!(e instanceof ApiError) || e.status !== 401) throw e; }
      set({ session });
      if (session) await enterSession();
      set({ phase: 'ready' });
    } catch (e) {
      set({ phase: 'fatal', fatal: e instanceof ApiError ? `${e.message} (${e.code})` : String(e) });
    }
  },

  async signIn(role, identityId) {
    const session = await api.signIn(role, identityId);
    set({ session, loadedOnce: false });
    await enterSession();
  },

  async switchTo(role, identityId) {
    set(CLEAR);
    await get().signIn(role, identityId);
  },

  async signOut() {
    stopEvents?.(); stopEvents = null;
    try { await api.signOut(); } catch { /* ignore */ }
    set({ ...CLEAR, session: null, toasts: [] });
    location.hash = '#/';
  },

  async refresh() {
    if (refreshing) { refreshAgain = true; return; }
    refreshing = true;
    try {
      const s = get().session;
      if (!s) return;
      if (s.role === 'coordinator') {
        const [snap, queue, activity] = await Promise.all([api.snapshot(), api.queue(), activityOrEmpty()]);
        set({ donors: snap.donors, recipients: snap.recipients, donations: snap.donations, transactions: snap.transactions, offers: snap.offers, queue, activity });
      } else if (s.role === 'donor' && s.identityId) {
        const [me, donations, offers, transactions, activity] = await Promise.all([api.donor(s.identityId), api.donations(), api.offers(), api.transactions(), activityOrEmpty()]);
        set({ me, donors: [me], donations, offers, transactions, activity });
      } else if (s.role === 'recipient' && s.identityId) {
        const [me, offers, transactions, activity] = await Promise.all([api.recipient(s.identityId), api.offers(), api.transactions(), activityOrEmpty()]);
        set({ me, recipients: [me], offers, transactions, activity });
      }
      set({ loadedOnce: true, refreshError: null });
    } catch (e) {
      set({ refreshError: e instanceof ApiError ? e.message : String(e) });
    } finally {
      refreshing = false;
      if (refreshAgain) { refreshAgain = false; void get().refresh(); }
    }
  },

  toast(t) {
    const id = ++toastSeq;
    set((s) => ({ toasts: [...s.toasts.slice(-3), { ...t, id }] }));
  },
  dismissToast: (id) => set((s) => ({ toasts: s.toasts.filter((t) => t.id !== id) })),
  markOwn(...ids) { for (const i of ids) if (i) { own.add(i); setTimeout(() => own.delete(i), 8000); } },
  setSimSpeed(simSpeed) {
    try { localStorage.setItem('cb-sim-speed', simSpeed); } catch { /* private mode */ }
    set({ simSpeed });
  },
}));

// other tabs change the clock setting: follow it
if (typeof window !== 'undefined') window.addEventListener('storage', (e) => { if (e.key === 'cb-sim-speed') useStore.setState({ simSpeed: readSpeed() }); });

async function enterSession() {
  const st = useStore.getState();
  const net = await api.network();
  useStore.setState({ network: net });
  await st.refresh();
  stopEvents?.();
  stopEvents = connectEvents(onEvent, (conn) => useStore.setState({ conn }));
}

function scheduleRefresh() {
  if (refreshTimer) clearTimeout(refreshTimer);
  refreshTimer = setTimeout(() => void useStore.getState().refresh(), 120);
}

function describe(ev: ApiEvent, role: Session['role']): string | null {
  const d = ev.data;
  switch (ev.type) {
    case 'offer.sent':
      return role === 'recipient' ? `New offer ${d.offerId}: ${fmt(d.quantity)} portions` : `Offer ${d.offerId} sent: ${fmt(d.quantity)} portions`;
    case 'offer.accepted': return `Offer ${d.offerId} was accepted; delivery ${d.txnId ?? ''} is on the way`;
    case 'offer.declined': return `Offer ${d.offerId} was declined`;
    case 'offer.withdrawn': return `Offer ${d.offerId} was withdrawn`;
    case 'donation.created': return `Donation ${d.entity?.id ?? ''} was added`;
    case 'donation.closed': return `Donation ${d.entity?.id ?? ''} was closed`;
    default: return null;
  }
}

function onEvent(ev: ApiEvent) {
  const s = useStore.getState();
  useStore.setState({ eventSeq: s.eventSeq + 1 });
  if (ev.type === 'engine.status') {
    const st = (ev.data.state ?? '').toLowerCase();
    useStore.setState({ engineOk: st === '' || st === 'ready' || st === 'ok' });
    if (st === 'ready' || st === 'ok') scheduleRefresh();
    return;
  }
  if (ev.type === 'resync' || ev.type === 'data.external_change') { scheduleRefresh(); return; }
  const key = ev.data.offerId ?? ev.data.entity?.id;
  const text = describe(ev, s.session?.role ?? 'coordinator');
  // The event can arrive before our own mutation response does, so decide after a short delay whether it was ours.
  setTimeout(() => {
    if (key && own.has(key)) return;
    if (text) useStore.getState().toast({ tone: 'info', text });
  }, 350);
  scheduleRefresh();
}

/** Wrap a mutation: toast the outcome, refresh, return ok/error to the caller. */
export async function runMutation<T extends { message?: string }>(fn: () => Promise<T>, opts?: { own?: (r: T) => (string | undefined | null)[]; silent?: boolean }): Promise<{ ok: true; data: T } | { ok: false; error: ApiError }> {
  try {
    const r = await fn();
    if (opts?.own) useStore.getState().markOwn(...opts.own(r));
    if (!opts?.silent && r.message) useStore.getState().toast({ tone: 'ok', text: r.message });
    void useStore.getState().refresh();
    return { ok: true, data: r };
  } catch (e) {
    const error = e instanceof ApiError ? e : new ApiError(0, 'UNKNOWN', String(e));
    if (['OFFER_ALREADY_ANSWERED', 'DONATION_NOT_OPEN', 'DUPLICATE_PENDING_OFFER'].includes(error.code)) void useStore.getState().refresh();
    return { ok: false, error };
  }
}

export function errorText(e: ApiError): string {
  switch (e.code) {
    case 'EXCEEDS_CAPACITY': return `${e.message}${e.details && typeof e.details.max === 'number' ? ` (room for ${fmt(e.details.max as number)} right now)` : ''}. Ask the coordinator to re-offer.`;
    case 'OFFER_ALREADY_ANSWERED': return 'Someone just answered this offer. The list has been refreshed.';
    case 'DUPLICATE_PENDING_OFFER': return `An offer to this recipient is already waiting for an answer (${String(e.details?.offerId ?? '')}). Withdraw it first or wait for the answer.`;
    case 'NOT_A_CANDIDATE': return `This recipient can no longer take the donation${e.details?.reason ? ` (${String(e.details.reason).toLowerCase().replace(/_/g, ' ')})` : ''}.`;
    case 'OUTCOME_UNKNOWN': return "We couldn't confirm the result. Refreshing the data now.";
    case 'ENGINE_UNAVAILABLE': return 'The engine is restarting. Please try again in a moment.';
    default: return e.message;
  }
}
