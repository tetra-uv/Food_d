// Signed, httpOnly cookie sessions (open mode: role + identity chosen by name).
import crypto from 'node:crypto';

export const COOKIE = 'cb_session';
const MAX_AGE_S = 8 * 3600;

export function makeCookieValue(secret, session) {
  const payload = Buffer.from(JSON.stringify({
    r: session.role, i: session.identityId ?? null, e: Math.floor(Date.now() / 1000) + MAX_AGE_S,
  })).toString('base64url');
  const sig = crypto.createHmac('sha256', secret).update(payload).digest('base64url');
  return `${payload}.${sig}`;
}

export function readCookieValue(secret, value) {
  if (!value || typeof value !== 'string') return null;
  const dot = value.indexOf('.');
  if (dot < 1) return null;
  const payload = value.slice(0, dot);
  const sig = Buffer.from(value.slice(dot + 1), 'base64url');
  const want = crypto.createHmac('sha256', secret).update(payload).digest();
  if (sig.length !== want.length || !crypto.timingSafeEqual(sig, want)) return null;
  try {
    const p = JSON.parse(Buffer.from(payload, 'base64url').toString('utf8'));
    if (typeof p.e !== 'number' || p.e < Date.now() / 1000) return null;
    if (!['coordinator', 'donor', 'recipient'].includes(p.r)) return null;
    return { role: p.r, identityId: p.i };
  } catch { return null; }
}

export function parseCookies(header) {
  const out = {};
  if (!header) return out;
  for (const part of header.split(';')) {
    const at = part.indexOf('=');
    if (at < 0) continue;
    out[part.slice(0, at).trim()] = part.slice(at + 1).trim();
  }
  return out;
}

export function setCookieHeader(value) {
  return `${COOKIE}=${value}; HttpOnly; SameSite=Lax; Path=/; Max-Age=${MAX_AGE_S}`;
}

export function clearCookieHeader() {
  return `${COOKIE}=; HttpOnly; SameSite=Lax; Path=/; Max-Age=0`;
}
