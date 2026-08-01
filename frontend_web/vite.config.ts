import { fileURLToPath, URL } from 'node:url'

import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'
import vueJsx from '@vitejs/plugin-vue-jsx'
import vueDevTools from 'vite-plugin-vue-devtools'

// https://vite.dev/config/
export default defineConfig({
  plugins: [vue(), vueJsx(), vueDevTools()],
  resolve: {
    alias: {
      '@': fileURLToPath(new URL('./src', import.meta.url)),
    },
  },
  server: {
    proxy: {
      //'/api': 'http://192.168.178.52',
      '/api': 'http://172.20.10.3',
      '/api/ws': {
        //target: 'ws://192.168.178.52',
        target: 'ws://172.20.10.3',
        ws: true,
      },
    },
  },
})
