import { useCallback, useEffect, useRef, useState, type ReactNode } from 'react';
import type { DonationStatus, Food, OfferStatus } from '../api/types';
import { DONATION_STATUS, FOOD_META, GLYPHS, OFFER_STATUS } from '../lib/meta';
import { useStore } from '../store';

export function Glyph({ name, size = 16, className = '' }: { name: string; size?: number; className?: string }) {
  return (
    <svg className={`glyph ${className}`} viewBox="0 0 24 24" width={size} height={size} fill="none" stroke="currentColor" strokeWidth="1.75" strokeLinecap="round" strokeLinejoin="round" aria-hidden="true" dangerouslySetInnerHTML={{ __html: GLYPHS[name] ?? '' }} />
  );
}

export function FoodTag({ food }: { food: Food | null | undefined; plain?: boolean }) {
  if (!food) return null;
  return <span className="tag"><Glyph name={food} /> {FOOD_META[food].label.toLowerCase()}</span>;
}

export function StatusChip({ kind, status }: { kind: 'donation' | 'offer'; status: DonationStatus | OfferStatus }) {
  const m = kind === 'donation' ? DONATION_STATUS[status as DonationStatus] : OFFER_STATUS[status as OfferStatus];
  return (
    <span className={`status tone-${m.tone}`}>
      <Glyph name={status} size={14} /> {m.label}
    </span>
  );
}

export function Banner({ tone = 'info', children, action }: { tone?: 'info' | 'warn' | 'error'; children: ReactNode; action?: ReactNode }) {
  return (
    <div className={`banner b-${tone}`} role={tone === 'error' ? 'alert' : 'status'}>
      <Glyph name={tone === 'info' ? 'INFO' : 'WARN'} />
      <div className="banner-t">{children}</div>
      {action}
    </div>
  );
}

/** Empty state: what is (not) here and the next action. */
export function Empty({ title, children }: { title: string; children?: ReactNode }) {
  return (
    <div className="empty">
      <p>{title}</p>
      {children && <div className="empty-next">{children}</div>}
    </div>
  );
}

/** Page template: title, one-line summary, content. `wide` pages put a map beside the content. */
export function Page({ title, summary, crumb, children, aside }: { title: ReactNode; summary?: ReactNode; crumb?: { href: string; label: string }; children: ReactNode; aside?: ReactNode }) {
  const body = (
    <div className="page-body">
      {crumb && <p className="crumb"><a href={crumb.href}>← {crumb.label}</a></p>}
      <h1>{title}</h1>
      {summary && <p className="summary">{summary}</p>}
      {children}
    </div>
  );
  if (!aside) return <div className="page">{body}</div>;
  // the map is a collapsible section on phones (summary hidden from 768 px up, where it always shows)
  return (
    <div className="page split">
      <MapSection>{aside}</MapSection>
      {body}
    </div>
  );
}

/** Map column. Below 768 px it can be collapsed (the toggle is hidden on larger screens, where the map always shows). */
function MapSection({ children }: { children: ReactNode }) {
  const [open, setOpen] = useState(true);
  return (
    <section className={`split-map${open ? '' : ' collapsed'}`} aria-label="Map">
      <button type="button" className="map-toggle link-btn" aria-expanded={open} onClick={() => setOpen(!open)}>{open ? 'Hide map' : 'Show map'}</button>
      {children}
    </section>
  );
}

/** Tabs as links (each tab is its own URL, so back/forward and reload keep the view). */
export function Tabs({ tabs, current }: { tabs: { href: string; label: string; count?: number }[]; current: string }) {
  return (
    <nav className="tabs" aria-label="Views">
      {tabs.map((t) => (
        <a key={t.href} href={'#' + t.href} aria-current={t.href === current ? 'page' : undefined}>
          {t.label}{t.count !== undefined && <span className="tab-n"> {t.count}</span>}
        </a>
      ))}
    </nav>
  );
}

/** Long lists render in pages ("Show N more"). */
export function useShowMore<T>(rows: T[], first = 25): { shown: T[]; more: ReactNode } {
  const [n, setN] = useState(first);
  const shown = rows.length > n ? rows.slice(0, n) : rows;
  const left = rows.length - shown.length;
  return { shown, more: left > 0 ? <button type="button" className="link-btn more" onClick={() => setN(n + 50)}>Show {Math.min(50, left)} more of {left}</button> : null };
}

export interface Step { label: string; at?: string; state: 'done' | 'now' | 'todo' | 'stop' }
/** Horizontal status timeline (offered → accepted → on the way → delivered). */
export function Timeline({ steps, label }: { steps: Step[]; label: string }) {
  return (
    <ol className="timeline" aria-label={label}>
      {steps.map((s, i) => (
        <li key={i} className={`ts-${s.state}`} aria-current={s.state === 'now' ? 'step' : undefined}>
          <span className="ts-dot" aria-hidden="true" />
          <span className="ts-l">{s.label}</span>
          {s.at && <span className="ts-at">{s.at}</span>}
        </li>
      ))}
    </ol>
  );
}

export function Progress({ frac, label }: { frac: number; label: string }) {
  const pct = Math.round(Math.min(Math.max(frac, 0), 1) * 100);
  return <span className="bar" role="progressbar" aria-label={label} aria-valuenow={pct} aria-valuemin={0} aria-valuemax={100}><span style={{ width: pct + '%' }} /></span>;
}

/** Native <dialog>: modal, focus-trapped and Esc-closable by the browser. Content mounts only while open. */
export function Modal({ open, onClose, title, children, wide }: { open: boolean; onClose: () => void; title: string; children: ReactNode; wide?: boolean }) {
  const ref = useRef<HTMLDialogElement>(null);
  useEffect(() => {
    const d = ref.current;
    if (!d) return;
    if (open && !d.open) d.showModal();
    if (!open && d.open) d.close();
  }, [open]);
  const id = useRef('dlg' + Math.random().toString(36).slice(2, 7)).current;
  return (
    <dialog ref={ref} className={`modal ${wide ? 'wide' : ''}`} aria-labelledby={id} onClose={onClose} onCancel={(e) => { e.preventDefault(); onClose(); }}
      onClick={(e) => { if (e.target === ref.current) onClose(); }}>
      {open && (
        <div className="modal-in">
          <h2 id={id}>{title}</h2>
          {children}
        </div>
      )}
    </dialog>
  );
}

export function ConfirmDialog({ open, title, body, confirmLabel, danger, busy, error, onConfirm, onClose }: {
  open: boolean; title: string; body: ReactNode; confirmLabel: string; danger?: boolean; busy?: boolean; error?: string | null;
  onConfirm: () => void; onClose: () => void;
}) {
  return (
    <Modal open={open} onClose={onClose} title={title}>
      <div className="modal-body">{body}</div>
      {error && <p className="form-error" role="alert">{error}</p>}
      <div className="modal-actions">
        <button type="button" className="btn" onClick={onClose} autoFocus>Cancel</button>
        <button type="button" className={`btn ${danger ? 'danger' : 'primary'}`} onClick={onConfirm} disabled={busy}>{busy ? 'Working…' : confirmLabel}</button>
      </div>
    </Modal>
  );
}

export function Toasts() {
  const toasts = useStore((s) => s.toasts);
  const dismiss = useStore((s) => s.dismissToast);
  return (
    <div className="toasts" aria-live="polite" aria-atomic="false">
      {toasts.map((t) => <ToastItem key={t.id} id={t.id} tone={t.tone} text={t.text} action={t.action} onDone={dismiss} />)}
    </div>
  );
}

function ToastItem({ id, tone, text, action, onDone }: { id: number; tone: string; text: string; action?: { label: string; run: () => void }; onDone: (id: number) => void }) {
  const [hover, setHover] = useState(false);
  useEffect(() => {
    if (hover) return;
    const t = setTimeout(() => onDone(id), action ? 9000 : tone === 'error' ? 8000 : 5000);
    return () => clearTimeout(t);
  }, [hover, id, onDone, action, tone]);
  return (
    <div className={`toast t-${tone}`} onMouseEnter={() => setHover(true)} onMouseLeave={() => setHover(false)} role={tone === 'error' ? 'alert' : undefined}>
      <Glyph name={tone === 'error' ? 'WARN' : tone === 'ok' ? 'CHECK' : 'INFO'} />
      <span className="toast-t">{text}</span>
      {action && <button type="button" className="link-btn" onClick={() => { action.run(); onDone(id); }}>{action.label}</button>}
      <button type="button" className="icon-btn" aria-label="Dismiss notification" onClick={() => onDone(id)}>×</button>
    </div>
  );
}

// ---------------- hash router ----------------
export function useHashPath(): string {
  const read = () => (location.hash.replace(/^#/, '') || '/');
  const [p, setP] = useState(read);
  useEffect(() => {
    const on = () => setP(read());
    window.addEventListener('hashchange', on);
    return () => window.removeEventListener('hashchange', on);
  }, []);
  return p;
}
export const go = (path: string) => { location.hash = '#' + path; };

export function useDebounced<T>(value: T, ms: number): T {
  const [v, setV] = useState(value);
  useEffect(() => { const t = setTimeout(() => setV(value), ms); return () => clearTimeout(t); }, [value, ms]);
  return v;
}

/** Number input + stepper clamped to [min,max]. The typed text is the source of truth: every keystroke that parses to a
 *  number calls onChange with the clamped value (so labels and the value that is sent always agree); blur/Enter rewrites
 *  the text to that clamped value. */
export function NumberField({ id, label, value, min, max, onChange, hint, suffix }: {
  id: string; label: string; value: number; min: number; max: number; onChange: (n: number) => void; hint?: string; suffix?: string;
}) {
  const clamp = useCallback((n: number) => Math.min(Math.max(Math.round(n), min), max), [min, max]);
  const parse = (raw: string): number | null => {
    const t = raw.replace(/[,\s]/g, '');
    if (t === '' || t === '-') return null;
    const n = Number(t);
    return Number.isFinite(n) ? n : null;
  };
  const [text, setText] = useState(String(value));
  // follow outside changes (stepper, reset, new candidate) unless the text already means this value
  useEffect(() => {
    setText((cur) => { const n = parse(cur); return n !== null && clamp(n) === value ? cur : String(value); });
  }, [value, clamp]);
  const edit = (raw: string) => {
    setText(raw);
    const n = parse(raw);
    if (n !== null) { const c = clamp(n); if (c !== value) onChange(c); }
  };
  const commit = () => {
    const n = parse(text);
    const c = n === null ? value : clamp(n);
    if (c !== value) onChange(c);
    setText(String(c));
  };
  return (
    <div className="field">
      <label htmlFor={id}>{label}</label>
      <div className="stepper">
        <button type="button" className="step" aria-label={`Decrease ${label}`} onClick={() => onChange(Math.max(min, value - 1))}>−</button>
        <input id={id} inputMode="numeric" value={text} onChange={(e) => edit(e.target.value)} onBlur={commit} onKeyDown={(e) => { if (e.key === 'Enter') commit(); }} aria-describedby={hint ? id + '-h' : undefined} />
        <button type="button" className="step" aria-label={`Increase ${label}`} onClick={() => onChange(Math.min(max, value + 1))}>+</button>
        {suffix && <span className="suffix">{suffix}</span>}
      </div>
      {hint && <div className="hint" id={id + '-h'}>{hint}</div>}
    </div>
  );
}
