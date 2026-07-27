import { useEffect, useMemo, useRef, useState } from 'react';
import {
  Background,
  Controls,
  MarkerType,
  MiniMap,
  ReactFlow,
  applyNodeChanges,
  type EdgeTypes,
  type NodeChange,
  type NodeTypes,
  type ReactFlowInstance,
} from '@xyflow/react';
import '@xyflow/react/dist/style.css';
import type { GraphNode, GraphRendererProps } from '../contracts';
import { elkLayeredLayout } from '../elkLayout';
import { DashboardEdge, type DashboardFlowEdge } from './DashboardEdge';
import { DashboardNode, type DashboardFlowNode } from './DashboardNode';

const nodeTypes: NodeTypes = {
  job: DashboardNode,
  task: DashboardNode,
  agent: DashboardNode,
  hypothesis: DashboardNode,
};
const edgeTypes: EdgeTypes = { dashboard: DashboardEdge };

function matches(node: GraphNode, query: string, jobId: string): boolean {
  if (jobId && node.jobId !== jobId) return false;
  if (!query) return true;
  const haystack = [node.id, node.label, node.status, node.role ?? ''].join(' ').toLowerCase();
  return haystack.includes(query.toLowerCase());
}

/** The only component that knows about React Flow types and lifecycle. */
export function ReactFlowRenderer({
  graph,
  selectedNodeId,
  filter,
  commands,
  onNodeSelect,
  onContextMenu,
}: GraphRendererProps) {
  const [nodes, setNodes] = useState<DashboardFlowNode[]>([]);
  const [edges, setEdges] = useState<DashboardFlowEdge[]>([]);
  const [instance, setInstance] = useState<ReactFlowInstance<DashboardFlowNode, DashboardFlowEdge> | null>(null);
  const graphRef = useRef(graph);
  graphRef.current = graph;

  const topologySignature = useMemo(
    () => [
      ...graph.nodes.map((node) => node.id).sort(),
      ...graph.edges.map((edge) => `${edge.id}:${edge.source}:${edge.target}`).sort(),
    ].join('|'),
    [graph.nodes, graph.edges],
  );

  useEffect(() => {
    setNodes((current) => {
      const previous = new Map(current.map((node) => [node.id, node]));
      return graph.nodes.map((model) => {
        const old = previous.get(model.id);
        const visible = matches(model, filter.query, filter.jobId);
        return {
          id: model.id,
          type: model.kind,
          data: { model },
          position: old?.position ?? { x: 0, y: 0 },
          width: model.width,
          height: model.height,
          selected: model.id === selectedNodeId,
          style: { width: model.width, height: model.height, opacity: visible ? 1 : 0.12 },
        };
      });
    });
    const visibleIds = new Set(
      graph.nodes.filter((node) => matches(node, filter.query, filter.jobId)).map((node) => node.id),
    );
    const filtering = Boolean(filter.query || filter.jobId);
    setEdges(graph.edges.map((model) => ({
      id: model.id,
      source: model.source,
      target: model.target,
      type: 'dashboard',
      data: { model },
      markerEnd: { type: MarkerType.ArrowClosed, color: '#65788b' },
      style: { opacity: !filtering || (visibleIds.has(model.source) && visibleIds.has(model.target)) ? 1 : 0.08 },
    })));
  }, [graph, filter, selectedNodeId]);

  useEffect(() => {
    let cancelled = false;
    const model = graphRef.current;
    void elkLayeredLayout.layout(model).then((positions) => {
      if (cancelled) return;
      setNodes((current) => current.map((node) => ({
        ...node,
        position: positions.get(node.id) ?? node.position,
      })));
      window.setTimeout(() => instance?.fitView({ padding: 0.14, duration: 350 }), 0);
    });
    return () => { cancelled = true; };
  }, [topologySignature, commands.layoutRevision, instance]);

  useEffect(() => {
    if (commands.fitRevision > 0) instance?.fitView({ padding: 0.14, duration: 300 });
  }, [commands.fitRevision, instance]);

  return (
    <ReactFlow<DashboardFlowNode, DashboardFlowEdge>
      nodes={nodes}
      edges={edges}
      nodeTypes={nodeTypes}
      edgeTypes={edgeTypes}
      onInit={setInstance}
      onNodesChange={(changes: NodeChange<DashboardFlowNode>[]) =>
        setNodes((current) => applyNodeChanges(changes, current))
      }
      onNodeClick={(_, node) => onNodeSelect(node.id)}
      onPaneClick={() => onNodeSelect(null)}
      onNodeContextMenu={(event, node) => {
        event.preventDefault();
        onContextMenu({ nodeId: node.id, clientX: event.clientX, clientY: event.clientY });
      }}
      onPaneContextMenu={(event) => {
        event.preventDefault();
        onContextMenu({ nodeId: null, clientX: event.clientX, clientY: event.clientY });
      }}
      minZoom={0.08}
      maxZoom={3}
      fitView
    >
      <Background color="#26313e" gap={24} size={1} />
      <MiniMap pannable zoomable nodeColor="#31577b" maskColor="rgba(13,17,23,.72)" />
      <Controls />
    </ReactFlow>
  );
}
