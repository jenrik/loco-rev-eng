import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

// FastAPI serves the generated index at / and mounts this directory at /static.
export default defineConfig({
  base: '/static/',
  plugins: [react()],
  build: {
    outDir: '../static',
    emptyOutDir: true,
    sourcemap: false,
  },
  server: {
    proxy: {
      '/api': 'http://127.0.0.1:8765',
      '/ws': { target: 'ws://127.0.0.1:8765', ws: true },
    },
  },
});
