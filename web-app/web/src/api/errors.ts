// Error type shared by the HTTP transport and the in-browser (local) transport.
export class ApiError extends Error {
  status: number; code: string; details?: Record<string, unknown>; retryable: boolean;
  constructor(status: number, code: string, message: string, details?: Record<string, unknown>) {
    super(message);
    this.status = status; this.code = code; this.details = details;
    this.retryable = status === 503 || status === 429 || code === 'SAVE_FAILED';
  }
}
