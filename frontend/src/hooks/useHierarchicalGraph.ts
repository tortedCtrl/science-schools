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
  memberships: Map<string, number>,
  expandedCommunities: Set<number>
) {
  return useMemo(() => {
    const displayNodes: HierarchicalGraphNode[] = [];
    const displayLinks: HierarchicalGraphLink[] = [];

    // 1. Группируем авторов по сообществам
    const authorsByCommunity = new Map<number, any[]>();
    const unassignedNodes: any[] = [];

    rawGraph.nodes.forEach(node => {
      const cid = memberships.get(node.id);
      if (cid !== undefined) {
        if (!authorsByCommunity.has(cid)) authorsByCommunity.set(cid, []);
        authorsByCommunity.get(cid)!.push(node);
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
            parentCommunityId: cid,
          });
        });

        authors.forEach(author => {
          displayLinks.push({
            source: anchorId,
            target: author.id,
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
    const communityLinksMap = new Map<string, boolean>();
    rawGraph.links.forEach(link => {
      const s = typeof link.source === 'object' ? link.source.id : link.source;
      const t = typeof link.target === 'object' ? link.target.id : link.target;
      const sCid = memberships.get(s);
      const tCid = memberships.get(t);
      if (sCid !== undefined && tCid !== undefined && sCid !== tCid) {
        const key = sCid < tCid ? `${sCid}-${tCid}` : `${tCid}-${sCid}`;
        communityLinksMap.set(key, true);
      }
    });

    communityLinksMap.forEach((_, key) => {
      const [cid1, cid2] = key.split('-').map(Number);
      const id1 = `community-${cid1}`;
      const id2 = `community-${cid2}`;
      if (visibleNodeIds.has(id1) && visibleNodeIds.has(id2)) {
        displayLinks.push({
          source: id1,
          target: id2,
          isHidden: false,
        });
      }
    });

    // 4. Формируем связи внутри сообществ
    const rawLinks = rawGraph.links || [];

    rawLinks.forEach(link => {
      const s = typeof link.source === 'object' ? link.source.id : link.source;
      const t = typeof link.target === 'object' ? link.target.id : link.target;
      const sCid = memberships.get(s);
      const tCid = memberships.get(t);

      // Пропускаем связи между разными сообществами (они уже агрегированы)
      if (sCid !== undefined && tCid !== undefined && sCid !== tCid) {
        return;
      }

      // 👇 УБРАНО условие, которое отбрасывало связи внутри развёрнутого сообщества
      // (раньше здесь был return, теперь связи внутри hull видны)

      let sourceId = s;
      let targetId = t;

      if (sCid !== undefined && !expandedCommunities.has(sCid)) {
        sourceId = `community-${sCid}`;
      }
      if (tCid !== undefined && !expandedCommunities.has(tCid)) {
        targetId = `community-${tCid}`;
      }

      if (sourceId === targetId) return;

      if (visibleNodeIds.has(sourceId) && visibleNodeIds.has(targetId)) {
        displayLinks.push({
          source: sourceId,
          target: targetId,
          isHidden: link.isHidden,
        });
      }
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