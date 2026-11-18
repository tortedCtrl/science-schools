// src/hooks/backend.tsx
import { useCallback } from 'react';

import { api } from '../api';

export type CommunityTopic = {
    topic_id: string;
    name: string;
    weight: number;
    article_count: number;
};

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
    //weight: number;
    isHidden: boolean;
};

type GraphResponse = {
    nodes: GraphNode[] | null;
    links: GraphLink[] | null;
};

export function useDatabase(): () => Promise<GraphResponse> {
    return useCallback(async (): Promise<GraphResponse> => {
        const res = await api.get<GraphResponse | null>('/graph');
        const data = res.data;
        if (!data || !Array.isArray(data.nodes) || !Array.isArray(data.links)) {
            return { nodes: [], links: [] };
        }

        const nodes = data.nodes
            .filter((n) => n && (typeof n.id === 'number' || typeof n.id === 'string'))
            .map((n) => ({
            id: String(n.id),
            name: typeof n.name === 'string' ? n.name : `Автор ${n.id}`,
            community_id: n.community_id ?? null,
            val: 1,
            type: 'person' as const,
            location: (n.institution_name ? 'RU' : 'INT') as 'RU' | 'INT',
            info: n.institution_name ?? undefined,
        }));

        const links = data.links.map((l) => ({
            source: String(l.source),
            target: String(l.target),
            isHidden: false,
        }));
        return { nodes, links };
    }, []);
}

export function useCommunityTopics(
    algorithm: 'leiden' | 'bigclam',
    communityId: number | null
): () => Promise<CommunityTopic[]> {
    return useCallback(async () => {
        if (communityId == null) return [];
        const response = await api.get<CommunityTopic[]>(
            `/communities/${algorithm}/${communityId}/topics`
        );
        return response.data;
    }, [algorithm, communityId]);
}
