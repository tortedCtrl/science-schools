// src/hooks/backend.tsx
import { useCallback } from 'react';

import { api } from '../api';

export type CommunityTopic = {
    topic_id: string;
    name: string;
    weight: number;
    article_count: number;
};

export type ApiGraphNode = {
    id: string;
    name: string;
    community_id: number | null;
    communities: { community_id: number }[];
    institution_name: string | null;
    weighted_degree: number;
};

export type ApiGraphLink = {
    source: string;
    target: string;
    weight: number;
};

export type ApiGraphResponse = {
    algorithm: 'leiden' | 'bigclam';
    nodes: ApiGraphNode[] | null;
    links: ApiGraphLink[] | null;
};

export type GraphNode = {
    id: string;
    name: string;
    community_id: number | null;
    type: 'person';
    location: 'RU' | 'INT';
    info?: string;
    val: number;
};

export type GraphLink = {
    source: string;
    target: string;
    weight?: number;
    isHidden: boolean;
};

export type GraphResponse = {
    nodes: GraphNode[];
    links: GraphLink[];
    memberships: Map<string, number[]>;
};

export function useGraph(
    algorithm: 'leiden' | 'bigclam'
): () => Promise<GraphResponse> {
    return useCallback(async (): Promise<GraphResponse> => {
        const res = await api.get<ApiGraphResponse | null>('/graph', {
            params: { algorithm },
        });
        const data = res.data;
        if (!data || !Array.isArray(data.nodes) || !Array.isArray(data.links)) {
            return { nodes: [], links: [], memberships: new Map() };
        }

        const memberships = new Map<string, number[]>();
        const nodes = data.nodes
            .filter(
                (node): node is ApiGraphNode =>
                    !!node &&
                    typeof node.id === 'string'
            )
            .map((node): GraphNode => {
                const id = node.id;
                const communityIds = node.communities
                    .map((community) => community.community_id)
                    .filter(Number.isInteger);
                if (communityIds.length === 0 && node.community_id !== null) {
                    communityIds.push(node.community_id);
                }
                if (communityIds.length > 0) {
                    memberships.set(id, [...new Set(communityIds)]);
                }
                return {
                    id,
                    name: node.name,
                    community_id: communityIds[0] ?? null,
                    type: 'person' as const,
                    location: (node.institution_name ? 'RU' : 'INT') as 'RU' | 'INT',
                    info: node.institution_name ?? undefined,
                    val: 1,
                };
            });

        const links: GraphLink[] = data.links
            .filter(
                (link): link is ApiGraphLink =>
                    !!link &&
                    typeof link.source === 'string' &&
                    typeof link.target === 'string'
            )
            .map((link) => ({
                source: link.source,
                target: link.target,
                weight: link.weight,
                isHidden: false,
            }));
        return { nodes, links, memberships };
    }, [algorithm]);
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


export async function fetchNeighborhood(
    algorithm: 'leiden' | 'bigclam',
    authorId: number,
    depth: number,
    cap: number
): Promise<GraphResponse> {

    const response = await api.get<ApiGraphResponse>(
        '/graph/neighborhood',
        {
            params: {
                algorithm: algorithm,
                author_id: authorId,
                depth: depth,
                v_cap: cap,
            },
        }
    );

    const data = response.data;

    if (!data ||
        !Array.isArray(data.nodes) ||
        !Array.isArray(data.links)) {
        throw new Error('Некорректный ответ сервера');
    }
    const memberships = new Map<string, number[]>();

    const nodes: GraphNode[] = data.nodes.map((node) => {
        const id = String(node.id);

        const communityIds = (node.communities ?? [])
            .map((community) => community.community_id)
            .filter(Number.isInteger);

        if (
            communityIds.length === 0 &&
            node.community_id != null
        ) {
            communityIds.push(node.community_id);
        }

        memberships.set(id, [...new Set(communityIds)]);

        return {
            id,
            name: node.name,
            community_id: communityIds[0] ?? null,
            type: 'person',
            location: node.institution_name ? 'RU' : 'INT',
            info: node.institution_name ?? undefined,
            val: 1,
        };
    });

    const links: GraphLink[] = data.links.map((link) => ({
        source: String(link.source),
        target: String(link.target),
        weight: link.weight,
        isHidden: false,
    }));

    return { nodes, links, memberships };
}

