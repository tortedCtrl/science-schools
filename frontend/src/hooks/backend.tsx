import { api } from '../api';

export type DatabaseResponse = {
    community_id: number | null;
    id: number;
    institution_name: string;
    name: string;
    openalex_id: string;
};

type GraphNode = {
    id: string;
    name: string;
    community_id?: number | null;
    institution_name?: string | null;
};

type GraphLink = {
    source: string;
    target: string;
    weight: number;
};

type GraphResponse = {
    nodes: GraphNode[];
    links: GraphLink[];
};

export function useDatabase(): () => Promise<DatabaseResponse> { // Не соотвествует контракту DatabaseResponse, требует исправления типизации
    async function fetchData(): Promise<DatabaseResponse> {
        const res = await api.get<GraphResponse>('/graph');
        const data = res.data;

        const nodes = (data.nodes ?? []).map((n) => ({
            id: String(n.id),
            name: n.name,
            val: 1,
            type: 'person' as const,
            location: (n.institution_name ? 'RU' : 'INT') as 'RU' | 'INT',
            info: n.institution_name ?? undefined,
        }));

        const links = (data.links ?? []).map((l) => ({
            source: String(l.source),
            target: String(l.target),
            isHidden: false,
        }));

        return { nodes, links };
    }
    return fetchData;
}
