// Semantics from doc 08: colour + glyph + word, never colour alone.
import type { DonationStatus, Food, OfferStatus, Reason } from '../api/types';

const S = (inner: string) => inner; // marker for readability: 24px grid, stroke 1.75, round caps

export const GLYPHS: Record<string, string> = {
  COOKED: S('<path d="M4 12h16c0 4.4-3.6 8-8 8s-8-3.6-8-8z"/><path d="M9 4c-1 1.5 1 2.5 0 4M14 4c-1 1.5 1 2.5 0 4"/>'),
  PACKAGED: S('<path d="M3 7l9-4 9 4v10l-9 4-9-4z"/><path d="M3 7l9 4 9-4M12 11v10"/>'),
  PRODUCE: S('<path d="M5 19C5 9 11 4 20 4c0 9-5 15-15 15z"/><path d="M5 19L14 10"/>'),
  BAKERY: S('<path d="M4 15c0-4 3-7 8-7s8 3 8 7v3H4z"/><path d="M9 12l1 3M13 11l1 3M17 13l.5 2"/>'),
  OK: S('<circle cx="12" cy="12" r="9"/><path d="M8 12.5l3 3 5-6"/>'),
  NOT_ACCEPTING: S('<circle cx="12" cy="12" r="8"/><path d="M6.3 17.7L17.7 6.3"/>'),
  FOOD_INCOMPATIBLE: S('<path d="M6 6l12 12M18 6L6 18"/>'),
  NO_CAPACITY: S('<rect x="3" y="8" width="18" height="8" rx="2"/><path d="M6 11v2"/>'),
  NO_PICKUP: S('<path d="M2 7h11v9H2zM13 10h4l3 3v3h-7"/><circle cx="6" cy="18" r="1.4"/><circle cx="16" cy="18" r="1.4"/><path d="M3 3l18 18"/>'),
  UNREACHABLE: S('<path d="M10 14l-2.5 2.5a3.2 3.2 0 0 1-4.5-4.5L6 9M14 10l2.5-2.5a3.2 3.2 0 0 1 4.5 4.5L18 15"/><path d="M9 4v2M4 8h2M15 20v-2M20 16h-2"/>'),
  TOO_FAR: S('<circle cx="12" cy="12" r="9"/><path d="M12 7v5l3 2"/><path d="M4 20L20 4"/>'),
  PENDING: S('<circle cx="12" cy="12" r="9"/><path d="M12 7v5l3 2"/>'),
  ACCEPTED: S('<circle cx="12" cy="12" r="9"/><path d="M8 12.5l3 3 5-6"/>'),
  DECLINED: S('<circle cx="12" cy="12" r="9"/><path d="M9 9l6 6M15 9l-6 6"/>'),
  WITHDRAWN: S('<path d="M9 7L4 12l5 5"/><path d="M4 12h10a6 6 0 0 1 6 6"/>'),
  PARTIAL: S('<circle cx="12" cy="12" r="9"/><path d="M12 3a9 9 0 0 1 0 18z" fill="currentColor"/>'),
  COMPLETED: S('<circle cx="12" cy="12" r="9"/><path d="M8 12.5l3 3 5-6"/>'),
  CLOSED: S('<rect x="5" y="5" width="14" height="14" rx="2"/><path d="M9 9l6 6M15 9l-6 6"/>'),
  WARN: S('<path d="M12 3l10 18H2z"/><path d="M12 10v5M12 18v.5"/>'),
  INFO: S('<circle cx="12" cy="12" r="9"/><path d="M12 11v6M12 7.5v.5"/>'),
  CHECK: S('<path d="M5 12.5l4.5 4.5L19 7"/>'),
  FLAG: S('<path d="M6 21V4M6 5h11l-2 4 2 4H6"/>'),
};

export const FOOD_META: Record<Food, { label: string; color: string }> = {
  COOKED: { label: 'Cooked', color: 'inherit' },
  PACKAGED: { label: 'Packaged', color: 'inherit' },
  PRODUCE: { label: 'Produce', color: 'inherit' },
  BAKERY: { label: 'Bakery', color: 'inherit' },
};

export interface ReasonMeta { label: string; color: string; hex: string; dash: string; gateName: string }
export const REASON_META: Record<Reason, ReasonMeta> = {
  NOT_ACCEPTING: { label: 'Not accepting', color: 'var(--text-2)', hex: '#A8432A', dash: '2 5', gateName: 'Accepting food' },
  FOOD_INCOMPATIBLE: { label: 'Food type not taken', color: 'var(--text-2)', hex: '#A8432A', dash: '6 3 1 3', gateName: 'Takes this food' },
  NO_CAPACITY: { label: 'No room', color: 'var(--text-2)', hex: '#A8432A', dash: '8 4', gateName: 'Has room' },
  NO_PICKUP: { label: "Can't collect", color: 'var(--text-2)', hex: '#A8432A', dash: '3 3', gateName: 'Can collect' },
  UNREACHABLE: { label: 'No road', color: 'var(--text-2)', hex: '#A8432A', dash: '1 6', gateName: 'Road exists' },
  TOO_FAR: { label: 'Too far', color: 'var(--text-2)', hex: '#A8432A', dash: '10 3', gateName: 'Arrives in time' },
};
export const REASON_ORDER: Reason[] = ['NOT_ACCEPTING', 'FOOD_INCOMPATIBLE', 'NO_CAPACITY', 'NO_PICKUP', 'UNREACHABLE', 'TOO_FAR'];

export interface StatusMeta { label: string; tone: 'ok' | 'warn' | 'plain' | 'muted' | 'danger' }
export const DONATION_STATUS: Record<DonationStatus, StatusMeta> = {
  PENDING: { label: 'Waiting', tone: 'warn' },
  PARTIAL: { label: 'Partly given', tone: 'plain' },
  COMPLETED: { label: 'Given', tone: 'ok' },
  CLOSED: { label: 'Closed', tone: 'muted' },
};
export const OFFER_STATUS: Record<OfferStatus, StatusMeta> = {
  PENDING: { label: 'Pending', tone: 'plain' },
  ACCEPTED: { label: 'Accepted', tone: 'ok' },
  DECLINED: { label: 'Declined', tone: 'muted' },
  WITHDRAWN: { label: 'Withdrawn', tone: 'muted' },
};

export const SEPARATED_TEXT = {
  ACCEPT: 'accepts',
  TRAVEL: 'travel time',
  UNUSED: 'spare capacity',
  ID: 'id',
} as const;
