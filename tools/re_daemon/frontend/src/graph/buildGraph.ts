import type { DaemonSnapshot } from '../api/types';
import type { GraphEdge, GraphModel, GraphNode } from './contracts';

/** Convert daemon persistence DTOs into the stable renderer-neutral graph model. */
export function buildGraph(snapshot: DaemonSnapshot): GraphModel {
  const nodes: GraphNode[] = [];
  const edges: GraphEdge[] = [];
  const jobIds = new Set(snapshot.jobs.map((job) => job.id));
  const taskIds = new Set(snapshot.tasks.map((task) => task.id));
  const tasksWithPrerequisites = new Set(snapshot.taskEdges.map((edge) => edge.task_id));

  for (const job of snapshot.jobs) {
    nodes.push({
      id: job.id,
      kind: 'job',
      label: job.title,
      status: job.status,
      jobId: job.id,
      description: job.goal,
      width: 220,
      height: 68,
    });
  }

  for (const task of snapshot.tasks) {
    nodes.push({
      id: task.id,
      kind: 'task',
      label: task.title,
      status: task.status,
      role: task.role,
      jobId: task.job_id,
      description: task.instructions,
      width: 190,
      height: 72,
    });
    if (jobIds.has(task.job_id) && !tasksWithPrerequisites.has(task.id)) {
      edges.push({
        id: `contains:${task.job_id}:${task.id}`,
        source: task.job_id,
        target: task.id,
        kind: 'contains',
      });
    }
  }

  for (const edge of snapshot.taskEdges) {
    if (taskIds.has(edge.task_id) && taskIds.has(edge.dependency_task_id)) {
      edges.push({
        id: `dependency:${edge.dependency_task_id}:${edge.task_id}:${edge.relation}`,
        source: edge.dependency_task_id,
        target: edge.task_id,
        kind: edge.relation,
        label: edge.relation,
      });
    }
  }

  for (const agent of snapshot.agents) {
    const task = snapshot.tasks.find((candidate) => candidate.assigned_agent_id === agent.id);
    nodes.push({
      id: agent.id,
      kind: 'agent',
      label: agent.role,
      status: agent.status,
      role: agent.role,
      jobId: agent.job_id,
      description: agent.task,
      width: 88,
      height: 58,
    });
    if (task) {
      edges.push({
        id: `assignment:${task.id}:${agent.id}`,
        source: task.id,
        target: agent.id,
        kind: 'assignment',
      });
    }
  }

  for (const hypothesis of snapshot.hypotheses) {
    nodes.push({
      id: hypothesis.id,
      kind: 'hypothesis',
      label: hypothesis.subject,
      status: hypothesis.status,
      jobId: hypothesis.job_id,
      description: hypothesis.statement,
      width: 180,
      height: 64,
    });
    if (jobIds.has(hypothesis.job_id)) {
      edges.push({
        id: `hypothesis:${hypothesis.job_id}:${hypothesis.id}`,
        source: hypothesis.job_id,
        target: hypothesis.id,
        kind: 'contains',
      });
    }
  }

  return { nodes, edges };
}
