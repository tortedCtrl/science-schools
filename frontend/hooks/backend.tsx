import { api } from '../../api';

export type DatabaseResponse = {
    nodes: {
        id: string;
        name: string;
        val: number;
        type: "person" | "project";
        location: "RU" | "INT" | "HIDDEN";
        info?: string | undefined;
    }[];
    links: {
        source: string;
        target: string;
        isHidden?: boolean | undefined;
    }[];
};

export function useDatabase(): () => Promise<DatabaseResponse> {
    async function fetchData(): Promise<DatabaseResponse> {
        const response = await api.get<DatabaseResponse>('/');
        return response.data;
    }

    return fetchData;
}
