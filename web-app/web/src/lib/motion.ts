export type MotionLevel = 'full' | 'off';

/** True when animation should be skipped: follows the operating system's prefers-reduced-motion setting. */
export const reduced = (): boolean => document.documentElement.dataset.motion === 'off';

const mq = (): MediaQueryList | null => window.matchMedia?.('(prefers-reduced-motion: reduce)') ?? null;
export function initialMotion(): MotionLevel {
  return mq()?.matches ? 'off' : 'full';
}
let listening = false;
export function applyMotion(m: MotionLevel) {
  document.documentElement.dataset.motion = m;
  const q = mq();
  if (q && !listening) {
    listening = true;
    q.addEventListener?.('change', (e) => { document.documentElement.dataset.motion = e.matches ? 'off' : 'full'; });
  }
}
