// Small display helpers shared by the role screens.
import { memo } from 'react';
import type { DeliveryRow } from '../lib/derive';
import { fmt, isNoExpiry } from '../lib/format';
import { clockTime, deliveryClock, type SimSpeed } from '../lib/simclock';
import { FoodTag, Progress } from '../components/ui';

export const URGENT_MIN = 30;
export const timeLeft = (m: number) => (isNoExpiry(m) ? 'keeps (no expiry)' : `good for ${fmt(m)} min`);
export const isUrgent = (m: number) => !isNoExpiry(m) && m <= URGENT_MIN;

/** One delivery row: who, what, progress on the simulated clock. `perspective` drops the side the viewer already knows. */
export const DeliveryLine = memo(function DeliveryLine({ r, now, speed, href, perspective }: { r: DeliveryRow; now: number; speed: SimSpeed; href?: string; perspective?: 'recipient' | 'donor' }) {
  const c = deliveryClock(r.startAt, r.t.travelMinutes, now, speed);
  const who = perspective === 'recipient' ? `from ${r.donorName}` : perspective === 'donor' ? `to ${r.recipientName}` : `${r.donorName} → ${r.recipientName}`;
  const main = (
    <>
      <span className="row-main">
        <span className="line1"><strong className="num">{fmt(r.t.quantity)}</strong> <FoodTag food={r.food} /> <span className="muted">{who}</span></span>
        <span className="line2 small muted">{r.t.id} · {r.t.travelMinutes} min drive{c.phase === 'onway' && r.startAt ? ` · left ${clockTime(r.startAt)}` : ''}</span>
      </span>
      <span className="row-side">
        {c.phase === 'onway' && <><Progress frac={c.frac} label={`Delivery ${r.t.id} progress`} /><span className="num">{c.leftMin} min left</span></>}
        {c.phase === 'delivered' && <span className="tone-ok">Delivered {c.doneAt ? clockTime(c.doneAt) : ''}</span>}
        {c.phase === 'earlier' && <span className="muted">Delivered earlier</span>}
      </span>
    </>
  );
  return <li>{href ? <a className="row row-link" href={href}>{main}</a> : <div className="row">{main}</div>}</li>;
});
