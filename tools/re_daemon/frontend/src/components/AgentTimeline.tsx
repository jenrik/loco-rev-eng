import type { ReactNode } from 'react';
import type { DaemonEvent } from '../api/types';

/** Renders payload as compact raw JSON. */
function RawPayload({ payload }: { payload: Record<string, unknown> }): ReactNode {
  const keys = Object.keys(payload);
  if (!keys.length) return null;
  return <pre>{JSON.stringify(payload)}</pre>;
}

// ---- utility ----

/** RE tools whose start/finish plumbing is hidden; a dedicated daemon event shows instead. */
const RE_EVENT_TOOLS = new Set([
  're_record_observation',
  're_expand_task_graph',
  're_record_hypothesis',
  're_transition_task',
  're_defer_task',
]);

function toolLabel(payload: Record<string, unknown>): string {
  return String(payload.toolName ?? '?');
}

const CONFIDENCE_CLASS: Record<string, string> = {
  observed: 'timeline__badge--green',
  tentative: 'timeline__badge--amber',
};

const STATUS_CLASS: Record<string, string> = {
  tentative: 'timeline__badge--amber',
  supported: 'timeline__badge--green',
  rejected: 'timeline__badge--red',
  superseded: 'timeline__badge--muted',
  completed: 'timeline__badge--green',
  blocked: 'timeline__badge--red',
  deferred: 'timeline__badge--amber',
  failed: 'timeline__badge--red',
};

function badge(label: string, cls?: string): ReactNode {
  const c = cls ? `timeline__badge ${cls}` : 'timeline__badge';
  return <span className={c}>{label}</span>;
}

function truncate(text: string, max: number): string {
  if (text.length <= max) return text;
  return text.slice(0, max) + '…';
}

// ---- per-kind custom renderers ----

function RenderAgentStart({ event: _event }: { event: DaemonEvent }): ReactNode {
  return <div className="timeline__summary">Agent run started.</div>;
}

function RenderAgentEnd({ event }: { event: DaemonEvent }): ReactNode {
  const { willRetry, messageCount } = event.payload;
  const parts: string[] = [`${messageCount ?? '?'} message(s) generated`];
  if (willRetry) parts.push('will retry');
  return <div className="timeline__summary">Agent run complete — {parts.join(', ')}.</div>;
}

function RenderAgentSettled({ event: _event }: { event: DaemonEvent }): ReactNode {
  return <div className="timeline__summary">Agent settled.</div>;
}

function RenderTurn({ event }: { event: DaemonEvent }): ReactNode {
  const verb = event.kind === 'turn_start' ? 'started' : 'ended';
  return <div className="timeline__summary">Turn {verb}.</div>;
}

function RenderAssistantDelta({ event }: { event: DaemonEvent }): ReactNode {
  const { delta, deltaType } = event.payload;
  const text = typeof delta === 'string' ? delta : '';
  if (deltaType === 'thinking_delta') {
    return (
      <details>
        <summary>Thinking</summary>
        <pre>{text}</pre>
      </details>
    );
  }
  if (deltaType === 'toolcall_delta') {
    return (
      <details>
        <summary>Tool call arguments</summary>
        <pre><code>{text}</code></pre>
      </details>
    );
  }
  return <pre>{text}</pre>;
}

function RenderAssistantStreamState({ event }: { event: DaemonEvent }): ReactNode {
  const { deltaType, toolCall, reason } = event.payload;
  const label = { toolcall_start: 'Tool call started', toolcall_end: 'Tool call ended', done: 'Done', error: 'Error' }[String(deltaType)] ?? String(deltaType);
  const detail = toolCall ? ` — ${String((toolCall as Record<string,unknown>).name ?? '')}` : reason ? ` — ${String(reason)}` : '';
  return <div className="timeline__summary">{label}{detail}</div>;
}

function RenderToolStarted({ event }: { event: DaemonEvent }): ReactNode {
  const name = toolLabel(event.payload);
  if (RE_EVENT_TOOLS.has(name)) return null;
  return <div className="timeline__summary">Tool started: <code>{name}</code></div>;
}

function RenderToolFinished({ event }: { event: DaemonEvent }): ReactNode {
  const name = toolLabel(event.payload);
  if (RE_EVENT_TOOLS.has(name)) return null;
  const isError = !!event.payload.isError;
  const cls = isError ? ' timeline__summary--error' : '';
  return <div className={`timeline__summary${cls}`}>Tool finished: <code>{name}</code>{isError ? ' (error)' : ''}</div>;
}

function RenderToolProgress({ event: _event }: { event: DaemonEvent }): ReactNode {
  return null;
}

function RenderMessageFinished({ event }: { event: DaemonEvent }): ReactNode {
  const { stopReason } = event.payload;
  const role = String(event.payload.role ?? '?');
  return <div className="timeline__summary">Message finished ({role}){stopReason ? ` — ${String(stopReason)}` : ''}</div>;
}

function RenderCompaction({ event }: { event: DaemonEvent }): ReactNode {
  const verb = event.kind === 'compaction_start' ? 'started' : 'ended';
  const reason = event.payload.reason ? ` (reason: ${String(event.payload.reason)})` : '';
  return <div className="timeline__summary">Compaction {verb}{reason}</div>;
}

function RenderAutoRetry({ event }: { event: DaemonEvent }): ReactNode {
  const verb = event.kind === 'auto_retry_start' ? 'started' : 'ended';
  const attempt = event.payload.attempt != null ? ` attempt #${event.payload.attempt}` : '';
  return <div className="timeline__summary">Auto-retry {verb}{attempt}</div>;
}

function RenderExtensionError({ event }: { event: DaemonEvent }): ReactNode {
  return (
    <div className="timeline__summary timeline__summary--error">
      Extension error: <code>{String(event.payload.extensionPath ?? '?')}</code>
      {event.payload.error ? <pre>{String(event.payload.error)}</pre> : null}
    </div>
  );
}

// ---- daemon lifecycle renderers ----

function RenderDaemonLaunching({ event: _event }: { event: DaemonEvent }): ReactNode {
  return <div className="timeline__summary timeline__summary--muted">Daemon launching Pi process…</div>;
}

function RenderDaemonStarted({ event }: { event: DaemonEvent }): ReactNode {
  return <div className="timeline__summary timeline__summary--muted">Pi process started (pid {String(event.payload.pid ?? '?')}).</div>;
}

function RenderDaemonExited({ event }: { event: DaemonEvent }): ReactNode {
  const exitCode = Number(event.payload.exitCode);
  const aborted = !!event.payload.aborted;
  const ok = exitCode === 0;
  const cls = ok ? ' timeline__summary--muted' : ' timeline__summary--error';
  let label: string;
  if (aborted && exitCode === 0) {
    label = 'aborted';
  } else if (aborted) {
    label = `aborted (code ${exitCode})`;
  } else if (exitCode > 128) {
    const sig = exitCode - 128;
    label = `killed by signal ${sig} (code ${exitCode})`;
  } else {
    label = `exit ${exitCode}`;
  }
  return <div className={`timeline__summary${cls}`}>Pi process {label}.</div>;
}

function RenderAgentStderr({ event }: { event: DaemonEvent }): ReactNode {
  const line = event.payload.line;
  return typeof line === 'string'
    ? <pre className="timeline__stderr">{line}</pre>
    : <RawPayload payload={event.payload} />;
}

function RenderToolTimeout({ event }: { event: DaemonEvent }): ReactNode {
  return <div className="timeline__summary timeline__summary--error">Tool inactivity timeout ({String(event.payload.timeoutSeconds ?? '?')}s) — aborting.</div>;
}

function RenderTaskAttemptFailed({ event }: { event: DaemonEvent }): ReactNode {
  const { reason } = event.payload;
  return <div className="timeline__summary timeline__summary--error">Task attempt failed{reason ? `: ${String(reason)}` : ''}.</div>;
}

function RenderTaskCheckpointed({ event: _event }: { event: DaemonEvent }): ReactNode {
  return <div className="timeline__summary timeline__summary--muted">Task checkpointed on daemon shutdown.</div>;
}

// ---- RE daemon event renderers ----

function RenderObservationRecorded({ event }: { event: DaemonEvent }): ReactNode {
  const address = String(event.payload.address ?? '?');
  const statement = String(event.payload.statement ?? '');
  const confidence = String(event.payload.confidence ?? 'tentative');
  const cls = CONFIDENCE_CLASS[confidence] ?? '';
  return (
    <div className="timeline__summary timeline__re-event">
      {badge(confidence.toUpperCase(), cls)}{' '}
      <code>{address}</code>
      {' '}{truncate(statement, 120)}
    </div>
  );
}

function RenderTaskGraphExpanded({ event }: { event: DaemonEvent }): ReactNode {
  const tasks = event.payload.tasks as Array<{ id?: string; key?: string; title?: string; role?: string }> | undefined;
  const edgeCount = Number(event.payload.edgeCount ?? 0);
  const taskCount = tasks?.length ?? 0;
  const replay = !!event.payload.idempotentReplay;
  return (
    <div className="timeline__summary timeline__re-event">
      Task graph expanded: {taskCount} task(s), {edgeCount} edge(s){replay ? ' (idempotent replay)' : ''}
      {tasks && tasks.length > 0 && (
        <ul className="timeline__task-list">
          {tasks.map((t) => (
            <li key={t.id ?? t.key}>{t.title ?? t.key} {t.role ? badge(t.role, 'timeline__badge--muted') : null}</li>
          ))}
        </ul>
      )}
    </div>
  );
}

function RenderHypothesisRecorded({ event }: { event: DaemonEvent }): ReactNode {
  const hyp = event.payload.hypothesis as Record<string, unknown> | undefined;
  if (!hyp) return <RawPayload payload={event.payload} />;
  const subject = String(hyp.subject ?? '?');
  const status = String(hyp.status ?? 'tentative');
  const statusCls = STATUS_CLASS[status] ?? '';
  const revision = hyp.revision != null ? ` r${hyp.revision}` : '';
  return (
    <div className="timeline__summary timeline__re-event">
      Hypothesis{revision} on <code>{subject}</code>{' '}
      {badge(status.toUpperCase(), statusCls)}
    </div>
  );
}

function RenderTaskTransitioned({ event }: { event: DaemonEvent }): ReactNode {
  const task = event.payload.task as Record<string, unknown> | undefined;
  if (!task) return <RawPayload payload={event.payload} />;
  const status = String(task.status ?? '?');
  const reason = task.transition_reason ? String(task.transition_reason) : '';
  const statusCls = STATUS_CLASS[status] ?? '';
  return (
    <div className="timeline__summary timeline__re-event">
      Task → {badge(status.toUpperCase(), statusCls)}
      {reason ? <span className="timeline__reason"> — {truncate(reason, 150)}</span> : null}
    </div>
  );
}

// ---- renderer registry ----

type EventRenderer = (props: { event: DaemonEvent }) => ReactNode;

const renderers: Record<string, EventRenderer | undefined> = {
  agent_start: RenderAgentStart,
  agent_end: RenderAgentEnd,
  agent_settled: RenderAgentSettled,
  turn_start: RenderTurn,
  turn_end: RenderTurn,
  assistant_delta: RenderAssistantDelta,
  assistant_stream_state: RenderAssistantStreamState,
  tool_started: RenderToolStarted,
  tool_progress: RenderToolProgress,
  tool_finished: RenderToolFinished,
  message_finished: RenderMessageFinished,
  compaction_start: RenderCompaction,
  compaction_end: RenderCompaction,
  auto_retry_start: RenderAutoRetry,
  auto_retry_end: RenderAutoRetry,
  extension_error: RenderExtensionError,
  daemon_launching: RenderDaemonLaunching,
  daemon_started: RenderDaemonStarted,
  daemon_exited: RenderDaemonExited,
  agent_stderr: RenderAgentStderr,
  agent_tool_timeout: RenderToolTimeout,
  task_attempt_failed: RenderTaskAttemptFailed,
  task_checkpointed: RenderTaskCheckpointed,
  observation_recorded: RenderObservationRecorded,
  task_graph_expanded: RenderTaskGraphExpanded,
  hypothesis_recorded: RenderHypothesisRecorded,
  task_transitioned: RenderTaskTransitioned,
};

// ---- component ----

/** Merge consecutive assistant deltas of the same type into a single progressing block. */
function consolidateStreaming(events: DaemonEvent[]): DaemonEvent[] {
  const out: DaemonEvent[] = [];
  for (const event of events) {
    if (event.kind !== 'assistant_delta') { out.push(event); continue; }
    const last = out[out.length - 1];
    if (
      last && last.kind === 'assistant_delta' &&
      last.payload.deltaType === event.payload.deltaType &&
      last.payload.contentIndex === event.payload.contentIndex &&
      typeof last.payload.delta === 'string' && typeof event.payload.delta === 'string'
    ) {
      out[out.length - 1] = {
        ...last,
        payload: {
          ...last.payload,
          delta: (last.payload.delta as string) + (event.payload.delta as string),
        },
      };
    } else {
      out.push(event);
    }
  }
  return out;
}

export function AgentTimeline({ events }: { events: DaemonEvent[] }) {
  if (!events.length) return <p className="empty">No events in the bounded history window.</p>;
  const consolidated = consolidateStreaming(events);
  return (
    <div className="timeline">
      {[...consolidated].reverse().slice(0, 250).map((event) => {
        const render = renderers[event.kind];
        const content = render ? render({ event }) : <RawPayload payload={event.payload} />;
        if (content === null) return null; // suppressed (e.g., RE tool plumbing)
        return (
          <div className={`timeline__event timeline__event--${event.kind}`} key={event.sequence}>
            <div className="timeline__meta">
              <span>{event.kind.replaceAll('_', ' ')}</span>
              <span>#{event.sequence}</span>
            </div>
            {content}
          </div>
        );
      })}
    </div>
  );
}