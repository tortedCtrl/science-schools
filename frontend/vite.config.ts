import react from '@vitejs/plugin-react'
import { defineConfig, loadEnv } from 'vite'
import { fileURLToPath } from 'url'
import path from 'path'

export default defineConfig(({ mode }) => {

    const _dirname = path.dirname(fileURLToPath(import.meta.url));

    const parentEnv = loadEnv(mode, path.resolve(_dirname, '..'), '');

    // BACKEND_HOST в .env = 0.0.0.0 — это адрес прослушивания сервера,
    // для запросов с фронта нужен конкретный адрес.
    const host = '127.0.0.1';
    const port = parentEnv.BACKEND_PORT || '8080';

    const apiUrl = `http://${host}:${port}`;

    return {
        envDir: '..',
        plugins: [react()],

                            server: {
                                host: '127.0.0.1',
                            port: Number(parentEnv.FRONTEND_PORT) || 5173,
                            },

                            define: {
                                'import.meta.env.VITE_API_URL': JSON.stringify(apiUrl)
                            }
    }
})
