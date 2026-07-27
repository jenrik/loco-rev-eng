import { useCallback, useEffect, useMemo, useState } from 'react';
import { api } from './api/client';
import { ContextMenu, type ContextMenuState } from './components/ContextMenu';
import { Inspector, type InspectorActions } from './components/Inspector';
import { InputDialog, useInputDialog } from './components/InputDialog';
import { GraphCanvas } from './graph/GraphCanvas';
import { buildGraph } from './graph/buildGraph';
import type { GraphContextTarget } from './graph/contracts';
import { useDaemonDashboard } from './hooks/useDaemonDashboard';

export default function App() {
  const { snapshot, events, connection, error, refresh, backfillAgentEvents } = useDaemonDashboard();
  const graph = useMemo(() => buildGraph(snapshot), [snapshot]);
  const [selectedNodeId, setSelectedNodeId] = useState<string | null>(null);
  const [search, setSearch] = useState('');
  const [jobFilter, setJobFilter] = useState('');
  const [fitRevision, setFitRevision] = useState(0);
  const [layoutRevision, setLayoutRevision] = useState(0);
  const [panelCollapsed, setPanelCollapsed] = useState(true);
  const [scopePanelOpen, setScopePanelOpen] = useState(false);
  const [menu, setMenu] = useState<ContextMenuState | null>(null);
  const [toast, setToast] = useState<{ message: string; error: boolean } | null>(null);
  const { request: inputDialog, openInputDialog, resolveInputDialog } = useInputDialog();

  const selectedNode = graph.nodes.find((node) => node.id === selectedNodeId) ?? null;
  const pendingScopes = snapshot.writeScopeRequests.filter((request) => request.status === 'pending');

  const notify = useCallback((message: string, isError = false) => {
    setToast({ message, error: isError });
    window.setTimeout(() => setToast(null), 3200);
  }, []);

  const perform = useCallback(async (work: () => Promise<unknown>, success: string) => {
    try {
      await work();
      notify(success);
      await refresh();
    } catch (reason) {
      notify(reason instanceof Error ? reason.message : String(reason), true);
    }
  }, [notify, refresh]);

  const newJob = useCallback(() => {
    void (async () => {
      const values = await openInputDialog({
        title: 'Create job',
        fields: [
          { name: 'title', label: 'Job title', required: true },
          { name: 'goal', label: 'Job goal', type: 'textarea', required: true },
        ],
        submitLabel: 'Create job',
      });
      if (!values) return;
      await perform(
        () => api('/api/jobs', { method: 'POST', body: JSON.stringify({ title: values.title, goal: values.goal }) }),
        'Job created and triage started',
      );
    })();
  }, [openInputDialog, perform]);

  const addTask = useCallback((jobId: string) => {
    void (async () => {
      const values = await openInputDialog({
        title: 'Add task',
        fields: [
          { name: 'title', label: 'Task title', required: true },
          { name: 'instructions', label: 'Instructions', type: 'textarea', required: true, rows: 6 },
          {
            name: 'role', label: 'Role', type: 'select', defaultValue: 'investigator', required: true,
            options: ['investigator', 'transcriber', 'validator', 'integrator', 'reviewer'].map((role) => ({ label: role, value: role })),
          },
        ],
        submitLabel: 'Add task',
      });
      if (!values) return;
      await perform(
        () => api(`/api/jobs/${jobId}/tasks`, {
          method: 'POST', body: JSON.stringify({
            title: values.title, instructions: values.instructions, role: values.role, write_scope: [],
          }),
        }),
        'Task created',
      );
    })();
  }, [openInputDialog, perform]);

  const addDependency = useCallback((taskId: string) => {
    const task = snapshot.tasks.find((candidate) => candidate.id === taskId);
    const candidates = snapshot.tasks.filter((candidate) => candidate.id !== taskId && candidate.job_id === task?.job_id);
    if (!candidates.length) return notify('No task in this job can be used as a prerequisite', true);
    void (async () => {
      const values = await openInputDialog({
        title: 'Add dependency',
        fields: [
          {
            name: 'dependencyTaskId', label: 'Prerequisite', type: 'select', required: true,
            options: candidates.map((candidate) => ({ label: candidate.title, value: candidate.id })),
          },
          {
            name: 'relation', label: 'Relation', type: 'select', defaultValue: 'requires', required: true,
            options: ['requires', 'evidence', 'invalidates'].map((relation) => ({ label: relation, value: relation })),
          },
        ],
        submitLabel: 'Add dependency',
      });
      if (!values) return;
      await perform(
        () => api(`/api/tasks/${taskId}/dependencies`, {
          method: 'POST', body: JSON.stringify({ dependency_task_id: values.dependencyTaskId, relation: values.relation }),
        }),
        'Dependency created',
      );
    })();
  }, [notify, openInputDialog, perform, snapshot.tasks]);

  const actions: InspectorActions = useMemo(() => ({
    schedule: (jobId) => void perform(() => api(`/api/jobs/${jobId}/schedule?limit=1`, { method: 'POST' }), 'Scheduling evaluated'),
    bootstrap: (jobId) => void perform(() => api(`/api/jobs/${jobId}/bootstrap`, { method: 'POST' }), 'Evidence triage started'),
    addTask,
    addDependency,
    retry: (taskId) => {
      void (async () => {
        const values = await openInputDialog({
          title: 'Requeue task',
          fields: [{ name: 'reason', label: 'Retry reason', type: 'textarea', required: true }],
          submitLabel: 'Requeue',
        });
        if (!values) return;
        await perform(
          () => api(`/api/tasks/${taskId}/retry`, { method: 'POST', body: JSON.stringify({ reason: values.reason }) }),
          'Task requeued',
        );
      })();
    },
    recover: (taskId) => {
      void (async () => {
        const values = await openInputDialog({
          title: 'Recover task attempt',
          description: 'This fails the stuck attempt and reaps its agent process.',
          fields: [{ name: 'reason', label: 'Recovery reason', type: 'textarea', required: true }],
          submitLabel: 'Recover attempt',
          danger: true,
        });
        if (!values) return;
        await perform(
          () => api(`/api/tasks/${taskId}/recover`, { method: 'POST', body: JSON.stringify({ reason: values.reason }) }),
          'Recovery requested',
        );
      })();
    },
    controlAgent: (agentId, action) => {
      if (action === 'abort') {
        void perform(() => api(`/api/agents/${agentId}/control`, {
          method: 'POST', body: JSON.stringify({ action, message: null }),
        }), 'Agent abort requested');
        return;
      }
      void (async () => {
        const actionLabel = action.replace('_', ' ');
        const values = await openInputDialog({
          title: `Agent ${actionLabel}`,
          fields: [{ name: 'message', label: 'Message', type: 'textarea', rows: 6 }],
          submitLabel: action === 'steer' ? 'Steer agent' : 'Send follow-up',
        });
        if (!values) return;
        await perform(() => api(`/api/agents/${agentId}/control`, {
          method: 'POST', body: JSON.stringify({ action, message: values.message }),
        }), `Agent ${actionLabel} requested`);
      })();
    },
  }), [addDependency, addTask, openInputDialog, perform]);

  const handleContextMenu = useCallback((target: GraphContextTarget) => {
    const node = target.nodeId ? graph.nodes.find((candidate) => candidate.id === target.nodeId) : null;
    const items = !node ? [{ label: '＋ New job', action: newJob }] : node.kind === 'job' ? [
      { label: '＋ Add task', action: () => addTask(node.id) },
      { label: '▶ Schedule', action: () => actions.schedule(node.id) },
      { label: '◆ Start evidence triage', action: () => actions.bootstrap(node.id) },
    ] : node.kind === 'task' ? [
      { label: '＋ Add dependency', action: () => addDependency(node.id) },
      ...(node.status === 'in_progress' ? [{ label: 'Recover attempt', action: () => actions.recover(node.id), danger: true }] : []),
      ...(['blocked', 'deferred', 'failed'].includes(node.status) ? [{ label: 'Requeue', action: () => actions.retry(node.id) }] : []),
    ] : node.kind === 'agent' ? [
      { label: 'Steer', action: () => actions.controlAgent(node.id, 'steer') },
      { label: 'Follow up', action: () => actions.controlAgent(node.id, 'follow_up') },
      { label: 'Abort', action: () => actions.controlAgent(node.id, 'abort'), danger: true },
    ] : [{ label: 'No actions' }];
    setMenu({
      x: Math.min(target.clientX, window.innerWidth - 220),
      y: Math.min(target.clientY, window.innerHeight - items.length * 38 - 20),
      items,
    });
  }, [actions, addDependency, addTask, graph.nodes, newJob]);

  useEffect(() => {
    if (!selectedNode || selectedNode.kind !== 'agent') return;
    const agent = snapshot.agents.find((candidate) => candidate.id === selectedNode.id);
    void backfillAgentEvents(selectedNode.id, agent?.last_activity_sequence ?? snapshot.lastSequence);
  }, [backfillAgentEvents, selectedNode, snapshot.agents, snapshot.lastSequence]);

  useEffect(() => {
    if (selectedNodeId && !graph.nodes.some((node) => node.id === selectedNodeId)) setSelectedNodeId(null);
  }, [graph.nodes, selectedNodeId]);

  useEffect(() => {
    const onKey = (event: KeyboardEvent) => {
      if (document.querySelector('dialog[open]')) return;
      if (event.key === 'Escape') { setMenu(null); setSelectedNodeId(null); setPanelCollapsed(true); }
      if (event.key === 'f' && event.ctrlKey) {
        event.preventDefault();
        document.querySelector<HTMLInputElement>('#graph-search')?.focus();
      }
    };
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, []);

  const selectNode = (nodeId: string | null) => {
    setSelectedNodeId(nodeId);
    if (nodeId) setPanelCollapsed(false);
  };

  const resolveScope = (requestId: string, decision: 'approved' | 'rejected') => {
    void (async () => {
      const values = await openInputDialog({
        title: `${decision === 'approved' ? 'Approve' : 'Reject'} write scope`,
        fields: [{ name: 'reason', label: 'Reason (optional)', type: 'textarea' }],
        submitLabel: decision === 'approved' ? 'Approve' : 'Reject',
        danger: decision === 'rejected',
      });
      if (!values) return;
      await perform(
        () => api(`/api/write-scope-requests/${requestId}/resolve`, {
          method: 'POST', body: JSON.stringify({ decision, reason: values.reason || undefined }),
        }),
        `Write scope ${decision}`,
      );
    })();
  };

  return (
    <main className="app-shell">
      <header className="topbar">
        <span className={`connection connection--${connection}`}><i />{connection}</span>
        <span className="separator" />
        <input id="graph-search" value={search} onChange={(event) => setSearch(event.target.value)} placeholder="Search nodes…" />
        <label>Job</label>
        <select value={jobFilter} onChange={(event) => setJobFilter(event.target.value)}>
          <option value="">All jobs</option>
          {snapshot.jobs.map((job) => <option value={job.id} key={job.id}>{job.title}</option>)}
        </select>
        <span className="separator" />
        <button onClick={() => setFitRevision((value) => value + 1)}>Fit</button>
        <button onClick={() => setLayoutRevision((value) => value + 1)}>Reset layout</button>
        <button onClick={newJob}>New job</button>
        <span className="counts">{graph.nodes.length} nodes · {graph.edges.length} edges</span>
        <span className="spacer" />
        <button className={pendingScopes.length ? 'warning' : ''} onClick={() => setScopePanelOpen((open) => !open)}>
          Scopes {pendingScopes.length ? `(${pendingScopes.length})` : ''}
        </button>
        <span className="ghidra">Ghidra: {snapshot.ghidra.configured ? 'ready' : 'off'} · DB: {snapshot.ghidra.databaseId ?? '—'}</span>
      </header>

      <div className="graph-viewport">
        <GraphCanvas
          graph={graph}
          selectedNodeId={selectedNodeId}
          filter={{ query: search, jobId: jobFilter }}
          commands={{ fitRevision, layoutRevision }}
          onNodeSelect={selectNode}
          onContextMenu={handleContextMenu}
        />
      </div>

      {scopePanelOpen && (
        <aside className="scope-panel">
          <header><strong>Write-scope requests</strong><button onClick={() => setScopePanelOpen(false)}>×</button></header>
          {!pendingScopes.length && <p className="empty">No pending requests.</p>}
          {pendingScopes.map((request) => (
            <article key={request.id}>
              <code>{request.path}</code>
              <p>{request.reason}</p>
              <button className="primary" onClick={() => resolveScope(request.id, 'approved')}>Approve</button>
              <button className="danger" onClick={() => resolveScope(request.id, 'rejected')}>Reject</button>
            </article>
          ))}
        </aside>
      )}

      <Inspector
        node={selectedNode}
        graph={graph}
        snapshot={snapshot}
        events={events}
        collapsed={panelCollapsed}
        onToggle={() => setPanelCollapsed((collapsed) => !collapsed)}
        actions={actions}
      />
      <InputDialog request={inputDialog} onResolve={resolveInputDialog} />
      <ContextMenu menu={menu} onClose={() => setMenu(null)} />
      {(toast || error) && <div className={`toast ${toast?.error || error ? 'toast--error' : ''}`}>{toast?.message ?? error}</div>}
    </main>
  );
}
