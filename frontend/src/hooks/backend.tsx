import { api } from '../api';

export type DatabaseResponse = {
    community_id: number | null;
    id: number;
    institution_name: string;
    name: string;
    openalex_id: string;
};

export function useDatabase(): () => Promise<DatabaseResponse> {
    async function fetchData(): Promise<DatabaseResponse> {
        const response = await api.get<any>('/authors');
        return response.data;
    }

    return fetchData;
}
