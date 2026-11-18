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

type ApiGraphNode = {
    id: number | string;
    name?: string;
    community_id?: number | null;
    communities?: ({ community_id: number } | number)[] | null;
    institution_name?: string | null;
};

type ApiGraphLink = {
    source: number | string;
    target: number | string;
};

type ApiGraphResponse = {
    nodes: ApiGraphNode[] | null;
    links: ApiGraphLink[] | null;
};

export type GraphResponse = {
    nodes: Record<string, unknown>[];
    links: { source: string; target: string; isHidden: boolean }[];
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
                    (typeof node.id === 'number' || typeof node.id === 'string')
            )
            .map((node) => {
                const id = String(node.id);
                const communityIds = Array.isArray(node.communities)
                    ? node.communities
                          .map((community) =>
                              typeof community === 'number'
                                  ? community
                                  : community?.community_id
                          )
                          .filter(
                              (communityId): communityId is number =>
                                  Number.isInteger(communityId)
                          )
                    : [];
                if (
                    communityIds.length === 0 &&
                    typeof node.community_id === 'number' &&
                    Number.isInteger(node.community_id)
                ) {
                    communityIds.push(node.community_id);
                }
                if (communityIds.length > 0) {
                    memberships.set(id, [...new Set(communityIds)]);
                }
                return {
                    id,
                    name: typeof node.name === 'string' ? node.name : `Автор ${id}`,
                    community_id: communityIds[0] ?? null,
                    type: 'person' as const,
                    location: (node.institution_name ? 'RU' : 'INT') as 'RU' | 'INT',
                    info: node.institution_name ?? undefined,
                    val: 1,
                };
            });

        const links = data.links
            .filter(
                (link): link is ApiGraphLink =>
                    !!link &&
                    (typeof link.source === 'number' ||
                        typeof link.source === 'string') &&
                    (typeof link.target === 'number' ||
                        typeof link.target === 'string')
            )
            .map((link) => ({
                source: String(link.source),
                target: String(link.target),
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
