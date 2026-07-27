import type { ComponentType } from 'react';

/** Library-neutral graph contract. Application code must not expose renderer types. */
export type GraphNodeKind = 'job' | 'task' | 'agent' | 'hypothesis';
export type GraphEdgeKind = 'contains' | 'requires' | 'evidence' | 'invalidates' | 'assignment';

export interface GraphPosition {
  x: number;
  y: number;
}

export interface GraphNode {
  id: string;
  kind: GraphNodeKind;
  label: string;
  status: string;
  role?: string;
  jobId?: string;
  description?: string;
  width: number;
  height: number;
}

export interface GraphEdge {
  id: string;
  source: string;
  target: string;
  kind: GraphEdgeKind;
  label?: string;
}

export interface GraphModel {
  nodes: GraphNode[];
  edges: GraphEdge[];
}

export interface GraphFilter {
  query: string;
  jobId: string;
}

export interface GraphRendererCommands {
  /** Incrementing these counters makes commands declarative and renderer-neutral. */
  fitRevision: number;
  layoutRevision: number;
}

export interface GraphContextTarget {
  nodeId: string | null;
  clientX: number;
  clientY: number;
}

export interface GraphRendererProps {
  graph: GraphModel;
  selectedNodeId: string | null;
  filter: GraphFilter;
  commands: GraphRendererCommands;
  onNodeSelect(nodeId: string | null): void;
  onContextMenu(target: GraphContextTarget): void;
}

export type GraphRendererComponent = ComponentType<GraphRendererProps>;

export interface GraphLayoutEngine {
  readonly id: string;
  layout(graph: GraphModel): Promise<Map<string, GraphPosition>>;
}
