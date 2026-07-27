import type { DaemonEvent, DaemonSnapshot } from './types';

export async function api<T>(path: string, init: RequestInit = {}): Promise<T> {
  const response = await fetch(path, {
    ...init,
    headers: { 'content-type': 'application/json', ...init.headers },
  });
  if (!response.ok) {
    const body = await response.text();
    throw new Error(body || response.statusText);
  }
  return response.json() as Promise<T>;
}

export const getSnapshot = () => api<DaemonSnapshot>('/api/status');

export const getAgentEvents = (agentId: string, after = 0, limit = 500) =>
  api<{ events: DaemonEvent[] }>(
    `/api/agents/${encodeURIComponent(agentId)}/events?after=${after}&limit=${limit}`,
  );
