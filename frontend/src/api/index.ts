import axios, { type AxiosRequestConfig } from 'axios';

const apiBaseUrl = (import.meta.env.VITE_API_URL as string | undefined ?? 'http://localhost:8080').replace(/\/$/, '');

const httpClient = axios.create({
  baseURL: apiBaseUrl,
  headers: {
    'Content-Type': 'application/json',
    Accept: 'application/json',
  },
});

export const api = {
  get: <T = unknown>(url = '/api', config?: AxiosRequestConfig) =>
    httpClient.get<T>(url, config),

  post: <T = unknown>(url = '/api', data?: unknown, config?: AxiosRequestConfig) =>
    httpClient.post<T>(url, data, config),
};
