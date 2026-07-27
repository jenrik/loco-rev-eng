import { useCallback, useEffect, useRef, useState } from 'react';
import { getAgentEvents, getSnapshot } from '../api/client';
import type { DaemonEvent, DaemonSnapshot } from '../api/types';

const emptySnapshot: DaemonSnapshot = {
  jobs: [], tasks: [], agents: [], taskEdges: [], hypotheses: [],
  writeScopeRequests: [], lastSequence: 0, ghidra: {},
};

function mergeEvents(current: DaemonEvent[], incoming: DaemonEvent[]): DaemonEvent[] {
  const bySequence = new Map(current.map((event) => [event.sequence, event]));
  for (const event of incoming) bySequence.set(event.sequence, event);
  return [...bySequence.values()].sort((a, b) => a.sequence - b.sequence).slice(-2000);
}

export function useDaemonDashboard() {
  const [snapshot, setSnapshot] = useState<DaemonSnapshot>(emptySnapshot);
  const [events, setEvents] = useState<DaemonEvent[]>([]);
  const [connection, setConnection] = useState<'connecting' | 'live' | 'disconnected'>('connecting');
  const [error, setError] = useState<string | null>(null);
  const sequenceRef = useRef(0);

  const refresh = useCallback(async () => {
    try {
      const next = await getSnapshot();
      setSnapshot(next);
      sequenceRef.current = Math.max(sequenceRef.current, next.lastSequence || 0);
      setError(null);
    } catch (reason) {
      setError(reason instanceof Error ? reason.message : String(reason));
    }
  }, []);

  const backfillAgentEvents = useCallback(async (agentId: string, lastActivitySequence: number) => {
    // Fetch a bounded historical window; partial streamed messages are consolidated by sequence.
    const after = Math.max(0, lastActivitySequence - 500);
    const response = await getAgentEvents(agentId, after, 500);
    setEvents((current) => mergeEvents(current, response.events));
  }, []);

  useEffect(() => {
    let socket: WebSocket | null = null;
    let reconnectTimer: number | undefined;
    let refreshTimer: number | undefined;
    let stopped = false;

    const connect = () => {
      if (stopped) return;
      setConnection('connecting');
      const protocol = location.protocol === 'https:' ? 'wss:' : 'ws:';
      socket = new WebSocket(`${protocol}//${location.host}/ws`);
      socket.onopen = () => setConnection('live');
      socket.onmessage = (message) => {
        const event = JSON.parse(message.data) as DaemonEvent;
        if (event.sequence <= sequenceRef.current) return;
        sequenceRef.current = event.sequence;
        setEvents((current) => mergeEvents(current, [event]));
        window.clearTimeout(refreshTimer);
        refreshTimer = window.setTimeout(() => void refresh(), event.kind === 'assistant_delta' ? 500 : 150);
      };
      socket.onerror = () => setConnection('disconnected');
      socket.onclose = () => {
        setConnection('disconnected');
        reconnectTimer = window.setTimeout(connect, 3000);
      };
    };

    void refresh().finally(connect);
    return () => {
      stopped = true;
      window.clearTimeout(reconnectTimer);
      window.clearTimeout(refreshTimer);
      socket?.close();
    };
  }, [refresh]);

  return { snapshot, events, connection, error, refresh, backfillAgentEvents };
}
