// src/hooks/useHierarchicalGraph.ts
import { useMemo } from 'react';
import type { MutableRefObject } from 'react';

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
  originalNodeId?: string;
  x?: number;
  y?: number;
  // Фиксация узла (d3-force игнорирует интегратор, но связи действуют).
  fx?: number | null;
  fy?: number | null;
}

export interface HierarchicalGraphLink {
  source: string | any;
  target: string | any;
  isHidden?: boolean;
  isInvisible?: boolean;
}

export type PositionCache = MutableRefObject<
Map<string, { x: number; y: number }>
>;

export function useHierarchicalGraph(
  rawGraph: { nodes: any[]; links: any[] },
  memberships: Map<string, number[]>,
  expandedCommunities: Set<number>,
  positionCacheRef: PositionCache
) {
  return useMemo(() => {
    const displayNodes: HierarchicalGraphNode[] = [];
    const displayLinks: HierarchicalGraphLink[] = [];

    // 1. Группируем авторов по сообществам
    const authorsByCommunity = new Map<number, any[]>();
    const unassignedNodes: any[] = [];

    rawGraph.nodes.forEach((node) => {
      const communityIds = memberships.get(node.id) ?? [];
      if (communityIds.length > 0) {
        communityIds.forEach((cid) => {
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
        // Хаб раскрытого сообщества фиксируется через fx/fy.
        // Позиция берётся из кэша (там уже есть позиция свёрнутого
        // community-cid — id совпадает). Если кэша нет — fx/fy не задаём,
        // и onRenderFramePost в App зафиксирует его на первом кадре.
        const anchorPos = positionCacheRef.current.get(anchorId);
        displayNodes.push({
          id: anchorId,
          name: `С#${cid}`,
          isCommunity: true,
          isAnchor: true,
          type: 'community',
          val: 1,
          memberCount,
          ...(anchorPos
          ? { x: anchorPos.x, y: anchorPos.y, fx: anchorPos.x, fy: anchorPos.y }
          : {}),
        });

        authors.forEach((author) => {
          // Авторы стартуют в позиции хаба (если известна),
          // чтобы не «врываться» из случайных мест.
          displayNodes.push({
            ...author,
            id: `${author.id}__community_${cid}`,
            originalNodeId: author.id,
            parentCommunityId: cid,
            ...(anchorPos ? { x: anchorPos.x, y: anchorPos.y } : {}),
          });
        });

        // Невидимые рёбра хаб ↔ участник — держат авторов вокруг хаба.
        authors.forEach((author) => {
          displayLinks.push({
            source: anchorId,
            target: `${author.id}__community_${cid}`,
            isInvisible: true,
            isHidden: false,
          });
        });
      } else {
        // Свёрнутое сообщество — один узел-«шар» без фиксации.
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

    unassignedNodes.forEach((node) => {
      displayNodes.push({ ...node });
    });

    const visibleNodeIds = new Set(displayNodes.map((n) => n.id));

    // 3. Проецируем исходные связи.
    const rawLinks = rawGraph.links || [];

    rawLinks.forEach((link) => {
      const s = typeof link.source === 'object' ? link.source.id : link.source;
      const t = typeof link.target === 'object' ? link.target.id : link.target;
      if (s === t) return;

      const sourceCommunities = memberships.get(s) ?? [];
      const targetCommunities = memberships.get(t) ?? [];

      // Случай 1: оба без сообществ — прямое видимое ребро.
      if (sourceCommunities.length === 0 && targetCommunities.length === 0) {
        if (visibleNodeIds.has(s) && visibleNodeIds.has(t)) {
          displayLinks.push({
            source: s,
            target: t,
            isHidden: link.isHidden,
          });
        }
        return;
      }

      // Случай 2: один без сообщества, другой с сообществом(ами).
      if (sourceCommunities.length === 0) {
        const hubs = targetCommunities.map((cid) => `community-${cid}`);
        hubs.forEach((hubId) => {
          if (hubId !== s && visibleNodeIds.has(s) && visibleNodeIds.has(hubId)) {
            displayLinks.push({
              source: s,
              target: hubId,
              isHidden: link.isHidden,
            });
          }
        });
        return;
      }
      if (targetCommunities.length === 0) {
        const hubs = sourceCommunities.map((cid) => `community-${cid}`);
        hubs.forEach((hubId) => {
          if (hubId !== t && visibleNodeIds.has(t) && visibleNodeIds.has(hubId)) {
            displayLinks.push({
              source: hubId,
              target: t,
              isHidden: link.isHidden,
            });
          }
        });
        return;
      }

      // Случай 3: оба с сообществами.
      const shared = sourceCommunities.filter((cid) =>
      targetCommunities.includes(cid)
      );
      shared.forEach((cid) => {
        if (expandedCommunities.has(cid)) {
          const sId = `${s}__community_${cid}`;
          const tId = `${t}__community_${cid}`;
          if (
            sId !== tId &&
            visibleNodeIds.has(sId) &&
            visibleNodeIds.has(tId)
          ) {
            displayLinks.push({
              source: sId,
              target: tId,
              isHidden: link.isHidden,
              isInvisible: true, // не рисуем, но физика учитывает
            });
          }
        }
      });

      // Межкластерные рёбра — от хаба к хабу.
      sourceCommunities.forEach((cidS) => {
        targetCommunities.forEach((cidT) => {
          if (cidS === cidT) return;
          const sHub = `community-${cidS}`;
          const tHub = `community-${cidT}`;
          if (
            sHub !== tHub &&
            visibleNodeIds.has(sHub) &&
            visibleNodeIds.has(tHub)
          ) {
            displayLinks.push({
              source: sHub,
              target: tHub,
              isHidden: link.isHidden,
            });
          }
        });
      });
    });

    // Удаляем дубликаты.
    const uniqueLinks = Array.from(
      new Map(
        displayLinks.map((l) => {
          if (l.isInvisible) return [`invis-${l.source}-${l.target}`, l];
          const s = typeof l.source === 'object' ? l.source.id : l.source;
          const t = typeof l.target === 'object' ? l.target.id : l.target;
          const key = s < t ? `${s}-${t}` : `${t}-${s}`;
          return [key, l];
        })
      ).values()
    );

    // Применяем кэш позиций к узлам (страховка).
    const cache = positionCacheRef.current;
    const nodesWithPositions = displayNodes.map((n) => {
      const cached = cache.get(n.id);
      if (cached) {
        return {
          ...n,
          x: n.x ?? cached.x,
          y: n.y ?? cached.y,
        };
      }
      return n;
    });

    return { nodes: nodesWithPositions, links: uniqueLinks };
  }, [rawGraph, memberships, expandedCommunities, positionCacheRef]);
}
