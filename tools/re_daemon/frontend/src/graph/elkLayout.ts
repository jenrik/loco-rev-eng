import type { GraphLayoutEngine, GraphModel, GraphPosition } from './contracts';

// Keep ELK out of the initial UI chunk. It is large and only needed when the
// graph topology changes or the operator explicitly requests a new layout.
let elkPromise: Promise<InstanceType<(typeof import('elkjs/lib/elk.bundled.js'))['default']>> | null = null;
function getElk() {
  elkPromise ??= import('elkjs/lib/elk.bundled.js').then(({ default: ELK }) => new ELK());
  return elkPromise;
}

/** ELK is isolated behind GraphLayoutEngine for the same reason as the renderer. */
export const elkLayeredLayout: GraphLayoutEngine = {
  id: 'elk-layered',
  async layout(graph: GraphModel): Promise<Map<string, GraphPosition>> {
    if (graph.nodes.length === 0) return new Map();

    const elk = await getElk();
    const result = await elk.layout({
      id: 'root',
      layoutOptions: {
        'elk.algorithm': 'layered',
        'elk.direction': 'DOWN',
        'elk.edgeRouting': 'ORTHOGONAL',
        'elk.spacing.nodeNode': '42',
        'elk.layered.spacing.nodeNodeBetweenLayers': '80',
        'elk.layered.nodePlacement.strategy': 'NETWORK_SIMPLEX',
      },
      children: graph.nodes.map((node) => ({
        id: node.id,
        width: node.width,
        height: node.height,
      })),
      edges: graph.edges.map((edge) => ({
        id: edge.id,
        sources: [edge.source],
        targets: [edge.target],
      })),
    });

    return new Map(
      (result.children ?? []).map((node) => [
        node.id,
        { x: node.x ?? 0, y: node.y ?? 0 },
      ]),
    );
  },
};
