import React, {
  useEffect,
  useLayoutEffect,
  useRef,
  useState,
} from 'react';
import * as d3 from 'd3-force';
import {
    useCommunityTopics,
    useGraph,
    fetchNeighborhood,
    searchAuthors,
    type CommunityTopic,
} from './hooks/backend';
import { useHierarchicalGraph } from './hooks/useHierarchicalGraph';
import { SearchPanel, type AlgorithmMode, type ColorMode } from './components/SearchPanel';
import { InfoPanel } from './components/InfoPanel';
import { GraphView } from './components/GraphView';
import {
  drawCommunityHulls,
  getCommunityHullBounds,
} from './components/CommunityHull';
import { getLinkId } from './utils/graph';
import './App.css';

const getVisualHullRadius = (memberCount: number) =>
50 + Math.sqrt(memberCount) * 7;

const getHullRadius = (memberCount: number) =>
60 + Math.sqrt(memberCount) * 7;

const getVisualRadius = (memberCount: number) =>
Math.min(18, 12 + memberCount * 1.5);

const getCollapsedCollisionRadius = (memberCount: number) =>
getVisualRadius(memberCount) * 1.5 + 20;

// --- Пороги масштаба ---
const NODE_LABEL_MIN_SCALE = 0.8;
const COMMUNITY_LABEL_MIN_SCALE = 0.5;
const MIN_ZOOM_SCALE = 0.07;
const MAX_ZOOM_SCALE = 10;
const INITIAL_ZOOM_SCALE = 1.2;

// Сколько миллисекунд держим жёсткую заморозку после пересборки графа.
// Должно быть >= cooldownTime в GraphView (3000): к моменту снятия фиксации
// альфа симуляции уже остыла, поэтому снятие не вызывает рывка.
const FROZEN_RELEASE_MS = 3500;

// Сколько кадров после пересборки дополнительно переутверждаем позиции
// прямо в onRenderFramePost — страховка на случай, если force-graph
// применил новые данные позже эффекта стабилизации.
const STABILIZE_FRAMES = 10;

export default function App() {
  const fetchLeidenData = useGraph('leiden');
  const fetchBigClamData = useGraph('bigclam');

  const [algorithmMode, setAlgorithmMode] =
  useState<AlgorithmMode>('leiden');

  const [searchName, setSearchName] = useState('');
  const [showAll, setShowAll] = useState(true);
  const [searchDepth, setSearchDepth] = useState(1);
  const [vCap, setVCap] = useState<number | ''>(100);
  const [loading, setLoading] = useState(false);
  const [graphStatus, setGraphStatus] = useState<
  'loading' | 'ready' | 'empty' | 'error'
  >('loading');

  const [showHiddenLinks, setShowHiddenLinks] = useState(false);
  const [colorMode, setColorMode] = useState<ColorMode>('type');
  const [highlightCluster, setHighlightCluster] = useState(false);
  const [communityTopics, setCommunityTopics] = useState<CommunityTopic[]>([]);
  const [communityTopicsLoading, setCommunityTopicsLoading] = useState(false);
  const [communityTopicsError, setCommunityTopicsError] = useState<
  string | null
  >(null);
  const communityTopicsRequestId = useRef(0);

  const [fullGraphData, setFullGraphData] = useState<{
    nodes: any[];
    links: any[];
  }>({ nodes: [], links: [] });
  const [graphData, setGraphData] = useState<{
    nodes: any[];
    links: any[];
  }>({ nodes: [], links: [] });
  const [searchResult, setSearchResult] = useState<{
    nodes: any[];
    links: any[];
  } | null>(null);
  const [searchError, setSearchError] = useState<string | null>(null);
  const [rawGraph, setRawGraph] = useState<{
    nodes: any[];
    links: any[];
  }>({ nodes: [], links: [] });
  const [memberships, setMemberships] =
  useState<Map<string, number[]>>(new Map());
  const [expandedCommunities, setExpandedCommunities] =
  useState<Set<number>>(new Set());

  const [selectedNode, setSelectedNode] = useState<any>(null);
  const [leftOpen, setLeftOpen] = useState(true);
  const [rightOpen, setRightOpen] = useState(true);

  const selectedCommunityId =
  typeof selectedNode?.community_id === 'number'
  ? selectedNode.community_id
  : null;
  const fetchCommunityTopics = useCommunityTopics(
    algorithmMode,
    selectedCommunityId
  );

  const viewportRef = useRef<HTMLDivElement>(null!);
  const graphRef = useRef<any>(null);
  const [viewportSize, setViewportSize] = useState({ w: 0, h: 0 });

  // Кэш позиций узлов: id → {x, y}. Живёт в App, чтобы:
  //  - useHierarchicalGraph читал его при пересборке;
  //  - handleBigClamNodeClick писал туда позиции ВСЕХ узлов и кольцо новичков;
  //  - стабилизация возвращала всем узлам кэшированные координаты с fx/fy;
  //  - onRenderFramePost обновлял позиции якорей из живого графа.
  const positionCacheRef = useRef<Map<string, { x: number; y: number }>>(
    new Map()
  );

  // Таймер снятия заморозки после пересборки.
  const frozenReleaseTimerRef = useRef<number | null>(null);

  // Счётчик кадров, в которые дополнительно переутверждаем позиции.
  const stabilizeFramesRef = useRef(0);

  // Флаг «стартовый зум уже сделан» — не даём камере прыгать при каждом
  // раскрытии/сворачивании сообщества (главный источник видимых рывков).
  const zoomDoneRef = useRef(false);

  // Ключ кэша для узла: scene-id или originalNodeId — на случай, если хук
  // переименовал id участника при пересборке.
  const cacheKeyFor = (n: any): string | null => {
    if (n?.id != null && positionCacheRef.current.has(String(n.id))) {
      return String(n.id);
    }
    if (
      n?.originalNodeId != null &&
      positionCacheRef.current.has(String(n.originalNodeId))
    ) {
      return String(n.originalNodeId);
    }
    return null;
  };

  // Возвращает всем узлам кэшированные позиции и ЖЁСТКО фиксирует их (fx/fy).
  // Узлы без кэша ставим у центроида сцены — вместо случайного разброса d3.
  const applyStabilization = (nodes: any[]) => {
    if (!nodes.length || positionCacheRef.current.size === 0) return;

    let cx = 0;
    let cy = 0;
    for (const p of positionCacheRef.current.values()) {
      cx += p.x;
      cy += p.y;
    }
    const cacheSize = positionCacheRef.current.size;
    cx /= cacheSize;
    cy /= cacheSize;

    for (const node of nodes) {
      const key = cacheKeyFor(node);
      const pos = key ? positionCacheRef.current.get(key)! : null;
      const target = pos ?? {
        x: cx + (Math.random() - 0.5) * 120,
        y: cy + (Math.random() - 0.5) * 120,
      };
      if (!pos) {
        positionCacheRef.current.set(String(node.id), target);
      }
      node.x = target.x;
      node.y = target.y;
      node.fx = target.x;
      node.fy = target.y;
      node.vx = 0;
      node.vy = 0;
    }
  };

  useLayoutEffect(() => {
    const el = viewportRef.current;
    if (!el) return;

    const update = () => {
      const r = el.getBoundingClientRect();
      if (r.width <= 1 || r.height <= 1) return;
      setViewportSize({
        w: Math.max(1, Math.floor(r.width)),
                      h: Math.max(1, Math.floor(r.height)),
      });
    };

    const ro = new ResizeObserver(update);
    ro.observe(el);
    update();
    const frameIds: number[] = [];
    frameIds.push(requestAnimationFrame(update));
    frameIds.push(requestAnimationFrame(() => {
      update();
      frameIds.push(requestAnimationFrame(update));
    }));
    window.addEventListener('resize', update);

    return () => {
      ro.disconnect();
      frameIds.forEach((frameId) => cancelAnimationFrame(frameId));
      window.removeEventListener('resize', update);
    };
  }, [algorithmMode]);

  const reloadLeidenGraph = async (): Promise<{
    nodes: any[];
    links: any[];
  }> => {
    setLoading(true);
    setGraphStatus('loading');
    try {
      const data = await fetchLeidenData();
      const nodes = Array.isArray(data?.nodes) ? data.nodes : [];
      const links = Array.isArray(data?.links) ? data.links : [];
      const full = { nodes, links };
      setFullGraphData(full);
      setGraphData(full);
      setSelectedNode(null);
      setGraphStatus(full.nodes.length === 0 ? 'empty' : 'ready');
      return full;
    } catch (error) {
      console.error('Leiden graph API error:', error);
      const empty = { nodes: [], links: [] };
      setFullGraphData(empty);
      setGraphData(empty);
      setGraphStatus('error');
      return empty;
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    let isMounted = true;

    async function loadMode() {
      setLoading(true);
      setGraphStatus('loading');
      setSelectedNode(null);
      zoomDoneRef.current = false;
      positionCacheRef.current.clear();

      try {
        if (algorithmMode === 'leiden') {
          await reloadLeidenGraph();
          return;
        }

        const { memberships: membershipMap, nodes, links } =
        await fetchBigClamData();
        if (!isMounted) return;

        setMemberships(membershipMap);
        setExpandedCommunities(new Set());
        setRawGraph({ nodes, links });
        setGraphStatus(
          nodes.length === 0 || membershipMap.size === 0 ? 'empty' : 'ready'
        );
      } catch (error) {
        console.error('BigCLAM graph error:', error);
        setMemberships(new Map());
        setRawGraph({ nodes: [], links: [] });
        setGraphStatus('error');
      } finally {
        if (isMounted) setLoading(false);
      }
    }

    loadMode();
    return () => {
      isMounted = false;
    };
  }, [algorithmMode, fetchBigClamData]);

  const displayBigClamGraph = useHierarchicalGraph(
    rawGraph,
    memberships,
    expandedCommunities,
    positionCacheRef
  );
  const currentGraphData =
    searchResult ??
    (algorithmMode === 'bigclam'
      ? displayBigClamGraph
      : graphData);
  const toggleHighlightCluster = () => {
    const enabled = !highlightCluster;
    setHighlightCluster(enabled);

    if (!enabled) {
      setCommunityTopicsLoading(false);
      setCommunityTopicsError(null);
    }
  };

  useEffect(() => {
    communityTopicsRequestId.current += 1;
    const requestId = communityTopicsRequestId.current;
    setCommunityTopics([]);
    setCommunityTopicsError(null);
    if (selectedCommunityId == null) {
      setCommunityTopicsLoading(false);
      return;
    }

    setCommunityTopicsLoading(true);
    fetchCommunityTopics()
    .then((topics) => {
      if (requestId === communityTopicsRequestId.current) {
        setCommunityTopics(topics);
      }
    })
    .catch((error: unknown) => {
      console.error('Community topics API error:', error);
      if (requestId === communityTopicsRequestId.current) {
        setCommunityTopicsError('Не удалось загрузить темы научных школ.');
      }
    })
    .finally(() => {
      if (requestId === communityTopicsRequestId.current) {
        setCommunityTopicsLoading(false);
      }
    });
  }, [algorithmMode, selectedCommunityId, fetchCommunityTopics]);

  
  const handleGraphSearch = async (event?: React.FormEvent) => {
    event?.preventDefault();
    setSearchError(null);

    if (showAll) {
      setSearchResult(null);
      setSelectedNode(null);
      return;
    }

    const query = searchName.trim();
    if (!query) {
      setSearchError('Введите имя или ID автора');
      return;
    }

    const cap = vCap === '' ? 1000 : vCap;

    if (!Number.isInteger(cap) || cap < 1 || cap > 1000) {
      setSearchError('V_cap должен быть от 1 до 1000');
      return;
    }

    setSearchResult(null);
    setSelectedNode(null);
    setLoading(true);

    try {
      const authors = await searchAuthors(query);

      if (authors.length === 0) {
        setSearchError('Автор не найден в базе данных');
        return;
      }

      if (authors.length > 1) {
        const suggestions = authors
          .slice(0, 5)
          .map((author) => `${author.id} — ${author.name}`)
          .join('; ');

        setSearchError(
          `Найдено несколько авторов: ${suggestions}. Введите нужный ID.`
        );
        return;
      }

      const authorId = authors[0].id;

      const result = await fetchNeighborhood(
        algorithmMode,
        authorId,
        searchDepth,
        cap
      );

      setSearchResult({
        nodes: result.nodes,
        links: result.links,
      });

      const selected = result.nodes.find(
        (node) => node.id === String(authorId)
      );

      setSelectedNode(selected ?? null);

      setGraphStatus(result.nodes.length > 0 ? 'ready' : 'empty');

    } catch (error) {
      console.error('Author search / neighborhood API error:', error);
      setSearchError('Не удалось выполнить поиск на сервере');
    } finally {
      setLoading(false);
    }
  };

  const toggleHiddenLinks = (checked: boolean) => {
    setShowHiddenLinks(checked);
    if (algorithmMode === 'bigclam') return;

    const baseLinks = checked
    ? fullGraphData.links
    : fullGraphData.links.filter((link: any) => !link.isHidden);

    if (showAll || !searchName.trim()) {
      setGraphData({ nodes: fullGraphData.nodes, links: baseLinks });
    } else {
      const nodeIds = new Set(graphData.nodes.map((node: any) => node.id));
      const filteredLinks = baseLinks.filter((link: any) => {
        const source = getLinkId(link.source);
        const target = getLinkId(link.target);
        return nodeIds.has(source) && nodeIds.has(target);
      });
      setGraphData((previous) => ({
        ...previous,
        links: filteredLinks,
      }));
    }
  };

  const handleShowAllChange = (checked: boolean) => {
    setShowAll(checked);
    setSearchError(null);
    if (checked) {
        setSearchResult(null);
        setSelectedNode(null);
    }
};

  // Физика BigCLAM. Силы фиксируем по rawGraph.nodes.length,
  // чтобы раскрытие/сворачивание не меняло глобальную физику.
  useEffect(() => {
    if (algorithmMode !== 'bigclam') return;
    const graph = graphRef.current;
    if (!graph) return;

    const nodeCount = rawGraph.nodes.length;
    const chargeStrength = -Math.max(250, Math.min(1200, nodeCount * 0.6));
    const baseLinkDistance = Math.max(
      100,
      Math.min(400, Math.sqrt(nodeCount) * 12)
    );

    graph.d3Force('charge')?.strength(chargeStrength);

    graph.d3Force('link')
    ?.distance((link: any) => {
      if (link.isInvisible) {
        const sourceId =
        typeof link.source === 'object' ? link.source.id : link.source;
        const communityId = Number(
          String(sourceId).replace('community-', '')
        );
        if (Number.isFinite(communityId)) {
          const bounds = getCommunityHullBounds(
            displayBigClamGraph.nodes,
            communityId,
            getHullRadius
          );
          return Math.max(24, bounds.radius * 0.7);
        }
        return baseLinkDistance;
      }
      return baseLinkDistance;
    })
    .strength((link: any) => {
      if (link.isInvisible) return 0.05;
      return nodeCount > 1000 ? 0.15 : nodeCount > 300 ? 0.25 : 0.4;
    });

    graph.d3Force(
      'collision',
      d3
      .forceCollide((node: any) => {
        if (node.isCommunity && !node.isAnchor) {
          return getCollapsedCollisionRadius(node.memberCount || 0);
        }
        if (node.isAnchor) {
          const communityId = Number(
            String(node.id).replace('community-', '')
          );
          if (Number.isFinite(communityId)) {
            const bounds = getCommunityHullBounds(
              displayBigClamGraph.nodes,
              communityId,
              getHullRadius
            );
            return bounds.radius + 30;
          }
          return getHullRadius(node.memberCount || 0) + 30;
        }
        return 8;
      })
      .strength(1)
      .iterations(2)
    );
  }, [algorithmMode, expandedCommunities, displayBigClamGraph.nodes, rawGraph.nodes.length]);

  // Стабилизация сцены после пересборки иерархии.
  // Пересборка создаёт НОВЫЕ объекты узлов: без позиций d3 раскидывает их
  // случайно, а force-graph перезапускает симуляцию. Поэтому:
  //   1) всем узлам с кэшем возвращаем позицию и жёстко фиксируем fx/fy;
  //   2) узлы без кэша ставим у центроида (не даём случайного разброса);
  //   3) первые кадры дублируем фиксацию в onRenderFramePost;
  //   4) после остывания физики снимаем фиксацию — двигаться уже нечему.
  useEffect(() => {
    if (algorithmMode !== 'bigclam') return;
    const graph = graphRef.current;
    if (!graph) return;
    const nodes: any[] = graph.graphData?.()?.nodes ?? [];
    if (!nodes.length) return;

    applyStabilization(nodes);

    if (positionCacheRef.current.size > 0) {
      stabilizeFramesRef.current = STABILIZE_FRAMES;
    }

    if (frozenReleaseTimerRef.current != null) {
      window.clearTimeout(frozenReleaseTimerRef.current);
    }
    frozenReleaseTimerRef.current = window.setTimeout(() => {
      const live: any[] = graphRef.current?.graphData?.()?.nodes ?? [];
      for (const m of live) {
        if (!m.isAnchor) {
          m.fx = null;
          m.fy = null;
        }
      }
    }, FROZEN_RELEASE_MS);
  }, [displayBigClamGraph, algorithmMode]);

  const handleBigClamNodeClick = (node: any) => {
    if (node.isCommunity) {
      const communityId = parseInt(node.id.split('-')[1], 10);
      const anchorId = `community-${communityId}`;

      const liveNodes = graphRef.current?.graphData?.()?.nodes;
      const liveList = Array.isArray(liveNodes) ? liveNodes : [];

      // 1. Кэшируем позиции ВСЕХ живых узлов до пересборки.
      for (const n of liveList) {
        if (typeof n.x === 'number' && typeof n.y === 'number') {
          positionCacheRef.current.set(String(n.id), {
            x: n.x,
            y: n.y,
          });
        }
      }

      const wasExpanded = expandedCommunities.has(communityId);
      const next = new Set(expandedCommunities);
      if (wasExpanded) next.delete(communityId);
      else next.add(communityId);

      // 2. При РАСКРЫТИИ заранее раскладываем участников кольцом вокруг якоря
      //    прямо в кэш — до пересборки графа. Тогда ни хук, ни стабилизатор
      //    не дадут новичкам случайных стартовых позиций.
      if (!wasExpanded) {
        let anchorPos = positionCacheRef.current.get(anchorId);
        if (!anchorPos) {
          const liveAnchor = liveList.find(
            (n: any) => String(n.id) === anchorId
          );
          if (liveAnchor && typeof liveAnchor.x === 'number') {
            anchorPos = { x: liveAnchor.x, y: liveAnchor.y };
            positionCacheRef.current.set(anchorId, anchorPos);
          }
        }

        if (anchorPos) {
          const onScene = new Set(liveList.map((n: any) => String(n.id)));
          const newcomers: string[] = [];
          for (const [id, comms] of memberships) {
            if (comms.includes(communityId) && !onScene.has(id)) {
              newcomers.push(id);
            }
          }

          const total = Math.max(1, newcomers.length);
          const ringRadius =
          getVisualHullRadius(node.memberCount || newcomers.length) * 0.8;
          const phase = ((communityId * 137.508) / 180) * Math.PI;

          newcomers.forEach((id, idx) => {
            const angle = phase + (idx / total) * 2 * Math.PI;
            const jitter = 0.75 + Math.random() * 0.5;
            positionCacheRef.current.set(id, {
              x: anchorPos!.x + Math.cos(angle) * ringRadius * jitter,
                                         y: anchorPos!.y + Math.sin(angle) * ringRadius * jitter,
            });
          });
        }
      }

      setSelectedNode({
        ...node,
        community_id: communityId,
        type: 'community',
      });
      setRightOpen(true);
      setExpandedCommunities(next);
      return;
    }

    setSelectedNode({
      ...node,
      id: node.originalNodeId ?? node.id,
      community_id:
      node.parentCommunityId ??
      memberships.get(node.originalNodeId ?? node.id)?.[0] ??
      node.community_id,
    });
    setRightOpen(true);
  };

  const bigClamNodeCanvasObject = (
    node: any,
    ctx: CanvasRenderingContext2D,
    globalScale: number
  ) => {
    const posX = node.x ?? 0;
    const posY = node.y ?? 0;
    const fontSize = Math.max(4, 12 / globalScale);

    if (node.isAnchor) {
      ctx.beginPath();
      ctx.arc(posX, posY, 8, 0, 2 * Math.PI);
      ctx.fillStyle = '#eb2f96';
      ctx.fill();
      ctx.strokeStyle = '#ffffff';
      ctx.lineWidth = 2 / globalScale;
      ctx.stroke();
      ctx.fillStyle = '#fff';
      ctx.font = `bold ${7 / globalScale}px sans-serif`;
      ctx.textAlign = 'center';
      ctx.textBaseline = 'middle';
      ctx.fillText('×', posX, posY);
      return;
    }

    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';

    if (node.isCommunity) {
      const radius = getVisualRadius(node.memberCount || 0);
      ctx.beginPath();
      ctx.arc(posX, posY, radius, 0, 2 * Math.PI);
      ctx.fillStyle = '#eb2f96';
      ctx.fill();
      ctx.strokeStyle = '#ffffff';
      ctx.lineWidth = 2 / globalScale;
      ctx.stroke();

      if (globalScale >= COMMUNITY_LABEL_MIN_SCALE) {
        ctx.fillStyle = '#fff';
        ctx.font = `bold ${fontSize * 0.9}px sans-serif`;
        ctx.fillText(node.name || '', posX, posY - 2);
        if (node.memberCount) {
          ctx.font = `${fontSize * 0.7}px sans-serif`;
          ctx.fillText(`(${node.memberCount})`, posX, posY + 8 / globalScale);
        }
      }
      return;
    }

    ctx.beginPath();
    ctx.arc(posX, posY, 6, 0, 2 * Math.PI);
    ctx.fillStyle = '#1890ff';
    ctx.fill();
    ctx.strokeStyle = '#ffffff';
    ctx.lineWidth = 1.5 / globalScale;
    ctx.stroke();

    if (globalScale >= NODE_LABEL_MIN_SCALE) {
      ctx.fillStyle = '#fff';
      ctx.font = `${fontSize}px sans-serif`;
      ctx.fillText(node.name || '', posX, posY + 12 / globalScale);
    }
  };

  // Стартовый зум — ОДИН раз за загрузку данных.
  // Раньше эффект зависел от currentGraphData и камера заново прыгала к 1.2
  // при каждом раскрытии сообщества — это выглядело как телепортация сцены.
  useEffect(() => {
    if (graphStatus === 'loading') {
      zoomDoneRef.current = false;
      return;
    }
    if (graphStatus !== 'ready' || zoomDoneRef.current) return;
    const graph = graphRef.current;
    if (!graph) return;

    const timer = window.setTimeout(() => {
      graph.zoomTo?.(INITIAL_ZOOM_SCALE, 400);
      zoomDoneRef.current = true;
    }, 600);

    return () => window.clearTimeout(timer);
  }, [graphStatus, algorithmMode]);

  return (
    <div className="app-container">
      <SearchPanel
        searchName={searchName}
        setSearchName={setSearchName}
        showAll={showAll}
        onShowAllChange={handleShowAllChange}
        searchDepth={searchDepth}
        setSearchDepth={setSearchDepth}
        vCap={vCap}
        setVCap={setVCap}
        loading={loading}
        onSubmit={handleGraphSearch}
        showHiddenLinks={showHiddenLinks}
        onToggleHiddenLinks={toggleHiddenLinks}
        colorMode={colorMode}
        setColorMode={setColorMode}
        highlightCluster={highlightCluster}
        onToggleHighlightCluster={toggleHighlightCluster}
        algorithmMode={algorithmMode}
        onAlgorithmModeChange={(mode) => {
          if (mode === algorithmMode) return;
          setAlgorithmMode(mode);
          setSearchResult(null);
          setSearchError(null);
          setShowAll(true);
          setSelectedNode(null);
          setExpandedCommunities(new Set());
          setRawGraph({ nodes: [], links: [] });
          setMemberships(new Map());
          setGraphData({ nodes: [], links: [] });
          setFullGraphData({ nodes: [], links: [] });
        }}
        leftOpen={leftOpen}
        onToggleLeft={() => setLeftOpen((value) => !value)}
      />

    <div className="graph-stage">
    <GraphView
    key={`graph-${algorithmMode}`}
    graphData={currentGraphData}
    viewportSize={viewportSize}
    viewportRef={viewportRef}
    graphRef={graphRef}
    colorMode={colorMode}
    highlightCluster={highlightCluster}
    minZoom={MIN_ZOOM_SCALE}
    maxZoom={MAX_ZOOM_SCALE}
    nodeCanvasObject={
      algorithmMode === 'bigclam'
      ? bigClamNodeCanvasObject
      : undefined
    }
    onRenderFramePost={
      algorithmMode === 'bigclam'
        ? (ctx: CanvasRenderingContext2D) => {
            if (searchResult !== null) return;

            if (stabilizeFramesRef.current > 0) {
              stabilizeFramesRef.current -= 1;
              const liveNodes =
                graphRef.current?.graphData?.()?.nodes;
              if (Array.isArray(liveNodes)) {
                applyStabilization(liveNodes);
              }
            }

            drawCommunityHulls({
              ctx,
              nodes: displayBigClamGraph.nodes,
              expandedCommunities,
              getRadius: getVisualHullRadius,
            });

            const liveNodes = graphRef.current?.graphData?.()?.nodes;
            if (Array.isArray(liveNodes)) {
              for (const node of liveNodes) {
                if (
                  node.isAnchor &&
                  typeof node.x === 'number' &&
                  typeof node.y === 'number'
                ) {
                  if (node.fx == null || node.fy == null) {
                    node.fx = node.x;
                    node.fy = node.y;
                  }
                  positionCacheRef.current.set(String(node.id), {
                    x: node.x,
                    y: node.y,
                  });
                }
              }
            }
          }
        : undefined
    }
    linkColor={
      algorithmMode === 'bigclam'
      ? (link: any) =>
      link.isInvisible
      ? 'rgba(0,0,0,0)'
  : link.isHidden
  ? '#ff4d4f'
  : 'rgba(255,255,255,0.3)'
  : undefined
    }
    linkWidth={
      algorithmMode === 'bigclam'
      ? (link: any) =>
      link.isInvisible ? 0 : link.isHidden ? 2 : 1
      : undefined
    }
    linkDirectionalParticles={
      algorithmMode === 'bigclam'
  ? (link: any) => (link.isHidden ? 3 : 0)
  : undefined
    }
    onNodeClick={(node: any) => {
      if (algorithmMode === 'bigclam') {
        handleBigClamNodeClick(node);
      } else {
        setSelectedNode(node);
        setRightOpen(true);
      }
    }}
    />
    {loading && (
      <div className="graph-status graph-status-loading">
      Сервер обрабатывает данные…
      </div>
    )}
    {!loading && graphStatus === 'error' && (
      <div className="graph-status graph-status-error">
      Сервер не запущен или API недоступен
      </div>
    )}
    {searchError && !loading && (
      <div className="graph-status graph-status-error">
        {searchError}
      </div>
    )}
    {!loading && graphStatus === 'empty' && (
      <div className="graph-status graph-status-empty">
      {algorithmMode === 'bigclam'
        ? 'Результаты BigCLAM ещё не рассчитаны или отсутствуют'
    : 'Данные отсутствуют или ещё обрабатываются сервером'}
    </div>
    )}
    </div>

    <InfoPanel
    selectedNode={selectedNode}
    rightOpen={rightOpen}
    onToggleRight={() => setRightOpen((value) => !value)}
    showCommunityTopics={selectedCommunityId !== null}
    communityTopics={communityTopics}
    communityTopicsLoading={communityTopicsLoading}
    communityTopicsError={communityTopicsError}
    />
    </div>
  );
}
