import type { DaemonEvent } from '../api/types';

function eventText(event: DaemonEvent): string {
  const payload = event.payload;
  if (typeof payload.delta === 'string') return payload.delta;
  if (typeof payload.error === 'string') return payload.error;
  if (typeof payload.operation === 'string') return payload.operation;
  if (payload.toolName) return String(payload.toolName);
  if (payload.task && typeof payload.task === 'object' && payload.task !== null && 'title' in payload.task) {
    return String((payload.task as { title: unknown }).title);
  }
  return Object.keys(payload).length ? JSON.stringify(payload) : event.kind;
}

export function AgentTimeline({ events }: { events: DaemonEvent[] }) {
  if (!events.length) return <p className="empty">No events in the bounded history window.</p>;
  return (
    <div className="timeline">
      {[...events].reverse().slice(0, 250).map((event) => {
        const text = eventText(event);
        const thinking = event.kind === 'assistant_delta' && event.payload.deltaType === 'thinking_delta';
        return (
          <div className={`timeline__event timeline__event--${event.kind}`} key={event.sequence}>
            <div className="timeline__meta">
              <span>{event.kind.replaceAll('_', ' ')}</span>
              <span>#{event.sequence}</span>
            </div>
            {thinking ? <details><summary>Thinking</summary><pre>{text}</pre></details> : <pre>{text}</pre>}
          </div>
        );
      })}
    </div>
  );
}
