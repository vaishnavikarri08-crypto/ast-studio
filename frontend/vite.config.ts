import react from '@vitejs/plugin-react'
import { defineConfig } from 'vite'

// https://vite.dev/config/
export default defineConfig({
  plugins: [react()],
  build: {
    // The codemirror chunk (~520 kB, mostly the C++ grammar) is needed on first paint.
    chunkSizeWarningLimit: 600,
    rolldownOptions: {
      output: {
        // Keep large, rarely-changing libraries in their own cacheable chunks.
        codeSplitting: {
          groups: [
            { name: 'codemirror', test: /node_modules[\\/](@codemirror|@lezer|codemirror|crelt|style-mod|w3c-keyname)/ },
            { name: 'react', test: /node_modules[\\/](react|react-dom|scheduler)[\\/]/ },
          ],
        },
      },
    },
  },
})
