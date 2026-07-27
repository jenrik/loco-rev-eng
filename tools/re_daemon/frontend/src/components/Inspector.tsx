import type { DaemonEvent, DaemonSnapshot } from '../api/types';
import type { GraphModel, GraphNode } from '../graph/contracts';
import { AgentTimeline } from './AgentTimeline';

export interface InspectorActions {
  schedule(jobId: string): void;
  bootstrap(jobId: string): void;
  addTask(jobId: string): void;
  addDependency(taskId: string): void;
  retry(taskId: string): void;
  recover(taskId: string): void;
  controlAgent(agentId: string, action: 'steer' | 'follow_up' | 'abort'): void;
}

export function Inspector({
  node,
  graph,
  snapshot,
  events,
  collapsed,
  onToggle,
  actions,
}: {
  node: GraphNode | null;
  graph: GraphModel;
  snapshot: DaemonSnapshot;
  events: DaemonEvent[];
  collapsed: boolean;
  onToggle(): void;
  actions: InspectorActions;
}) {
  const connected = node ? graph.edges.filter((edge) => edge.source === node.id || edge.target === node.id) : [];
  const agentEvents = node?.kind === 'agent' ? events.filter((event) => event.agent_id === node.id) : [];
  return (
    <section className={`inspector${collapsed ? ' inspector--collapsed' : ''}`}>
      <button className="inspector__handle" onClick={onToggle}>
        <span>{collapsed ? '▲' : '▼'}</span>
        <strong>{node?.label ?? 'Select a node'}</strong>
        <span>{node?.kind ?? ''}</span>
      </button>
      {!collapsed && node && (
        <div className={`inspector__body${node.kind === 'agent' ? ' inspector__body--agent' : ''}`}>
          <div className="inspector__column">
            <h3>Details</h3>
            <Field label="ID" value={node.id} />
            <Field label="Status" value={node.status} />
            {node.role && <Field label="Role" value={node.role} />}
            {node.description && <Field label="Description" value={node.description} pre />}
            <h3>Actions</h3>
            {node.kind === 'job' && <>
              <button className="primary" onClick={() => actions.schedule(node.id)}>Schedule ready task</button>
              <button onClick={() => actions.bootstrap(node.id)}>Start evidence triage</button>
              <button onClick={() => actions.addTask(node.id)}>Add task</button>
            </>}
            {node.kind === 'task' && <>
              <button onClick={() => actions.addDependency(node.id)}>Add dependency</button>
              {node.status === 'in_progress' && <button className="danger" onClick={() => actions.recover(node.id)}>Recover attempt</button>}
              {['blocked', 'deferred', 'failed'].includes(node.status) && <button onClick={() => actions.retry(node.id)}>Requeue</button>}
            </>}
            {node.kind === 'agent' && <>
              <button onClick={() => actions.controlAgent(node.id, 'steer')}>Steer</button>
              <button onClick={() => actions.controlAgent(node.id, 'follow_up')}>Follow up</button>
              <button className="danger" onClick={() => actions.controlAgent(node.id, 'abort')}>Abort</button>
            </>}
          </div>
          {node.kind === 'agent' ? (
            <div className="inspector__column inspector__timeline">
              <h3>Agent timeline</h3>
              <AgentTimeline events={agentEvents} />
            </div>
          ) : (
            <div className="inspector__column">
              <h3>Connections</h3>
              {connected.map((edge) => {
                const outgoing = edge.source === node.id;
                const otherId = outgoing ? edge.target : edge.source;
                const other = graph.nodes.find((candidate) => candidate.id === otherId);
                return <div className="connection" key={edge.id}><span>{edge.kind}</span>{outgoing ? '→' : '←'} {other?.label ?? otherId}</div>;
              })}
              {!connected.length && <p className="empty">No connections</p>}
              {node.kind === 'hypothesis' && <Field label="Revisions" value={String(snapshot.hypotheses.filter((item) => item.subject === node.label).length)} />}
            </div>
          )}
        </div>
      )}
    </section>
  );
}

function Field({ label, value, pre = false }: { label: string; value: string; pre?: boolean }) {
  return <div className="field"><span>{label}</span>{pre ? <pre>{value}</pre> : <div>{value}</div>}</div>;
}
