// Small hand-written fetch client for /api/v1 (doc 05). Same-origin; Vite proxies /api to the mock or the gateway.
import type {
  Activity, AggregatePreview, Identities, MatchView, Meta, MutationResponse, Network, Offer, OfferCheck, PreviewResponse,
  QueueResponse, RouteResult, Session, Snapshot, Donation, Transaction, Recipient, Donor, Food,
} from './types';
import { ApiError } from './errors';
import { BACKEND } from '../backend';

export const API_BASE = '/api/v1';

export { ApiError } from './errors';

export function uuid(): string {
  try { if (crypto.randomUUID) return crypto.randomUUID(); } catch { /* insecure context */ }
  return 'k' + Date.now().toString(36) + Math.random().toString(36).slice(2, 10);
}

let localP: Promise<typeof import('../local/api')> | null = null;
/** Start loading the in-browser API (and with it the engine) early; called at boot. */
export const preloadLocal = () => (localP ??= import('../local/api'));

async function request<T>(method: string, path: string, body?: unknown, signal?: AbortSignal, idemKey?: string): Promise<T> {
  if (BACKEND === 'local') {
    // Same endpoints and shapes, answered in this browser by the WebAssembly engine (src/local/api.ts).
    const { localRequest } = await preloadLocal();
    return localRequest<T>(method, path, body, signal, idemKey);
  }
  const headers: Record<string, string> = { Accept: 'application/json' };
  if (body !== undefined) headers['Content-Type'] = 'application/json';
  if (method !== 'GET') { headers['Idempotency-Key'] = idemKey ?? uuid(); headers['X-CB-Intent'] = '1'; }
  let res: Response;
  try {
    res = await fetch(API_BASE + path, { method, headers, body: body === undefined ? undefined : JSON.stringify(body), credentials: 'same-origin', signal });
  } catch (e) {
    if ((e as Error).name === 'AbortError') throw e;
    throw new ApiError(0, 'NETWORK', 'Cannot reach the server. Check your connection and try again.');
  }
  const text = await res.text();
  let data: unknown = null;
  if (text) { try { data = JSON.parse(text); } catch { data = null; } }
  if (!res.ok) {
    const p = (data ?? {}) as { code?: string; detail?: string; title?: string; details?: Record<string, unknown> };
    throw new ApiError(res.status, p.code || 'HTTP_' + res.status, p.detail || p.title || res.statusText || 'Request failed', p.details);
  }
  return data as T;
}

const get = <T,>(p: string, signal?: AbortSignal) => request<T>('GET', p, undefined, signal);
const post = <T,>(p: string, b?: unknown) => request<T>('POST', p, b ?? {});

export const api = {
  meta: () => get<Meta>('/meta'),
  identities: () => get<Identities>('/identities'),
  session: () => get<Session>('/session'),
  signIn: (role: string, identityId?: string, pin?: string) => post<Session>('/session', { role, identityId, pin }),
  signOut: () => request<unknown>('DELETE', '/session'),
  network: () => get<Network>('/network'),
  snapshot: () => get<Snapshot>('/snapshot'),
  queue: () => get<QueueResponse>('/queue'),
  donations: () => get<Donation[]>('/donations'),
  offers: () => get<Offer[]>('/offers'),
  transactions: () => get<Transaction[]>('/transactions'),
  donor: (id: string) => get<Donor>('/donors/' + id),
  recipient: (id: string) => get<Recipient>('/recipients/' + id),
  activity: () => get<Activity>('/activity'),
  match: (donationId: string) => get<MatchView>(`/donations/${donationId}/match`),
  preview: (b: { donorId?: string; node?: number; food: Food; quantity: number; usableMinutes: number }, signal?: AbortSignal) =>
    request<PreviewResponse>('POST', '/match/preview', b, signal),
  route: (from: number, to: number) => get<RouteResult>(`/network/route?from=${from}&to=${to}`),
  offerCheck: (id: string) => get<OfferCheck>(`/offers/${id}/check`),
  /** `idemKey`: one per dialog opening, so a double click or a retry cannot create a second offer. */
  sendOffer: (b: { donationId: string; recipientId: string; quantity: number }, idemKey?: string) =>
    request<MutationResponse<{ offer: Offer }>>('POST', '/offers', b, undefined, idemKey),
  accept: (id: string) => post<MutationResponse<{ offer: Offer; transaction: Transaction }>>(`/offers/${id}/accept`),
  decline: (id: string) => post<MutationResponse>(`/offers/${id}/decline`, { confirm: true }),
  withdraw: (id: string) => post<MutationResponse>(`/offers/${id}/withdraw`, { confirm: true }),
  createDonation: (b: { donorId: string; food: Food; quantity: number; usableMinutes: number }) =>
    post<MutationResponse<{ donation: Donation }>>('/donations', b),
  closeDonation: (id: string) => post<MutationResponse<{ donation: Donation; wasted: number }>>(`/donations/${id}/close`, { confirm: true }),
  patchRecipient: (id: string, b: { capacity?: number; accepting?: boolean; pickup?: boolean }) =>
    request<MutationResponse<{ recipient: Recipient }>>('PATCH', `/recipients/${id}`, b),
};

export const isAggregate = (p: PreviewResponse): p is AggregatePreview => 'feasibleCount' in p && !('candidates' in p);
