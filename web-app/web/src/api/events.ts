// SSE client for GET /events (doc 05 section 7). EventSource reconnects by itself and sends Last-Event-ID.
import { API_BASE } from './client';
import type { ApiEvent } from './types';
import { BACKEND } from '../backend';

export type ConnState = 'connecting' | 'live' | 'reconnecting';

const TYPES = [
  'donation.created', 'donation.closed', 'donation.updated',
  'offer.sent', 'offer.accepted', 'offer.declined', 'offer.withdrawn',
  'transaction.created', 'recipient.updated', 'recipient.created', 'donor.created',
  'data.external_change', 'engine.status', 'resync', 'presence',
];

export function connectEvents(onEvent: (e: ApiEvent) => void, onState: (s: ConnState) => void): () => void {
  if (BACKEND === 'local') {
    // Local mode: events come from this tab's writes and from other tabs over BroadcastChannel (src/local/bus.ts).
    let stop: (() => void) | null = null, stopped = false;
    onState('connecting');
    void import('../local/api').then((m) => { if (!stopped) stop = m.subscribeLocal(onEvent, onState); });
    return () => { stopped = true; stop?.(); };
  }
  let es: EventSource | null = null;
  let stopped = false;
  const open = () => {
    if (stopped) return;
    onState('connecting');
    es = new EventSource(API_BASE + '/events', { withCredentials: true });
    es.onopen = () => onState('live');
    es.onerror = () => {
      // EventSource retries on its own (retry: 2000). If it gave up (CLOSED) we reopen manually.
      onState('reconnecting');
      if (es && es.readyState === EventSource.CLOSED) { es.close(); setTimeout(open, 2000); }
    };
    for (const t of TYPES) {
      es.addEventListener(t, (m) => {
        let data: ApiEvent['data'] = {};
        try { data = JSON.parse((m as MessageEvent).data); } catch { /* ignore malformed */ }
        onEvent({ type: t, data });
      });
    }
  };
  open();
  return () => { stopped = true; es?.close(); };
}
