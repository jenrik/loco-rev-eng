import {
  BaseEdge,
  EdgeLabelRenderer,
  getSmoothStepPath,
  type Edge,
  type EdgeProps,
} from '@xyflow/react';
import type { GraphEdge } from '../contracts';

export type DashboardFlowEdge = Edge<{ model: GraphEdge }, 'dashboard'>;

const edgeColors: Record<GraphEdge['kind'], string> = {
  contains: '#31577b',
  requires: '#6ea8e6',
  evidence: '#d3a85a',
  invalidates: '#f85149',
  assignment: '#65788b',
};

export function DashboardEdge(props: EdgeProps<DashboardFlowEdge>) {
  const [path, labelX, labelY] = getSmoothStepPath(props);
  const model = props.data?.model;
  const kind = model?.kind ?? 'contains';
  const dashed = kind === 'requires' || kind === 'evidence' || kind === 'invalidates';
  return (
    <>
      <BaseEdge
        id={props.id}
        path={path}
        markerEnd={props.markerEnd}
        style={{
          stroke: edgeColors[kind],
          strokeWidth: kind === 'contains' ? 2 : 1.5,
          strokeDasharray: dashed ? '7 4' : kind === 'assignment' ? '2 4' : undefined,
          opacity: props.style?.opacity,
        }}
      />
      {model?.label && (
        <EdgeLabelRenderer>
          <span
            className="graph-edge-label nodrag nopan"
            style={{ transform: `translate(-50%, -50%) translate(${labelX}px, ${labelY}px)` }}
          >
            {model.label}
          </span>
        </EdgeLabelRenderer>
      )}
    </>
  );
}
