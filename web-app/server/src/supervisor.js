// Engine supervisor: one cb_engine.exe child process, strictly serialised commands (FIFO), per-command
// deadlines, restart with backoff, newline-delimited UTF-8 JSON. Arguments are always passed as an array
// (the project path contains a space), never through a shell.
import { spawn } from 'node:child_process';
import { EventEmitter } from 'node:events';

const BACKOFF_MS = [250, 1000, 4000];
const CRASH_WINDOW_MS = 60_000;
const MAX_CRASHES_IN_WINDOW = 5;

/** A failure that is not an engine answer (engine down, deadline, crash during a write). */
export class EngineFailure extends Error {
  constructor(code, message, details) {
    super(message);
    this.code = code;
    this.details = details || {};
  }
}

export class Supervisor extends EventEmitter {
  /**
   * @param {{exe:string, dataDir:string, log?:(level:string,msg:string,extra?:object)=>void,
   *          readDeadlineMs?:number, writeDeadlineMs?:number, queueWaitMs?:number, bootDeadlineMs?:number}} opts
   */
  constructor(opts) {
    super();
    this.exe = opts.exe;
    this.dataDir = opts.dataDir;
    this.log = opts.log || (() => {});
    this.readDeadlineMs = opts.readDeadlineMs ?? 2000;
    this.writeDeadlineMs = opts.writeDeadlineMs ?? 10000;
    this.queueWaitMs = opts.queueWaitMs ?? 5000;
    this.bootDeadlineMs = opts.bootDeadlineMs ?? 15000;

    /** STOPPED | STARTING | READY | RESTARTING | DEGRADED | FAILED_BOOT */
    this.state = 'STOPPED';
    this.proc = null;
    this.pid = null;
    this.startedAt = 0;
    this.bootId = null;
    this.rev = 0;
    this.warnings = [];
    this.bootError = null;
    this.restarts = 0;
    this.crashTimes = [];
    this.queue = [];         // jobs waiting for the engine
    this.inflight = null;    // { job, timer }
    this.pending = new Map(); // id -> job (at most one entry: commands are serialised)
    this.nextId = 1;
    this.stopping = false;
    this.rx = Buffer.alloc(0);
    this.restartTimer = null;
    this.bootTimer = null;
    this.ready = false;
    this.bootFailed = false;
  }

  start() {
    this.stopping = false;
    this._spawn();
  }

  _setState(state, extra) {
    if (this.state === state && !extra) return;
    this.state = state;
    this.emit('status', { state, pid: this.pid, bootId: this.bootId, ...extra });
  }

  _spawn() {
    clearTimeout(this.restartTimer);
    this._setState(this.state === 'RESTARTING' || this.state === 'DEGRADED' ? this.state : 'STARTING');
    this.rx = Buffer.alloc(0);
    let proc;
    try {
      proc = spawn(this.exe, ['--data', this.dataDir, '--protocol', '1'], {
        windowsHide: true,
        stdio: ['pipe', 'pipe', 'pipe'],
      });
    } catch (e) {
      this.bootError = { code: 'INTERNAL', message: `cannot start the engine: ${e.message}` };
      this._setState('FAILED_BOOT', { error: this.bootError });
      this._failQueue();
      return;
    }
    this.proc = proc;
    this.pid = proc.pid ?? null;
    this.startedAt = Date.now();
    this.ready = false;
    this.bootFailed = false;

    proc.stdout.on('data', (chunk) => this._onStdout(proc, chunk));
    proc.stderr.on('data', (chunk) => {
      for (const line of chunk.toString('utf8').split('\n')) {
        if (line.trim()) this.log('warn', line.trim(), { src: 'engine' });
      }
    });
    proc.stdin.on('error', () => {});   // an EPIPE is handled by the exit event
    proc.on('error', (e) => {
      this.log('error', `engine process error: ${e.message}`);
      if (!this.ready) {
        this.bootError = { code: 'INTERNAL', message: `cannot start the engine: ${e.message}` };
        this._onExit(proc, -1);
      }
    });
    proc.on('exit', (code) => this._onExit(proc, code));

    clearTimeout(this.bootTimer);
    this.bootTimer = setTimeout(() => {
      if (this.proc === proc && !this.ready) {
        this.log('error', 'engine did not become ready in time; killing it');
        try { proc.kill(); } catch { /* already gone */ }
      }
    }, this.bootDeadlineMs);
  }

  _onStdout(proc, chunk) {
    if (proc !== this.proc) return;
    this.rx = Buffer.concat([this.rx, chunk]);
    let at;
    while ((at = this.rx.indexOf(0x0a)) >= 0) {
      const lineBuf = this.rx.subarray(0, at);
      this.rx = this.rx.subarray(at + 1);
      const text = lineBuf.toString('utf8').replace(/\r$/, '');
      if (text) this._onLine(text);
    }
  }

  _onLine(text) {
    let msg;
    try {
      msg = JSON.parse(text);
    } catch {
      this.log('warn', `engine printed a line that is not JSON: ${text.slice(0, 200)}`);
      return;
    }
    if (msg.event === 'ready') {
      this.ready = true;
      clearTimeout(this.bootTimer);
      this.bootId = msg.bootId;
      this.rev = msg.rev ?? 0;
      this.warnings = msg.warnings || [];
      this.bootError = null;
      this.engineVersion = msg.engineVersion;
      this._setState('READY');
      this.emit('ready', msg);
      this._pump();
      return;
    }
    if (msg.event === 'boot_failed') {
      this.bootFailed = true;
      this.bootError = msg.error || { code: 'DATA_INVALID', message: 'The engine could not start' };
      return;
    }
    if (msg.event) {
      if (msg.event === 'log') this.log(msg.level || 'info', msg.msg || '', { src: 'engine' });
      return;
    }
    const job = this.pending.get(msg.id);
    if (!job) {
      this.log('warn', `engine answered an unknown id ${msg.id}`);
      return;
    }
    this.pending.delete(msg.id);
    clearTimeout(this.inflight?.timer);
    this.inflight = null;
    if (typeof msg.rev === 'number') this.rev = msg.rev;
    job.resolve(msg);
    this._pump();
  }

  _onExit(proc, code) {
    if (proc !== this.proc) return;
    clearTimeout(this.bootTimer);
    this.proc = null;
    const wasReady = this.ready;
    this.ready = false;
    const inflight = this.inflight;
    this.inflight = null;
    this.pending.clear();
    if (inflight) {
      clearTimeout(inflight.timer);
      const job = inflight.job;
      if (job.write) {
        job.reject(new EngineFailure('OUTCOME_UNKNOWN',
          'The engine stopped while this change was being made; refresh and check before trying again'));
      } else if (!job.retried) {
        job.retried = true;       // a read is safe to repeat after the restart
        this.queue.unshift(job);
      } else {
        job.reject(new EngineFailure('ENGINE_UNAVAILABLE', 'The engine stopped while answering'));
      }
    }
    if (this.stopping) {
      this._setState('STOPPED');
      this._failQueue();
      return;
    }
    if (!wasReady && (this.bootFailed || code === 2)) {
      // bad data files: restarting would fail the same way
      this._setState('FAILED_BOOT', { error: this.bootError });
      this.log('error', `engine boot failed: ${this.bootError?.message}`);
      this._failQueue();
      return;
    }
    const now = Date.now();
    this.crashTimes = this.crashTimes.filter((t) => now - t < CRASH_WINDOW_MS);
    this.crashTimes.push(now);
    this.restarts++;
    this.log('warn', `engine exited with code ${code}; restarting`);
    if (this.crashTimes.length >= MAX_CRASHES_IN_WINDOW) {
      this._setState('DEGRADED');
      this._failQueue();
      this.restartTimer = setTimeout(() => this._spawn(), 15_000);   // keep trying slowly
      return;
    }
    const delay = BACKOFF_MS[Math.min(this.crashTimes.length - 1, BACKOFF_MS.length - 1)];
    this._setState('RESTARTING');
    this.restartTimer = setTimeout(() => this._spawn(), delay);
  }

  _failQueue() {
    const failure = this.state === 'FAILED_BOOT'
      ? new EngineFailure('ENGINE_UNAVAILABLE', `The engine could not start: ${this.bootError?.message || 'unknown reason'}`, { boot: this.bootError })
      : new EngineFailure('ENGINE_UNAVAILABLE', 'The engine is not available');
    const jobs = this.queue.splice(0);
    for (const job of jobs) {
      clearTimeout(job.waitTimer);
      job.reject(failure);
    }
  }

  /**
   * Sends one command. Resolves with the engine's response object (ok true or false); rejects with EngineFailure
   * when the engine is down, too slow, or stopped during a write.
   */
  call(cmd, args = {}, opts = {}) {
    const write = opts.write ?? /^(offer\.(send|accept|decline|withdraw)|donation\.(create|close|createWithNewDonor)|donor\.create|recipient\.(create|update))$/.test(cmd);
    return new Promise((resolve, reject) => {
      if (this.state === 'FAILED_BOOT' || this.state === 'STOPPED' || this.state === 'DEGRADED') {
        reject(new EngineFailure('ENGINE_UNAVAILABLE',
          this.state === 'FAILED_BOOT' ? `The engine could not start: ${this.bootError?.message}` : 'The engine is not available',
          this.state === 'FAILED_BOOT' ? { boot: this.bootError } : {}));
        return;
      }
      if (this.queue.length > 200) {
        reject(new EngineFailure('BUSY', 'Too many requests are waiting'));
        return;
      }
      const job = {
        cmd, args, write, resolve, reject,
        deadlineMs: opts.deadlineMs ?? (write ? this.writeDeadlineMs : this.readDeadlineMs),
        retried: false,
      };
      job.waitTimer = setTimeout(() => {
        const i = this.queue.indexOf(job);
        if (i >= 0) {
          this.queue.splice(i, 1);
          reject(new EngineFailure('ENGINE_UNAVAILABLE', 'The engine is restarting; try again in a moment'));
        }
      }, opts.queueWaitMs ?? this.queueWaitMs);
      this.queue.push(job);
      this._pump();
    });
  }

  _pump() {
    if (this.state !== 'READY' || this.inflight || !this.proc || this.queue.length === 0) return;
    const job = this.queue.shift();
    clearTimeout(job.waitTimer);
    const id = this.nextId++;
    this.pending.set(id, job);
    const proc = this.proc;
    const timer = setTimeout(() => {
      if (this.inflight && this.inflight.job === job) {
        this.log('error', `${job.cmd} passed its deadline; restarting the engine`);
        try { proc.kill(); } catch { /* gone already */ }
      }
    }, job.deadlineMs);
    this.inflight = { job, timer, id };
    const line = JSON.stringify({ v: 1, id, cmd: job.cmd, args: job.args }) + '\n';
    try {
      proc.stdin.write(line, 'utf8');
    } catch (e) {
      this.log('warn', `write to the engine failed: ${e.message}`);
    }
  }

  /** Clean stop: asks the engine to exit, kills it after 3 seconds. */
  async stop() {
    this.stopping = true;
    clearTimeout(this.restartTimer);
    clearTimeout(this.bootTimer);
    const proc = this.proc;
    this._failQueue();
    if (!proc) { this._setState('STOPPED'); return; }
    await new Promise((resolve) => {
      const kill = setTimeout(() => { try { proc.kill(); } catch { /* gone */ } }, 3000);
      proc.once('exit', () => { clearTimeout(kill); resolve(); });
      try { proc.stdin.write('{"id":0,"cmd":"sys.shutdown"}\n'); proc.stdin.end(); } catch { try { proc.kill(); } catch { /* gone */ } }
    });
  }

  info() {
    return {
      state: this.state,
      pid: this.pid,
      uptimeSec: this.proc && this.startedAt ? Math.round((Date.now() - this.startedAt) / 1000) : 0,
      restarts: this.restarts,
      bootId: this.bootId,
      rev: this.rev,
      error: this.state === 'FAILED_BOOT' ? this.bootError : null,
    };
  }
}
