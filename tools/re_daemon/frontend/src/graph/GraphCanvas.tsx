import type { GraphRendererComponent, GraphRendererProps } from './contracts';
import { ReactFlowRenderer } from './react-flow/ReactFlowRenderer';

export type GraphRendererId = 'react-flow';

/**
 * Renderer registry: a future G6/Sigma adapter only needs to implement
 * GraphRendererProps and be registered here. Daemon DTOs and application UI
 * remain unaware of provider-specific node, edge, and viewport types.
 */
const rendererRegistry: Record<GraphRendererId, GraphRendererComponent> = {
  'react-flow': ReactFlowRenderer,
};

export function resolveGraphRenderer(id: string): GraphRendererComponent {
  const renderer = rendererRegistry[id as GraphRendererId];
  if (!renderer) throw new Error(`Unknown graph renderer: ${id}`);
  return renderer;
}

const configuredRenderer = import.meta.env.VITE_GRAPH_RENDERER ?? 'react-flow';
const Renderer = resolveGraphRenderer(configuredRenderer);

export function GraphCanvas(props: GraphRendererProps) {
  return <Renderer {...props} />;
}
