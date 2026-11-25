import { memo } from 'react';
import type { Donation } from '../../api/types';
import { Empty, FoodTag, Page, Tabs, useShowMore } from '../../components/ui';
import { useDonationGroups, useLookups } from '../../lib/derive';
import { fmt } from '../../lib/format';
import { useStore } from '../../store';
import { isUrgent, timeLeft } from '../common';

export type DonationView = 'assign' | 'offered' | 'done' | 'closed';
const TITLE: Record<DonationView, string> = { assign: 'Needs assignment', offered: 'Fully offered', done: 'Done', closed: 'Closed' };
const NOTE: Record<DonationView, string> = {
  assign: 'Food that spoils first is at the top (the engine’s urgency order).',
  offered: 'All food is in offers waiting for an answer.',
  done: 'Everything was given out.',
  closed: 'Closed by the coordinator; what was left is recorded as wasted.',
};
const EMPTY: Record<DonationView, string> = {
  assign: 'Every donation has been assigned.', offered: 'No donation is waiting only on answers.', done: 'No donation is fully given out yet.', closed: 'No donation has been closed.',
};

const Row = memo(function Row({ d, view, donor }: { d: Donation; view: DonationView; donor: string }) {
  const side = view === 'assign' ? timeLeft(d.usableMinutes)
    : view === 'offered' ? `${fmt(d.pendingOffered)} in offers`
      : view === 'done' ? `${fmt(d.original)} given`
        : `${fmt(d.remaining)} wasted`;
  return (
    <li>
      <a className="row row-link" href={`#/c/donations/${d.id}`}>
        <span className="row-main">
          <span className="line1"><strong className="num">{fmt(view === 'assign' ? d.effectiveRemaining : d.original)}</strong> <FoodTag food={d.food} /> <span className="muted">from {donor}</span></span>
          <span className="line2 small muted">{d.id}{view === 'assign' && d.effectiveRemaining !== d.original ? ` · ${fmt(d.effectiveRemaining)} left of ${fmt(d.original)}` : ''}{view === 'assign' && d.pendingOffered > 0 ? ` · ${fmt(d.pendingOffered)} in offers` : ''}</span>
        </span>
        <span className={`row-side ${view === 'assign' && isUrgent(d.usableMinutes) ? 'tone-warn' : 'muted'}`}>{side}</span>
      </a>
    </li>
  );
});

export default function Donations({ view }: { view: DonationView }) {
  const loaded = useStore((s) => s.loadedOnce);
  const g = useDonationGroups();
  const L = useLookups();
  const rows = g[view];
  const { shown, more } = useShowMore(rows, 30);
  return (
    <Page title="Donations" summary={`${NOTE[view]}`}>
      <Tabs current={view === 'assign' ? '/c/donations' : `/c/donations/${view}`} tabs={[
        { href: '/c/donations', label: TITLE.assign, count: g.assign.length },
        { href: '/c/donations/offered', label: TITLE.offered, count: g.offered.length },
        { href: '/c/donations/done', label: TITLE.done, count: g.done.length },
        { href: '/c/donations/closed', label: TITLE.closed, count: g.closed.length },
      ]} />
      {!loaded ? <div className="skeleton" style={{ height: 300 }} /> : rows.length === 0 ? (
        <Empty title={EMPTY[view]}>{view !== 'assign' && g.assign.length > 0 && <a href="#/c/donations">{g.assign.length} donations need assignment →</a>}</Empty>
      ) : (
        <>
          <ul className="rows multi">{shown.map((d) => <Row key={d.id} d={d} view={view} donor={L.donorName(d.donorId)} />)}</ul>
          {more}
        </>
      )}
    </Page>
  );
}
