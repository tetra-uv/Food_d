// Simulated delivery clock ("the fiction"). The engine records an accepted offer as a transaction with travel minutes but
// has no clock. The UI animates the delivery from the local activity log's startAt: progress = elapsed / travelMinutes,
// where elapsed runs on a simulated clock (default 1 real second = 1 simulated minute). Nothing here goes back to the engine.
import { useEffect, useState } from 'react';

export type SimSpeed = 'fast' | 'real';
export const SPEED_LABEL: Record<SimSpeed, string> = { fast: '1 second = 1 minute', real: 'Real time' };
const FACTOR: Record<SimSpeed, number> = { fast: 60, real: 1 };

export function readSpeed(): SimSpeed {
  try { return localStorage.getItem('cb-sim-speed') === 'real' ? 'real' : 'fast'; } catch { return 'fast'; }
}

export type DeliveryPhase = 'earlier' | 'onway' | 'delivered';
export interface DeliveryClock { phase: DeliveryPhase; frac: number; leftMin: number; elapsedMin: number; doneAt: number | null }

/** Simulated minutes elapsed since startAt (real ms timestamps). */
export const simMinutes = (startAt: number, now: number, speed: SimSpeed) => (Math.max(0, now - startAt) / 60000) * FACTOR[speed];
/** Real ms timestamp when a delivery of `travel` simulated minutes started at startAt arrives. */
export const arrivalAt = (startAt: number, travel: number, speed: SimSpeed) => startAt + (travel * 60000) / FACTOR[speed];

export function deliveryClock(startAt: number | undefined, travel: number, now: number, speed: SimSpeed): DeliveryClock {
  if (startAt === undefined) return { phase: 'earlier', frac: 1, leftMin: 0, elapsedMin: travel, doneAt: null };
  const el = simMinutes(startAt, now, speed);
  const done = el >= travel;
  return { phase: done ? 'delivered' : 'onway', frac: travel <= 0 ? 1 : Math.min(el / travel, 1), leftMin: Math.max(0, Math.ceil(travel - el)), elapsedMin: el, doneAt: arrivalAt(startAt, travel, speed) };
}

/** Current time, re-rendering every `ms` while `active` and the tab is visible (paused when hidden). */
export function useNow(ms: number, active = true): number {
  const [now, setNow] = useState(() => Date.now());
  useEffect(() => {
    if (!active) return;
    let t: ReturnType<typeof setInterval> | null = null;
    const start = () => { if (!t) { setNow(Date.now()); t = setInterval(() => setNow(Date.now()), ms); } };
    const stop = () => { if (t) { clearInterval(t); t = null; } };
    const vis = () => (document.hidden ? stop() : start());
    vis();
    document.addEventListener('visibilitychange', vis);
    return () => { stop(); document.removeEventListener('visibilitychange', vis); };
  }, [ms, active]);
  return now;
}

const tf = new Intl.DateTimeFormat('en-GB', { hour: '2-digit', minute: '2-digit' });
export const clockTime = (t: number) => tf.format(t);
/** "just now", "4 min", "2 h 5 min" (real time since t) */
export function ago(t: number, now: number): string {
  const m = Math.floor((now - t) / 60000);
  if (m < 1) return 'under a minute';
  if (m < 60) return `${m} min`;
  return `${Math.floor(m / 60)} h ${m % 60} min`;
}
