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
    val?: number;
    type?: 'person' | 'project' | 'article';
    location?: 'RU' | 'INT';
    info?: string;
};

type GraphLink = {
    source: string;
    target: string;
    isHidden: boolean;
};

type GraphResponse = {
    nodes: GraphNode[];
    links: GraphLink[];
};

// ==== БЫСТРЫЙ МОК ====
const USE_MOCK = true; // поставь false, чтобы вернуться к реальному API

const MOCK: GraphResponse = {
    nodes: [
        { id: '1', name: 'OpenAlex', type: 'project', location: 'INT', community_id: 1, val: 1 },
        { id: '2', name: 'SPBU', type: 'project', location: 'RU', community_id: 1, val: 1, info: 'SPbU' },
        { id: '3', name: 'Science Schools', type: 'project', location: 'RU', community_id: 2, val: 1 },
        { id: '4', name: 'Researcher A', type: 'person', location: 'RU', community_id: 2, val: 1, info: 'SPbU' },
        { id: '5', name: 'Researcher B', type: 'person', location: 'INT', community_id: 3, val: 1 },
        { id: '6', name: 'MIT', type: 'project', location: 'INT', community_id: 3, val: 1 },
        { id: '7', name: 'Article X', type: 'article', location: 'INT', community_id: 4, val: 1 },
        { id: '8', name: 'Article Y', type: 'article', location: 'RU', community_id: 4, val: 1 },
        { id: '9', name: 'Researcher C', type: 'person', location: 'RU', community_id: 1, val: 1 },
    ],
    links: [
        { source: '1', target: '2', isHidden: false },
        { source: '1', target: '3', isHidden: false },
        { source: '2', target: '3', isHidden: false },
        { source: '3', target: '4', isHidden: false },
        { source: '3', target: '5', isHidden: false },
        { source: '4', target: '7', isHidden: true },
        { source: '5', target: '6', isHidden: false },
        { source: '6', target: '7', isHidden: false },
        { source: '2', target: '5', isHidden: false },
        { source: '1', target: '9', isHidden: false },
        { source: '9', target: '8', isHidden: true },
        { source: '4', target: '8', isHidden: false },
    ],
};
// ==== /БЫСТРЫЙ МОК ====

export function useDatabase(): () => Promise<GraphResponse> {
    async function fetchData(): Promise<GraphResponse> {
        if (USE_MOCK) {
            await new Promise((r) => setTimeout(r, 200));
            return structuredClone(MOCK);
        }

        const res = await api.get<GraphResponse>('/graph');
        const data = res.data;

        const nodes: GraphNode[] = (data.nodes ?? []).map((n) => ({
            id: String(n.id),
            name: n.name,
            community_id: n.community_id ?? null,
            val: 1,
            type: 'person' as const,
            location: (n.institution_name ? 'RU' : 'INT') as 'RU' | 'INT',
            info: n.institution_name ?? undefined,
        }));

        const links: GraphLink[] = (data.links ?? []).map((l) => ({
            source: String(l.source),
            target: String(l.target),
            isHidden: false,
        }));

        return { nodes, links };
    }
    return fetchData;
}