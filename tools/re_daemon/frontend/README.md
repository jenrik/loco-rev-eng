# Autonomous RE dashboard frontend

React/TypeScript operator UI for the FastAPI daemon in `tools/re_daemon`.
The production bundle is written to `../static` and served by FastAPI at `/`.

## Commands

```bash
npm ci
npm run dev      # proxies /api and /ws to 127.0.0.1:8765
npm test
npm run build    # refreshes the standalone assets in ../static
```

From the repository root, use `make dashboard` or `make dashboard-test`.
Generated static assets are checked in so the Python daemon remains runnable
without a Node process or an npm install at runtime.

## Swappable graph architecture

Provider-independent code lives in `src/graph`:

- `contracts.ts` defines `GraphModel`, renderer callbacks, commands, and the
  layout interface. No React Flow types may cross this boundary.
- `buildGraph.ts` maps daemon persistence DTOs into that model.
- `elkLayout.ts` implements `GraphLayoutEngine` and lazy-loads ELK.
- `GraphCanvas.tsx` is the renderer registry.
- `react-flow/` is the only directory that imports `@xyflow/react`.

To add G6, Sigma, or another renderer, implement `GraphRendererProps`, add it
to the registry, and select it with `VITE_GRAPH_RENDERER` at build time. The
FastAPI API, daemon DTO mapping, inspector, operator actions, and WebSocket
handling should not change.

ELK is also behind a separate interface so a renderer can either reuse the
current layered layout or provide its own engine without changing graph data.
