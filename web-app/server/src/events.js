// Server-Sent Events hub: ring buffer of recent events, Last-Event-ID replay, heartbeat, audience filtering.

/** Coordinator sees everything; a donor/recipient sees events whose audience lists its identity id. */
export function canSee(session, ev) {
  if (!session) return false;
  if (session.role === 'coordinator') return true;
  if (ev.broadcast) return true;
  if (session.role === 'donor') return (ev.audience?.donors || []).includes(session.identityId);
  if (session.role === 'recipient') return (ev.audience?.recipients || []).includes(session.identityId);
  return false;
}

export class EventHub {
  constructor({ ringSize = 200, heartbeatMs = 15000 } = {}) {
    this.ringSize = ringSize;
    this.seq = 0;
    this.ring = [];
    this.clients = new Set();
    this.timer = setInterval(() => {
      for (const c of [...this.clients]) this._write(c, ': heartbeat\n\n');
    }, heartbeatMs);
    this.timer.unref();
  }

  publish(type, data, audience = {}, broadcast = false) {
    const ev = { id: ++this.seq, type, data, audience, broadcast };
    this.ring.push(ev);
    if (this.ring.length > this.ringSize) this.ring.shift();
    for (const c of [...this.clients]) if (canSee(c.session, ev)) this._write(c, this._frame(ev));
    return ev;
  }

  _frame(ev) {
    return `id: ${ev.id}\nevent: ${ev.type}\ndata: ${JSON.stringify(ev.data)}\n\n`;
  }

  _write(client, text) {
    try { client.res.write(text); } catch { this.clients.delete(client); }
  }

  /** Turns a raw Node response into an SSE stream for this session (call after reply.hijack()). */
  attach(rawReq, res, session) {
    res.writeHead(200, {
      'Content-Type': 'text/event-stream; charset=utf-8',
      'Cache-Control': 'no-store',
      Connection: 'keep-alive',
      'X-Accel-Buffering': 'no',
    });
    const client = { res, session };
    res.write('retry: 2000\n\n');
    const last = Number(rawReq.headers['last-event-id']);
    if (Number.isInteger(last) && last > 0) {
      const oldest = this.ring.length ? this.ring[0].id : this.seq + 1;
      if (last < oldest - 1) {
        res.write(`id: ${this.seq}\nevent: resync\ndata: ${JSON.stringify({ reason: 'gap', from: last, to: this.seq })}\n\n`);
      } else {
        for (const ev of this.ring) if (ev.id > last && canSee(session, ev)) this._write(client, this._frame(ev));
      }
    }
    this.clients.add(client);
    const drop = () => this.clients.delete(client);
    rawReq.on('close', drop);
    res.on('error', drop);
    return client;
  }

  closeAll() {
    clearInterval(this.timer);
    for (const c of this.clients) { try { c.res.end(); } catch { /* closed */ } }
    this.clients.clear();
  }
}
