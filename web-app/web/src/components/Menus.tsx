// Top-bar menus: the role switcher ("Coordinator ▾" / "Recipient: <name> ▾" / "Donor: <name> ▾") and a small Settings menu
// (data set, reset, leave). Both are plain disclosure popovers: Esc or a click outside closes them.
import { useEffect, useMemo, useRef, useState, type ReactNode } from 'react';
import type { Role } from '../api/types';
import { ApiError } from '../api/client';
import { errorText, useStore } from '../store';
import { go } from './ui';
import { IS_LOCAL } from '../backend';
import { ResetDataDialog, ScenarioSelect } from './LocalControls';
import { SPEED_LABEL, type SimSpeed } from '../lib/simclock';

export const HOME: Record<Role, string> = { coordinator: '/c', donor: '/d', recipient: '/r' };
const ROLE_WORD: Record<Role, string> = { coordinator: 'Coordinator', recipient: 'Recipient', donor: 'Donor' };

function usePopover() {
  const [open, setOpen] = useState(false);
  const box = useRef<HTMLDivElement>(null);
  useEffect(() => {
    if (!open) return;
    const onDown = (e: MouseEvent) => { if (box.current && !box.current.contains(e.target as Node)) setOpen(false); };
    const onKey = (e: KeyboardEvent) => {
      if (e.key !== 'Escape') return;
      setOpen(false);
      (box.current?.querySelector('[data-pop-trigger]') as HTMLElement | null)?.focus();
    };
    document.addEventListener('mousedown', onDown);
    document.addEventListener('keydown', onKey);
    return () => { document.removeEventListener('mousedown', onDown); document.removeEventListener('keydown', onKey); };
  }, [open]);
  return { open, setOpen, box };
}

/** Searchable list of donors or recipients. */
export function IdentityPicker({ role, onPick, busy }: { role: 'donor' | 'recipient'; onPick: (id: string) => void; busy?: boolean }) {
  const identities = useStore((s) => s.identities);
  const [q, setQ] = useState('');
  const list = useMemo(() => {
    const src = role === 'donor' ? identities?.donors ?? [] : identities?.recipients ?? [];
    const n = q.trim().toLowerCase();
    return src.filter((x) => !n || (x.name + ' ' + x.id + ' ' + (x.type ?? '')).toLowerCase().includes(n));
  }, [identities, role, q]);
  const id = `idq-${role}`;
  return (
    <div className="picker">
      <label htmlFor={id} className="sr-only">Search {role}s</label>
      <input id={id} type="search" placeholder={`Search ${role}s by name`} value={q} onChange={(e) => setQ(e.target.value)} autoFocus />
      <ul className="pick-list">
        {list.slice(0, 200).map((x) => (
          <li key={x.id}>
            <button type="button" className="pick" disabled={busy} onClick={() => onPick(x.id)}>
              <span>{x.name}</span>{x.type && <span className="muted small">{x.type.toLowerCase().replace(/_/g, ' ')}</span>}
            </button>
          </li>
        ))}
        {list.length === 0 && <li className="muted small pick-none">No match.</li>}
      </ul>
    </div>
  );
}

/** Sign in as a role (switching in place when someone is already signed in), then go to that role's home. */
export function useEnterRole() {
  const [busy, setBusy] = useState(false);
  const [err, setErr] = useState<string | null>(null);
  const enter = async (role: Role, id?: string) => {
    setBusy(true); setErr(null);
    try {
      if (useStore.getState().session) await useStore.getState().switchTo(role, id);
      else await useStore.getState().signIn(role, id);
      go(HOME[role]);
      return true;
    } catch (e) { setErr(e instanceof ApiError ? errorText(e) : String(e)); return false; }
    finally { setBusy(false); }
  };
  return { enter, busy, err };
}

export function RoleMenu() {
  const session = useStore((s) => s.session);
  const { open, setOpen, box } = usePopover();
  const [picking, setPicking] = useState<'donor' | 'recipient' | null>(null);
  const { enter, busy, err } = useEnterRole();
  useEffect(() => { if (!open) setPicking(null); }, [open]);
  const label = !session ? 'Choose a role' : session.role === 'coordinator' ? 'Coordinator' : `${ROLE_WORD[session.role]}: ${session.name ?? session.identityId ?? ''}`;
  const pick = async (role: Role, id?: string) => { if (await enter(role, id)) setOpen(false); };
  return (
    <div className="pop-host" ref={box}>
      <button type="button" className="menu-btn role-btn" data-pop-trigger aria-expanded={open} aria-haspopup="true" onClick={() => setOpen(!open)}>
        <span className="role-label">{label}</span> <span aria-hidden="true">▾</span>
      </button>
      {open && (
        <div className="popover role-pop" role="group" aria-label="Switch role">
          {!picking ? (
            <>
              <p className="eyebrow">View as</p>
              <ul className="menu-list">
                <li><button type="button" className="menu-item" disabled={busy} aria-current={session?.role === 'coordinator' || undefined} onClick={() => void pick('coordinator')}>Coordinator</button></li>
                <li><button type="button" className="menu-item" aria-current={session?.role === 'recipient' || undefined} onClick={() => setPicking('recipient')}>Recipient…</button></li>
                <li><button type="button" className="menu-item" aria-current={session?.role === 'donor' || undefined} onClick={() => setPicking('donor')}>Donor…</button></li>
              </ul>
            </>
          ) : (
            <>
              <p className="eyebrow"><button type="button" className="link-btn" onClick={() => setPicking(null)}>← Roles</button> · Which {picking}?</p>
              <IdentityPicker role={picking} busy={busy} onPick={(id) => void pick(picking, id)} />
            </>
          )}
          {err && <p className="form-error" role="alert">{err}</p>}
        </div>
      )}
    </div>
  );
}

export function SettingsMenu() {
  const session = useStore((s) => s.session);
  const signOut = useStore((s) => s.signOut);
  const { open, setOpen, box } = usePopover();
  const [resetOpen, setResetOpen] = useState(false);
  const speed = useStore((s) => s.simSpeed);
  const setSpeed = useStore((s) => s.setSimSpeed);
  const items: ReactNode[] = [];
  if (IS_LOCAL) items.push(<li key="scen" className="menu-row"><ScenarioSelect /></li>);
  items.push(
    <li key="clock" className="menu-row">
      <label className="scen-select">
        <span className="eyebrow">Simulated delivery clock</span>
        <select value={speed} onChange={(e) => setSpeed(e.target.value as SimSpeed)}>
          {(Object.keys(SPEED_LABEL) as SimSpeed[]).map((k) => <option key={k} value={k}>{SPEED_LABEL[k]}</option>)}
        </select>
      </label>
    </li>,
  );
  if (IS_LOCAL) items.push(<li key="reset"><button type="button" className="menu-item" onClick={() => { setOpen(false); setResetOpen(true); }}>Reset data…</button></li>);
  if (session) items.push(<li key="out"><button type="button" className="menu-item" onClick={() => { setOpen(false); void signOut(); }}>Back to start</button></li>);
  if (items.length === 0) return null;
  return (
    <div className="pop-host" ref={box}>
      <button type="button" className="menu-btn" data-pop-trigger aria-expanded={open} aria-haspopup="true" onClick={() => setOpen(!open)}>Settings <span aria-hidden="true">▾</span></button>
      {open && <div className="popover settings-pop" role="group" aria-label="Settings"><ul className="menu-list">{items}</ul></div>}
      <ResetDataDialog open={resetOpen} onClose={() => setResetOpen(false)} />
    </div>
  );
}
