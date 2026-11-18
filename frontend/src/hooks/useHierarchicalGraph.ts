// src/hooks/useHierarchicalGraph.ts
import { useMemo } from 'react';

export interface HierarchicalGraphNode {
  id: string;
  name: string;
  val?: number;
  type: 'person' | 'community';
  location?: 'RU' | 'INT';
  info?: string;
  isCommunity?: boolean;
  isAnchor?: boolean;
  memberCount?: number;
  parentCommunityId?: number;
  x?: number;
  y?: number;
}

export interface HierarchicalGraphLink {
  source: string | any;
  target: string | any;
  isHidden?: boolean;
  isInvisible?: boolean;
}

export function useHierarchicalGraph(
  rawGraph: { nodes: any[]; links: any[] },
  memberships: Map<string, number[]>,
  expandedCommunities: Set<number>
) {
  return useMemo(() => {
    const displayNodes: HierarchicalGraphNode[] = [];
    const displayLinks: HierarchicalGraphLink[] = [];

    // 1. Группируем авторов по сообществам
    const authorsByCommunity = new Map<number, any[]>();
    const unassignedNodes: any[] = [];

    rawGraph.nodes.forEach(node => {
      const communityIds = memberships.get(node.id) ?? [];
      if (communityIds.length > 0) {
        communityIds.forEach(cid => {
          if (!authorsByCommunity.has(cid)) authorsByCommunity.set(cid, []);
          authorsByCommunity.get(cid)!.push(node);
        });
      } else {
        unassignedNodes.push(node);
      }
    });

    // 2. Формируем узлы сообществ
    authorsByCommunity.forEach((authors, cid) => {
      const isExpanded = expandedCommunities.has(cid);
      const anchorId = `community-${cid}`;
      const memberCount = authors.length;

      if (isExpanded) {
        displayNodes.push({
          id: anchorId,
          name: `С#${cid}`,
          isCommunity: true,
          isAnchor: true,
          type: 'community',
          val: 1,
          memberCount,
          x: 0, y: 0,
        });

        authors.forEach(author => {
          displayNodes.push({
            ...author,
            id: `${author.id}__community_${cid}`,
            originalNodeId: author.id,
            parentCommunityId: cid,
          });
        });

        authors.forEach(author => {
          displayLinks.push({
            source: anchorId,
            target: `${author.id}__community_${cid}`,
            isInvisible: true,
            isHidden: false,
          });
        });
      } else {
        displayNodes.push({
          id: anchorId,
          name: `Сообщество #${cid}`,
          isCommunity: true,
          isAnchor: false,
          type: 'community',
          memberCount,
          val: 1,
          location: 'RU',
        });
      }
    });

    unassignedNodes.forEach(node => {
      displayNodes.push({ ...node });
    });

    const visibleNodeIds = new Set(displayNodes.map(n => n.id));

    // 3. Агрегация связей между сообществами (через общие проекты)
    // 3. Проецируем исходные связи на узлы сообществ и раскрытых авторов.
    const rawLinks = rawGraph.links || [];

    rawLinks.forEach(link => {
      const s = typeof link.source === 'object' ? link.source.id : link.source;
      const t = typeof link.target === 'object' ? link.target.id : link.target;
      const sourceCommunities = memberships.get(s) ?? [];
      const targetCommunities = memberships.get(t) ?? [];
      const sourceIds =
        sourceCommunities.length === 0
          ? [s]
          : sourceCommunities.map(cid =>
              expandedCommunities.has(cid)
                ? `${s}__community_${cid}`
                : `community-${cid}`
            );
      const targetIds =
        targetCommunities.length === 0
          ? [t]
          : targetCommunities.map(cid =>
              expandedCommunities.has(cid)
                ? `${t}__community_${cid}`
                : `community-${cid}`
            );

      sourceIds.forEach(sourceId => {
        targetIds.forEach(targetId => {
          if (
            sourceId !== targetId &&
            visibleNodeIds.has(sourceId) &&
            visibleNodeIds.has(targetId)
          ) {
            displayLinks.push({
              source: sourceId,
              target: targetId,
              isHidden: link.isHidden,
            });
          }
        });
      });
    });

    // Удаляем дубликаты
    const uniqueLinks = Array.from(
      new Map(displayLinks.map(l => {
        if (l.isInvisible) return [`invis-${l.source}-${l.target}`, l];
        const s = typeof l.source === 'object' ? l.source.id : l.source;
        const t = typeof l.target === 'object' ? l.target.id : l.target;
        return [`${s}-${t}`, l];
      })).values()
    );

    return { nodes: displayNodes, links: uniqueLinks };
  }, [rawGraph, memberships, expandedCommunities]);
}