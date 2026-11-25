import { useState } from 'react';
import type { Role } from '../api/types';
import { Banner } from '../components/ui';
import { useStore } from '../store';
import { ScenarioSelect } from '../components/LocalControls';
import { HOME, IdentityPicker, useEnterRole } from '../components/Menus';
import { IS_LOCAL } from '../backend';

export const homeOf = (r: Role) => HOME[r];

const ROLES: { role: Role; title: string; text: string }[] = [
  { role: 'coordinator', title: 'Coordinator', text: 'Decide where each donation goes, starting with the food that spoils first.' },
  { role: 'recipient', title: 'Recipient', text: 'Answer food offers for your kitchen or shelter and keep your details current.' },
  { role: 'donor', title: 'Donor', text: 'Add surplus food and see right away how many places could take it in time.' },
];

export default function Landing() {
  const session = useStore((s) => s.session);
  const [picking, setPicking] = useState<'donor' | 'recipient' | null>(null);
  const { enter, busy, err } = useEnterRole();

  return (
    <div className="landing">
      <div className="landing-intro">
      <h1>CommunityBridge</h1>
      <p className="lede">Surplus food goes to the nearby kitchens and shelters that can use it before it spoils.</p>
      <p className="muted prose">A coordinator assigns each donation to the recipient the matching engine ranks best; the recipient accepts or declines; the food is then on its way. Pick a role to try it.</p>
      <div className="landing-scen"><ScenarioSelect /></div>
      <footer className="landing-foot small muted">
        <p>Imitation of Delhi: roads and places from OpenStreetMap; names, capacities and donations are simulated. Map data © OpenStreetMap contributors.</p>
        {IS_LOCAL && <p>Everything runs in this browser; your changes are saved only here.</p>}
      </footer>
      </div>

      <div className="landing-roles">
      {session && <p className="continue">You are signed in as {session.name ?? session.role}. <a href={'#' + HOME[session.role]}>Continue →</a></p>}
      {err && <Banner tone="error">{err}</Banner>}

      <h2 className="eyebrow">Continue as</h2>
      <ul className="role-list">
        {ROLES.map((r) => (
          <li key={r.role}>
            <div className="role-row">
              <div>
                <h3>{r.title}</h3>
                <p className="muted">{r.text}</p>
              </div>
              {r.role === 'coordinator'
                ? <button type="button" className="btn" disabled={busy} onClick={() => void enter('coordinator')}>Continue</button>
                : <button type="button" className="btn" aria-expanded={picking === r.role} onClick={() => setPicking(picking === r.role ? null : r.role as 'donor' | 'recipient')}>{picking === r.role ? 'Close' : 'Choose…'}</button>}
            </div>
            {picking && picking === r.role && <IdentityPicker role={picking} busy={busy} onPick={(id) => void enter(picking, id)} />}
          </li>
        ))}
      </ul>
      </div>
    </div>
  );
}
