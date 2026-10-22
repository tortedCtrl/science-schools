import react from '@vitejs/plugin-react'
import { defineConfig, loadEnv } from 'vite'
import { fileURLToPath } from 'url'
import path from 'path'

export default defineConfig(({ mode }) => {

    const _dirname = path.dirname(fileURLToPath(import.meta.url));

    const parentEnv = loadEnv(mode, path.resolve(_dirname, '..'), '');

    const host = parentEnv.BACKEND_HOST || '127.0.0.1';
    const port = parentEnv.BACKEND_PORT || '8080';

    const apiUrl = `http://${host}:${port}`;

    return {
        envDir: '..',
        plugins: [react()],
        server: {
            // Можно дополнительно указать host
            port: Number(parentEnv.FRONTEND_PORT || '5173'),
            strictPort: true,

            // Прокси на бэкенд: браузер ходит на localhost:5173/api/*,
            // Vite переправляет на http://<host>:<port>/* (срезая /api).
            // Это снимает CORS без изменений на бэкенде.
            proxy: {
                '/api': {
                    target: apiUrl,
                    changeOrigin: true,
                    rewrite: (p) => p.replace(/^\/api/, ''),
                },
            },
        },

        define: {
            'import.meta.env.VITE_API_URL': JSON.stringify(apiUrl)
        }
    }
})