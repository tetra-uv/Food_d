// Hand-written from docs 04 (entity/MatchView JSON) and 05 (HTTP API). The frontend only renders what the API returns.

export type Food = 'COOKED' | 'PACKAGED' | 'PRODUCE' | 'BAKERY';
export const FOODS: Food[] = ['COOKED', 'PACKAGED', 'PRODUCE', 'BAKERY'];
export type Role = 'coordinator' | 'donor' | 'recipient';

export type Reason =
  | 'NOT_ACCEPTING'
  | 'FOOD_INCOMPATIBLE'
  | 'NO_CAPACITY'
  | 'NO_PICKUP'
  | 'UNREACHABLE'
  | 'TOO_FAR';

export type DonationStatus = 'PENDING' | 'PARTIAL' | 'COMPLETED' | 'CLOSED';
export type OfferStatus = 'PENDING' | 'ACCEPTED' | 'DECLINED' | 'WITHDRAWN';

export interface Donor { id: string; name: string; type: string; node: number }
export interface Recipient {
  id: string; name: string; type: string; availableCapacity: number;
  acceptedTypes: Food[]; accepting: boolean; canPickup: boolean; node: number;
}
export interface Donation {
  id: string; donorId: string; food: Food; original: number; remaining: number;
  usableMinutes: number; status: DonationStatus; pendingOffered: number; effectiveRemaining: number;
}
export interface Transaction { id: string; donationId: string; recipientId: string; quantity: number; travelMinutes: number }

/** Optional context that lets a recipient render an offer without reading the donation (see CONTRACT_DEVIATIONS). */
export interface OfferContext {
  food: Food; usableMinutes: number; donorId: string; donorName: string; donorNode: number;
  recipientName: string; recipientNode: number;
}
export interface Offer {
  id: string; donationId: string; recipientId: string; quantity: number; travelMinutes: number;
  status: OfferStatus; txnId: string | null; context?: OfferContext;
}

export interface GeoNodeLL { id: number; lat: number; lng: number; confidence?: string; note?: string }
export interface GeoEdge { u: number; v: number; minutes?: number; geometry?: [number, number][] }
export interface GeoOverlay {
  crs: string; scenario?: string; source?: string; disclaimer?: string; minutesSource?: string;
  nodes: GeoNodeLL[]; edges: GeoEdge[];
}
export interface NetNode {
  id: number; label: string; xKm: number; yKm: number; componentId: number; donors: string[]; recipients: string[];
}
export interface NetEdge { u: number; v: number; minutes: number }
export interface NetComponent { id: number; nodeIds: number[]; hasDonor: boolean; hasRecipient: boolean }
export interface Network { nodes: NetNode[]; edges: NetEdge[]; components: NetComponent[]; geo?: GeoOverlay }

export interface Leg { u: number; v: number; minutes: number }
export interface Route { nodes: number[]; legs: Leg[] }
export interface RouteResult extends Route { minutes: number | null; reachable: boolean }

export type SeparatedBy = 'ACCEPT' | 'TRAVEL' | 'UNUSED' | 'ID' | null;
export interface Candidate {
  rank: number; recipientId: string; accept: number; travelMinutes: number; unusedCapacity: number;
  marginMinutes: number; onBoundary: boolean; separatedFromPrevious: SeparatedBy; route: Route; routeText: string;
}
export interface Rejection {
  recipientId: string; reason: Reason; gate: number; text: string; travelMinutes: number | null;
  reachable: boolean; overByMinutes?: number; route: Route | null;
}
export interface Gate { gate: number; reason: Reason; entering: number; rejected: number }
export interface MatchView {
  donation: {
    id: string; food: Food; original: number; remaining: number; effectiveRemaining: number; pendingOffered: number;
    usableMinutes: number; status: DonationStatus; donorId: string; donorNode: number; hypothetical: boolean;
  };
  totals: { recipients: number; feasible: number; rejected: number; declinedEarlier: number };
  candidates: Candidate[];
  rejected: Rejection[];
  declined: { recipientId: string }[];
  gates: Gate[];
  survivors: number;
  rankingKeys: string[];
  mockNote?: string;
}
/** What a donor gets from /match/preview (privacy-preserving aggregate, ADR-12). */
export interface AggregatePreview {
  feasibleCount: number; bestTravelMinutes: number | null; byReason: Partial<Record<Reason, number>>;
  totalRecipients?: number;
}
export type PreviewResponse = MatchView | AggregatePreview;

export interface NetField { source: number; dist: (number | null)[]; prev: number[]; order: number[]; reachableCount: number }

export interface QueueEntry { donationId: string; usableMinutes: number; effectiveRemaining: number }
export interface QueueResponse { order: QueueEntry[]; nextId: string | null }

export interface CoordinatorStats { openDonations: number; waiting: number; pendingOffers: number; givenOut: number; wasted: number }
export interface DonorStats { donations: number; openDonations: number; pendingOffers: number; givenOut: number }
export interface RecipientStats { incoming: number; received: number }
export interface Summary {
  offered: number; allocated: number; wasted: number; stillOpen: number;
  byFood?: Partial<Record<Food, number>> | number[];
}
export interface Snapshot {
  etag: string; donors: Donor[]; recipients: Recipient[]; donations: Donation[]; transactions: Transaction[];
  offers: Offer[]; summary: Summary;
  /** the gateway names it coordinatorStats (see CONTRACT_DEVIATIONS); doc 05 said stats */
  coordinatorStats?: CoordinatorStats; stats?: CoordinatorStats;
}

export interface Meta {
  engineVersion: string; backendManifest?: unknown; counts?: Record<string, number>; warnings: string[];
  scenario: string; simulated: boolean; capabilities: { geo: boolean }; bootId?: string;
}
export interface Identities {
  donors: { id: string; name: string; type?: string; node?: number }[];
  recipients: { id: string; name: string; type?: string; node?: number }[];
}
export interface Session { role: Role; identityId: string | null; name?: string }

export interface MutationResponse<T = unknown> {
  revision: number; etag: string; message: string; effects?: Record<string, unknown>; data: T;
}
export interface OfferCheck { wouldSucceed: boolean; code?: string; message?: string; details?: { max?: number } }

export interface ApiEvent {
  type: string;
  data: {
    rev?: number; bootId?: string; at?: string; actor?: { role: string; id: string | null }; entity?: { type: string; id: string };
    offerId?: string; donationId?: string; recipientId?: string; quantity?: number; travelMinutes?: number; txnId?: string;
    wasted?: number; state?: string; effects?: unknown;
  };
}

/** Wall-clock timestamps (ms since epoch) kept by the local API beside the engine data; never sent to the engine. */
export interface Activity {
  now: number;
  offers: Record<string, { sentAt?: number; answeredAt?: number; answer?: string }>;
  deliveries: Record<string, { startAt: number; offerId?: string | null }>;
  donations: Record<string, { createdAt?: number; closedAt?: number }>;
}
