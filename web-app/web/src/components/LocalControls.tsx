// Controls that exist only in local mode (VITE_BACKEND=local): data-set (scenario) picker, Reset data, storage notices.
// In http mode these render nothing (the gateway decides the scenario).
import { useEffect, useState } from 'react';
import { IS_LOCAL } from '../backend';
import { Banner, ConfirmDialog } from './ui';
import { useStore } from '../store';
import type { LocalStatus } from '../local/engineHost';

let statusP: Promise<LocalStatus> | null = null;
export function useLocalStatus(): LocalStatus | null {
  const [st, setSt] = useState<LocalStatus | null>(null);
  const seq = useStore((s) => s.eventSeq);
  useEffect(() => {
    if (!IS_LOCAL) return;
    statusP = import('../local/api').then((m) => m.localStatus());
    let live = true;
    statusP.then((s) => { if (live) setSt(s); }).catch(() => undefined);
    return () => { live = false; };
  }, [seq]);
  return st;
}

async function pick(name: string) { (await import('../local/scenarios')).switchScenario(name); }

/** One quiet select. Switching reloads the page with the other data set. */
export function ScenarioSelect() {
  const st = useLocalStatus();
  if (!IS_LOCAL || !st || st.manifest.scenarios.length < 2) return null;
  return (
    <label className="scen-select">
      <span className="eyebrow">Data set</span>
      <select value={st.scenario.name} onChange={(e) => void pick(e.target.value)} title="Switch data set (reloads the page)">
        {st.manifest.scenarios.map((s) => <option key={s.name} value={s.name}>{s.label}</option>)}
      </select>
    </label>
  );
}

export function ResetDataDialog({ open, onClose }: { open: boolean; onClose: () => void }) {
  const st = useLocalStatus();
  const [busy, setBusy] = useState(false);
  const [err, setErr] = useState<string | null>(null);
  useEffect(() => { if (open) setErr(null); }, [open]);
  if (!IS_LOCAL || !st) return null;
  const doReset = async () => {
    setBusy(true); setErr(null);
    try {
      await (await import('../local/api')).resetLocalData();
      onClose();
      useStore.getState().toast({ tone: 'ok', text: `Data reset to the original ${st.scenario.label} files` });
      void useStore.getState().refresh();
    } catch (e) { setErr(String((e as Error).message ?? e)); }
    finally { setBusy(false); }
  };
  return (
    <ConfirmDialog open={open} title="Reset data?" danger confirmLabel="Reset to original data" busy={busy} error={err} onClose={onClose} onConfirm={() => void doReset()}
      body={<p>This deletes every change saved in this browser for <strong>{st.scenario.label}</strong> ({st.revision} saved change{st.revision === 1 ? '' : 's'}) and reloads the original files. Other open tabs follow. This cannot be undone.</p>} />
  );
}

export function LocalNotice() {
  const st = useLocalStatus();
  if (!IS_LOCAL || !st || st.notes.length === 0) return null;
  return <Banner tone="info">{st.notes.join(' ')}</Banner>;
}
