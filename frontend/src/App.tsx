// src/App.tsx
import React, { useState, useRef, useEffect } from 'react';
import { useDatabase } from './hooks/backend';
import { SearchPanel, type ColorMode } from './components/SearchPanel';
import { InfoPanel } from './components/InfoPanel';
import { GraphView } from './components/GraphView';
import { getLinkId } from './utils/graph';
import './App.css';

export default function App() {
  const fetchData = useDatabase();

  const [searchName, setSearchName] = useState('');
  const [showAll, setShowAll] = useState(true);
  const [searchDepth, setSearchDepth] = useState(1);
  const [vCap, setVCap] = useState<number | ''>(100);
  const [loading, setLoading] = useState(false);

  // Состояния аналитических фич
  const [showHiddenLinks, setShowHiddenLinks] = useState(false);
  const [colorMode, setColorMode] = useState<ColorMode>('type');
  const [highlightCluster, setHighlightCluster] = useState(false);

  // Полный граф (последний ответ бэкенда)
  const [fullGraphData, setFullGraphData] = useState<{ nodes: any[]; links: any[] }>({
    nodes: [],
    links: [],
  });

  // Отображаемый граф
  const [graphData, setGraphData] = useState<{ nodes: any[]; links: any[] }>({
    nodes: [],
    links: [],
  });

  const [selectedNode, setSelectedNode] = useState<any>(null);

  // Открытость боковых шторок
  const [leftOpen, setLeftOpen] = useState(true);
  const [rightOpen, setRightOpen] = useState(true);

  // Размер вьюпорта для ForceGraph2D
  const viewportRef = useRef<HTMLDivElement>(null!);
  const [viewportSize, setViewportSize] = useState({ w: 0, h: 0 });

  // Подгонка размера графа под контейнер
  useEffect(() => {
    const el = viewportRef.current;
    if (!el) return;

    const update = () => {
      const r = el.getBoundingClientRect();
      setViewportSize({
        w: Math.max(1, Math.floor(r.width)),
        h: Math.max(1, Math.floor(r.height)),
      });
    };

    const ro = new ResizeObserver(update);
    ro.observe(el);
    update();
    window.addEventListener('resize', update);

    return () => {
      ro.disconnect();
      window.removeEventListener('resize', update);
    };
  }, []);

  // Загрузка графа с бэкенда. Возвращает свежие данные.
  const reloadGraph = async (): Promise<{ nodes: any[]; links: any[] }> => {
    setLoading(true);
    try {
      const data = await fetchData();
      const full = { nodes: data.nodes, links: data.links };
      setFullGraphData(full);
      setGraphData(full);
      setSelectedNode(null);
      return full;
    } catch (e) {
      console.error('API error:', e);
      return { nodes: [], links: [] };
    } finally {
      setLoading(false);
    }
  };

  // Загрузка при старте
  useEffect(() => {
    reloadGraph();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  // Поиск: сначала тянем свежий граф, потом фильтруем
  const handleSearch = async (e?: React.FormEvent) => {
    if (e) e.preventDefault();

    const full = await reloadGraph();

    const baseLinks = showHiddenLinks
      ? full.links
      : full.links.filter((l: any) => !l.isHidden);

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

    const visitedNodeIds = new Set<string>([targetNode.id]);
    let currentLayerIds = new Set<string>([targetNode.id]);

    for (let depth = 0; depth < searchDepth; depth++) {
      const nextLayerIds = new Set<string>();

      baseLinks.forEach((link: any) => {
        const s = getLinkId(link.source);
        const t = getLinkId(link.target);

        if (currentLayerIds.has(s) && !visitedNodeIds.has(t)) {
          nextLayerIds.add(t);
          visitedNodeIds.add(t);
        }
        if (currentLayerIds.has(t) && !visitedNodeIds.has(s)) {
          nextLayerIds.add(s);
          visitedNodeIds.add(s);
        }
      });

      if (nextLayerIds.size === 0) break;
      currentLayerIds = nextLayerIds;
    }

    const filteredLinks = baseLinks.filter((link: any) => {
      const s = getLinkId(link.source);
      const t = getLinkId(link.target);
      return visitedNodeIds.has(s) && visitedNodeIds.has(t);
    });

    let filteredNodes = full.nodes.filter((node: any) =>
      visitedNodeIds.has(node.id)
    );
    if (vCap !== '' && filteredNodes.length > vCap) {
      filteredNodes = filteredNodes.slice(0, vCap);
    }

    setGraphData({ nodes: filteredNodes, links: filteredLinks });
  };

  // Быстрое включение скрытых связей (локально, по текущему fullGraphData)
  const toggleHiddenLinks = (checked: boolean) => {
    setShowHiddenLinks(checked);

    const baseLinks = checked
      ? fullGraphData.links
      : fullGraphData.links.filter((l: any) => !l.isHidden);

    if (showAll || !searchName.trim()) {
      setGraphData({ nodes: fullGraphData.nodes, links: baseLinks });
    } else {
      const currentNodeIds = new Set(graphData.nodes.map((n: any) => n.id));
      const filteredLinks = baseLinks.filter((link: any) => {
        const s = getLinkId(link.source);
        const t = getLinkId(link.target);
        return currentNodeIds.has(s) && currentNodeIds.has(t);
      });
      setGraphData((prev) => ({ ...prev, links: filteredLinks }));
    }
  };

  // Галочка «Показать граф полностью» — тянем свежий граф с бэкенда
  const handleShowAllChange = (checked: boolean) => {
    setShowAll(checked);

    if (checked) {
      setSearchName('');
      reloadGraph();
    }
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
        onSubmit={handleSearch}
        showHiddenLinks={showHiddenLinks}
        onToggleHiddenLinks={toggleHiddenLinks}
        colorMode={colorMode}
        setColorMode={setColorMode}
        highlightCluster={highlightCluster}
        onToggleHighlightCluster={() => setHighlightCluster(!highlightCluster)}
        leftOpen={leftOpen}
        onToggleLeft={() => setLeftOpen((v) => !v)}
      />

      <GraphView
        graphData={graphData}
        viewportSize={viewportSize}
        viewportRef={viewportRef}
        colorMode={colorMode}
        highlightCluster={highlightCluster}
        onNodeClick={(node: any) => {
          setSelectedNode(node);
          setRightOpen(true);
        }}
      />

      <InfoPanel
        selectedNode={selectedNode}
        rightOpen={rightOpen}
        onToggleRight={() => setRightOpen((v) => !v)}
      />
    </div>
  );
}