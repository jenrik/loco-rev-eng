import { Handle, Position, type Node, type NodeProps } from '@xyflow/react';
import type { GraphNode, GraphNodeKind } from '../contracts';

export type DashboardFlowNode = Node<{ model: GraphNode }, GraphNodeKind>;

const roleColors: Record<string, string> = {
  investigator: '#4a7fb5',
  transcriber: '#4a9e6e',
  validator: '#c4a43e',
  integrator: '#c47e3e',
  reviewer: '#b54a4a',
};

export function DashboardNode({ data, selected }: NodeProps<DashboardFlowNode>) {
  const { model } = data;
  const accent = model.role ? roleColors[model.role] : undefined;
  return (
    <div
      className={`graph-node graph-node--${model.kind} graph-node--status-${model.status}${selected ? ' is-selected' : ''}`}
      style={{ '--node-accent': accent } as React.CSSProperties}
    >
      <Handle type="target" position={Position.Top} />
      <div className="graph-node__header">
        <span className="graph-node__kind">{model.kind}</span>
        <span className={`status status--${model.status}`}>{model.status.replace('_', ' ')}</span>
      </div>
      <div className="graph-node__label">{model.label}</div>
      {model.role && <div className="graph-node__role">{model.role}</div>}
      <Handle type="source" position={Position.Bottom} />
    </div>
  );
}
