export interface Job {
  id: string;
  title: string;
  goal: string;
  status: string;
}

export interface Task {
  id: string;
  job_id: string;
  title: string;
  instructions: string;
  role: string;
  status: string;
  assigned_agent_id: string | null;
  transition_reason?: string | null;
}

export interface Agent {
  id: string;
  job_id: string;
  role: string;
  task: string;
  status: string;
  last_activity_sequence?: number;
}

export interface TaskEdge {
  task_id: string;
  dependency_task_id: string;
  relation: 'requires' | 'evidence' | 'invalidates';
}

export interface Hypothesis {
  id: string;
  job_id: string;
  subject: string;
  statement: string;
  status: string;
}

export interface WriteScopeRequest {
  id: string;
  agent_id: string;
  task_id: string | null;
  path: string;
  reason: string;
  status: string;
}

export interface GhidraStatus {
  configured?: boolean;
  opened?: boolean;
  databaseId?: string | null;
}

export interface DaemonSnapshot {
  jobs: Job[];
  tasks: Task[];
  agents: Agent[];
  taskEdges: TaskEdge[];
  hypotheses: Hypothesis[];
  writeScopeRequests: WriteScopeRequest[];
  lastSequence: number;
  ghidra: GhidraStatus;
}

export interface DaemonEvent {
  sequence: number;
  agent_id: string | null;
  kind: string;
  payload: Record<string, unknown>;
  created_at?: string;
}
