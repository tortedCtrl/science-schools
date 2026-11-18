import React, { useEffect, useLayoutEffect, useRef, useState } from 'react';
import * as d3 from 'd3-force';
import {
  useCommunityTopics,
  useDatabase,
  type CommunityTopic,
} from './hooks/backend';
import { useBclame } from './hooks/useBclame';
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

export default function App() {
  const fetchLeidenData = useDatabase();
  const fetchBigClamData = useBclame();

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

  const [rawGraph, setRawGraph] = useState<{
    nodes: any[];
    links: any[];
  }>({ nodes: [], links: [] });
  const [memberships, setMemberships] =
    useState<Map<string, number>>(new Map());
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

      try {
        if (algorithmMode === 'leiden') {
          await reloadLeidenGraph();
          return;
        }

        const { memberships: membershipMap, projects } =
          await fetchBigClamData();
        if (!isMounted) return;

        const validMemberships =
          membershipMap instanceof Map && membershipMap.size > 0
            ? membershipMap
            : new Map<string, number>();
        const validProjects = Array.isArray(projects) ? projects : [];

        setMemberships(validMemberships);
        setExpandedCommunities(new Set());

        const nodes: any[] = [];
        validMemberships.forEach((communityId, authorId) => {
          nodes.push({
            id: String(authorId),
            name: `Автор ${authorId}`,
            community_id: communityId,
            type: 'person',
            location: 'RU',
            info: `Ученый из сообщества #${communityId}`,
          });
        });

        const links: any[] = [];
        validProjects.forEach((project) => {
          if (!Array.isArray(project?.author_ids)) return;
          for (let i = 0; i < project.author_ids.length; i++) {
            for (let j = i + 1; j < project.author_ids.length; j++) {
              links.push({
                source: project.author_ids[i],
                target: project.author_ids[j],
                isHidden: false,
              });
            }
          }
        });

        const uniqueLinks = Array.from(
          new Map(
            links.map((link) => {
              const source =
                link.source < link.target ? link.source : link.target;
              const target =
                link.source < link.target ? link.target : link.source;
              return [`${source}-${target}`, link];
            })
          ).values()
        );

        setRawGraph({ nodes, links: uniqueLinks });
        setGraphStatus(nodes.length === 0 ? 'empty' : 'ready');
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
    expandedCommunities
  );
  const currentGraphData =
    algorithmMode === 'bigclam' ? displayBigClamGraph : graphData;

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
    if (!highlightCluster || selectedCommunityId == null) {
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
  }, [
    algorithmMode,
    highlightCluster,
    selectedCommunityId,
    fetchCommunityTopics,
  ]);

  const handleLeidenSearch = async (event?: React.FormEvent) => {
    if (event) event.preventDefault();
    if (algorithmMode === 'bigclam') return;

    const full = await reloadLeidenGraph();
    const baseLinks = showHiddenLinks
      ? full.links
      : full.links.filter((link: any) => !link.isHidden);

    if (showAll || !searchName.trim()) {
      setGraphData({ nodes: full.nodes, links: baseLinks });
      return;
    }

    const targetNode = full.nodes.find(
      (node: any) =>
        node.name.toLowerCase().includes(searchName.toLowerCase()) ||
        node.id === searchName
    );
    if (!targetNode) {
      setGraphData({ nodes: [], links: [] });
      setSelectedNode(null);
      return;
    }

    setSelectedNode(targetNode);
    const visited = new Set<string>([targetNode.id]);
    let current = new Set<string>([targetNode.id]);

    for (let depth = 0; depth < searchDepth; depth++) {
      const next = new Set<string>();
      baseLinks.forEach((link: any) => {
        const source = getLinkId(link.source);
        const target = getLinkId(link.target);
        if (current.has(source) && !visited.has(target)) {
          visited.add(target);
          next.add(target);
        }
        if (current.has(target) && !visited.has(source)) {
          visited.add(source);
          next.add(source);
        }
      });
      if (!next.size) break;
      current = next;
    }

    const filteredLinks = baseLinks.filter((link: any) => {
      const source = getLinkId(link.source);
      const target = getLinkId(link.target);
      return visited.has(source) && visited.has(target);
    });
    let filteredNodes = full.nodes.filter((node: any) =>
      visited.has(node.id)
    );
    if (vCap !== '' && filteredNodes.length > vCap) {
      filteredNodes = filteredNodes.slice(0, vCap);
    }
    setGraphData({ nodes: filteredNodes, links: filteredLinks });
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
    if (algorithmMode === 'leiden' && checked) {
      setSearchName('');
      reloadLeidenGraph();
    }
  };

  useEffect(() => {
    if (algorithmMode !== 'bigclam') return;
    const graph = graphRef.current;
    if (!graph) return;

    graph.d3Force('charge')?.strength(-250);
    graph.d3Force('link')
      ?.distance((link: any) => {
        if (link.isInvisible) {
          const sourceId =
            typeof link.source === 'object' ? link.source.id : link.source;
        const communityId = Number(sourceId?.replace('community-', ''));
        if (Number.isFinite(communityId)) {
          const bounds = getCommunityHullBounds(
            displayBigClamGraph.nodes,
            communityId,
            getHullRadius
          );
          return Math.max(24, bounds.radius * 0.7);
        }
        return 100;
      }
      return 100;
      })
      .strength((link: any) => (link.isInvisible ? 0.1 : 0.4));

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
          return 0;
        })
        .strength(1)
        .iterations(3)
    );
  }, [algorithmMode, expandedCommunities, displayBigClamGraph.nodes]);

  const handleBigClamNodeClick = (node: any) => {
    if (node.isCommunity) {
      const communityId = parseInt(node.id.split('-')[1], 10);
      setSelectedNode({
        ...node,
        community_id: communityId,
        type: 'community',
      });
      setRightOpen(true);
      setExpandedCommunities((previous) => {
        const next = new Set(previous);
        if (next.has(communityId)) next.delete(communityId);
        else next.add(communityId);
        return next;
      });
      return;
    }

    setSelectedNode({
      ...node,
      community_id: memberships.get(node.id) ?? node.community_id,
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
      ctx.fillStyle = '#fff';
      ctx.font = `bold ${fontSize * 0.9}px sans-serif`;
      ctx.fillText(node.name || '', posX, posY - 2);
      if (node.memberCount) {
        ctx.font = `${fontSize * 0.7}px sans-serif`;
        ctx.fillText(`(${node.memberCount})`, posX, posY + 8 / globalScale);
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
    ctx.fillStyle = '#fff';
    ctx.font = `${fontSize}px sans-serif`;
    ctx.fillText(node.name || '', posX, posY + 12 / globalScale);
  };

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
        onSubmit={handleLeidenSearch}
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
          nodeCanvasObject={
            algorithmMode === 'bigclam'
              ? bigClamNodeCanvasObject
              : undefined
          }
          onRenderFramePost={
            algorithmMode === 'bigclam'
              ? (ctx) =>
                  drawCommunityHulls({
                    ctx,
                    nodes: displayBigClamGraph.nodes,
                    expandedCommunities,
                    getRadius: getVisualHullRadius,
                  })
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
          onNodeClick={(node) => {
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
        {!loading && graphStatus === 'empty' && (
          <div className="graph-status graph-status-empty">
            Данные отсутствуют или ещё обрабатываются сервером
          </div>
        )}
      </div>

      <InfoPanel
        selectedNode={selectedNode}
        rightOpen={rightOpen}
        onToggleRight={() => setRightOpen((value) => !value)}
        showCommunityTopics={highlightCluster}
        communityTopics={communityTopics}
        communityTopicsLoading={communityTopicsLoading}
        communityTopicsError={communityTopicsError}
      />
    </div>
  );
}
