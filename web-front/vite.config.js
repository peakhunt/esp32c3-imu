import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'
import compression from 'vite-plugin-compression'

export default defineConfig({
  plugins: [
    vue(),
    compression({ 
      algorithm: 'gzip',
      deleteOriginFile: true // This automatically deletes index.js and style.css after creating .gz
    })
  ],
  build: {
    rollupOptions: {
      output: {
        // Keeps names simple for your ESP-IDF handlers
        entryFileNames: `[name].js`,
        chunkFileNames: `[name].js`,
        assetFileNames: `[name].[ext]`
      }
    }
  }
})
