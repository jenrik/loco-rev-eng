import { describe, expect, it } from 'vitest';
import type { DaemonSnapshot } from '../src/api/types';
import { buildGraph } from '../src/graph/buildGraph';

const snapshot: DaemonSnapshot = {
  jobs: [{ id: 'job', title: 'Job', goal: 'Goal', status: 'running' }],
  tasks: [
    { id: 'first', job_id: 'job', title: 'First', instructions: 'A', role: 'investigator', status: 'completed', assigned_agent_id: null },
    { id: 'second', job_id: 'job', title: 'Second', instructions: 'B', role: 'validator', status: 'in_progress', assigned_agent_id: 'agent' },
  ],
  agents: [{ id: 'agent', job_id: 'job', role: 'validator', task: 'B', status: 'running' }],
  taskEdges: [{ task_id: 'second', dependency_task_id: 'first', relation: 'requires' }],
  hypotheses: [{ id: 'hyp', job_id: 'job', subject: 'Claim', statement: 'Evidence', status: 'supported' }],
  writeScopeRequests: [],
  lastSequence: 7,
  ghidra: {},
};

describe('buildGraph', () => {
  it('keeps daemon semantics independent from the renderer', () => {
    const graph = buildGraph(snapshot);
    expect(graph.nodes).toHaveLength(5);
    expect(graph.edges).toEqual(expect.arrayContaining([
      expect.objectContaining({ source: 'job', target: 'first', kind: 'contains' }),
      expect.objectContaining({ source: 'first', target: 'second', kind: 'requires' }),
      expect.objectContaining({ source: 'second', target: 'agent', kind: 'assignment' }),
      expect.objectContaining({ source: 'job', target: 'hyp', kind: 'contains' }),
    ]));
    expect(graph.edges).not.toEqual(expect.arrayContaining([
      expect.objectContaining({ source: 'job', target: 'second' }),
    ]));
  });
});
