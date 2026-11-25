// App shell: one top bar (product, role navigation, role switcher, settings) and lazily loaded pages, one chunk per area.
import { lazy, Suspense, useEffect } from 'react';
import { Banner, Toasts, go, useHashPath } from './components/ui';
import { useStore } from './store';
import Landing, { homeOf } from './screens/Landing';
import { LocalNotice } from './components/LocalControls';
import { RoleMenu, SettingsMenu } from './components/Menus';
import { IS_LOCAL } from './backend';
import type { DonationView } from './screens/coord/Donations';

const Overview = lazy(() => import('./screens/coord/Overview'));
const Donations = lazy(() => import('./screens/coord/Donations'));
const DonationPage = lazy(() => import('./screens/coord/Donation'));
const Offers = lazy(() => import('./screens/coord/Offers'));
const OfferDetail = lazy(() => import('./screens/coord/Offers').then((m) => ({ default: m.OfferDetail })));
const Deliveries = lazy(() => import('./screens/Deliveries'));
const DeliveryDetail = lazy(() => import('./screens/Deliveries').then((m) => ({ default: m.DeliveryDetail })));
const Incoming = lazy(() => import('./screens/Recipient'));
const RecipientDetails = lazy(() => import('./screens/Recipient').then((m) => ({ default: m.RecipientDetails })));
const DonorHome = lazy(() => import('./screens/Donor').then((m) => ({ default: m.DonorHome })));
const DonorNew = lazy(() => import('./screens/Donor').then((m) => ({ default: m.DonorNew })));

const NAV: Record<'coordinator' | 'recipient' | 'donor', { href: string; label: string }[]> = {
  coordinator: [{ href: '/c', label: 'Overview' }, { href: '/c/donations', label: 'Donations' }, { href: '/c/offers', label: 'Offers' }, { href: '/c/deliveries', label: 'Deliveries' }],
  recipient: [{ href: '/r', label: 'Incoming offers' }, { href: '/r/deliveries', label: 'Deliveries' }, { href: '/r/profile', label: 'My details' }],
  donor: [{ href: '/d', label: 'My donations' }, { href: '/d/new', label: 'Add donation' }],
};

function TopBar({ path }: { path: string }) {
  const session = useStore((s) => s.session);
  const nav = session ? NAV[session.role] : [];
  // the longest nav prefix of the current path is the current section
  const cur = nav.filter((n) => path === n.href || path.startsWith(n.href + '/')).sort((a, b) => b.href.length - a.href.length)[0]?.href;
  return (
    <header className="topbar">
      <a className="brand" href={session ? '#' + homeOf(session.role) : '#/'} aria-label="CommunityBridge home">
        <svg viewBox="0 0 32 32" width="22" height="22" aria-hidden="true"><path d="M8 21 L24 11" stroke="currentColor" strokeWidth="2.5" strokeLinecap="round" /><rect x="4" y="17" width="8" height="8" rx="1.5" fill="currentColor" /><circle cx="24" cy="11" r="4.5" fill="none" stroke="currentColor" strokeWidth="2.5" /></svg>
        <span>CommunityBridge</span>
      </a>
      {nav.length > 0 && <nav className="topnav" aria-label="Main">{nav.map((n) => <a key={n.href} href={'#' + n.href} aria-current={cur === n.href ? 'page' : undefined}>{n.label}</a>)}</nav>}
      <div className="topright">
        {session && <RoleMenu />}
        <SettingsMenu />
      </div>
    </header>
  );
}

function coordinatorPage(seg: string[]) {
  const [, a, b] = seg;
  if (a === 'donations') {
    if (!b || ['offered', 'done', 'closed'].includes(b)) return <Donations view={(b ?? 'assign') as DonationView} />;
    return <DonationPage key={b} donationId={b} />;
  }
  if (a === 'match' && b) return <DonationPage key={b} donationId={b} />; // old links
  if (a === 'offers') return !b ? <Offers view="waiting" /> : b === 'history' ? <Offers view="history" /> : <OfferDetail key={b} offerId={b} />;
  if (a === 'deliveries') return !b ? <Deliveries view="active" role="coordinator" /> : b === 'done' ? <Deliveries view="done" role="coordinator" /> : <DeliveryDetail key={b} txnId={b} />;
  return <Overview />;
}

function Routes({ path }: { path: string }) {
  const role = useStore((s) => s.session)!.role;
  const seg = path.split('/').filter(Boolean);
  const area = seg[0];
  const want = role === 'coordinator' ? 'c' : role === 'donor' ? 'd' : 'r';
  useEffect(() => { if (area && area !== want) go(homeOf(role)); }, [area, want, role]);
  useEffect(() => { window.scrollTo(0, 0); }, [path]);
  if (!area) return <Landing />;
  if (area !== want) return null;
  if (role === 'coordinator') return coordinatorPage(seg);
  if (role === 'donor') return seg[1] === 'new' ? <DonorNew /> : <DonorHome />;
  if (seg[1] === 'deliveries') return <Deliveries view={seg[2] === 'done' ? 'done' : 'active'} role="recipient" />;
  if (seg[1] === 'profile') return <RecipientDetails />;
  return <Incoming />;
}

export default function App() {
  const phase = useStore((s) => s.phase);
  const fatal = useStore((s) => s.fatal);
  const session = useStore((s) => s.session);
  const conn = useStore((s) => s.conn);
  const engineOk = useStore((s) => s.engineOk);
  const refreshError = useStore((s) => s.refreshError);
  const path = useHashPath();
  useEffect(() => { void useStore.getState().init(); }, []);

  if (phase === 'boot') return <div className="boot" role="status">Loading CommunityBridge…</div>;
  if (phase === 'fatal') return (
    <div className="page"><Banner tone="error" action={<button className="btn" onClick={() => location.reload()}>Retry</button>}>{IS_LOCAL ? 'Could not start the in-browser engine' : 'Could not reach the server'}: {fatal}</Banner></div>
  );
  const degraded = session && (conn === 'reconnecting' || !engineOk);
  return (
    <>
      <a className="skip" href="#main" onClick={(e) => { e.preventDefault(); document.getElementById('main')?.focus(); }}>Skip to content</a>
      <TopBar path={path} />
      {degraded && <Banner tone="warn">Reconnecting… showing the last known data (read-only).</Banner>}
      <LocalNotice />
      {refreshError && session && <Banner tone="error">Could not refresh data: {refreshError}</Banner>}
      <main id="main" tabIndex={-1}>
        <Suspense fallback={<div className="page"><div className="skeleton" style={{ height: 240 }} /></div>}>
          {session ? <Routes path={path} /> : <Landing />}
        </Suspense>
      </main>
      <Toasts />
    </>
  );
}
