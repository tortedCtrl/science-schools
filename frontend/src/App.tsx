// src/App.tsx
import { useState, useRef, useEffect } from 'react';
// @ts-ignore
import ForceGraph2D from 'react-force-graph-2d';
// @ts-ignore
import * as d3 from 'd3-force';
import { useBclame } from './hooks/useBclame';
import { useHierarchicalGraph } from './hooks/useHierarchicalGraph';
import { TopicModal } from './components/TopicModal';
import { drawCommunityHulls } from './components/CommunityHull';
import { TOPICS } from './data/topics';
import './App.css';

// 👇 Визуал: корень * 7
const getVisualHullRadius = (memberCount: number) => {
  return 50 + Math.sqrt(memberCount) * 7;
};

// 👇 Коллизия: чуть больше
const getHullRadius = (memberCount: number) => {
  return 60 + Math.sqrt(memberCount) * 7;
};

const getVisualRadius = (memberCount: number) => {
  return Math.min(18, 12 + memberCount * 1.5);
};

const getCollapsedCollisionRadius = (memberCount: number) => {
  return getVisualRadius(memberCount) * 1.5 + 20;
};

export default function App() {
  const fetchMockData = useBclame();
  const graphRef = useRef<any>(null);

  const [selectedTopic, setSelectedTopic] = useState<string | null>(null);
  const [loading, setLoading] = useState(false);

  const [rawGraph, setRawGraph] = useState<{ nodes: any[]; links: any[] }>({ nodes: [], links: [] });
  const [memberships, setMemberships] = useState<Map<string, number>>(new Map());
  const [expandedCommunities, setExpandedCommunities] = useState<Set<number>>(new Set());

  const [selectedNode, setSelectedNode] = useState<any>(null);
  const [leftOpen, setLeftOpen] = useState(true);
  const [rightOpen, setRightOpen] = useState(true);

  const viewportRef = useRef<HTMLDivElement>(null);
  const [viewportSize, setViewportSize] = useState({ w: 0, h: 0 });

  // 1. Подгонка размера
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
  }, [selectedTopic]);

  // 2. Загрузка данных
  useEffect(() => {
    if (!selectedTopic) return;

    let isMounted = true;

    async function load() {
      setLoading(true);
      try {
        const { memberships: membershipsMap, projects: projectsData } = await fetchMockData();
        if (!isMounted) return;
        setMemberships(membershipsMap);

        const nodes: any[] = [];
        const links: any[] = [];

        membershipsMap.forEach((cid, authorId) => {
          nodes.push({
            id: String(authorId),
            name: `Автор ${authorId}`,
            type: 'person',
            location: 'RU',
            info: `Ученый из сообщества #${cid}`,
          });
        });

        projectsData.forEach(proj => {
          const authors = proj.author_ids;
          for (let i = 0; i < authors.length; i++) {
            for (let j = i + 1; j < authors.length; j++) {
              links.push({
                source: authors[i],
                target: authors[j],
                isHidden: false,
              });
            }
          }
        });

        const uniqueLinks = Array.from(
          new Map(links.map(l => {
            const s = l.source < l.target ? l.source : l.target;
            const t = l.source < l.target ? l.target : l.source;
            return [`${s}-${t}`, l];
          })).values()
        );

        setRawGraph({ nodes, links: uniqueLinks });
      } catch (e: any) {
        console.error('Ошибка в load():', e);
      } finally {
        if (isMounted) setLoading(false);
      }
    }

    load();
    return () => { isMounted = false; };
  }, [selectedTopic]);

  const displayGraph = useHierarchicalGraph(rawGraph, memberships, expandedCommunities);

  // 3.1. Статические силы
  useEffect(() => {
    const graph = graphRef.current;
    if (!graph) return;

    graph.d3Force('charge').strength(-250);

    graph.d3Force('link')
      .distance((link: any) => {
        if (link.isInvisible) {
          const anchorId = typeof link.source === 'object' ? link.source.id : link.source;
          const anchor = displayGraph.nodes.find((n: any) => n.id === anchorId);
          const memberCount = anchor?.memberCount || 1;
          const hullRadius = getHullRadius(memberCount);
          return hullRadius * 0.7;
        }
        if (link.distance) return link.distance;
        return 100;
      })
      .strength((link: any) => {
        if (link.isInvisible) return 0.1; // ослабили притяжение к центру
        return 0.4;
      });

    graph.d3Force(
      'collision',
      d3
        .forceCollide((node: any) => {
          if (node.isCommunity && !node.isAnchor) {
            return getCollapsedCollisionRadius(node.memberCount || 0);
          }
          if (node.isAnchor) {
            return getHullRadius(node.memberCount || 0) + 30;
          }
          return 0; // люди не отталкиваются глобально
        })
        .strength(1)
        .iterations(3)
    );
  }, []);

  // 3.2. Динамические силы
  useEffect(() => {
    const graph = graphRef.current;
    if (!graph) return;

    graph.d3Force('hullBoundary', null);
    graph.d3Force('hullRepulsion', null);
    graph.d3Force('communityRepulsion', null);
    graph.d3Force('peopleRepulsion', null);
    graph.d3Force('keepPeopleInside', null); // 👈 новая сила

    const nodes = displayGraph.nodes;
    const anchors = nodes.filter((n: any) => n.isAnchor);
    const collapsedCommunities = nodes.filter((n: any) => n.isCommunity && !n.isAnchor);

    // 3.2.1. hullBoundary (не пускаем чужих внутрь hull)
    if (anchors.length > 0) {
      graph.d3Force('hullBoundary', (alpha: number) => {
        nodes.forEach((node: any) => {
          if (node.isAnchor) return;
          const nodeCid = memberships.get(node.id);
          if (nodeCid !== undefined && expandedCommunities.has(nodeCid)) return; // свои — пропускаем

          anchors.forEach((anchor: any) => {
            const dx = (node.x ?? 0) - (anchor.x ?? 0);
            const dy = (node.y ?? 0) - (anchor.y ?? 0);
            const dist = Math.sqrt(dx * dx + dy * dy) || 1;
            const hullRadius = getHullRadius(anchor.memberCount || 0);
            const minDist = hullRadius + 30;

            if (dist < minDist) {
              const force = (minDist - dist) * alpha * 0.8;
              node.vx = (node.vx ?? 0) + (dx / dist) * force;
              node.vy = (node.vy ?? 0) + (dy / dist) * force;
            }
          });
        });
      });
    }

    // 3.2.2. hullRepulsion (отталкиваем якоря)
    if (anchors.length > 1) {
      graph.d3Force('hullRepulsion', (alpha: number) => {
        for (let i = 0; i < anchors.length; i++) {
          for (let j = i + 1; j < anchors.length; j++) {
            const a = anchors[i];
            const b = anchors[j];
            const dx = (a.x ?? 0) - (b.x ?? 0);
            const dy = (a.y ?? 0) - (b.y ?? 0);
            const dist = Math.sqrt(dx * dx + dy * dy) || 1;

            const radiusA = getHullRadius(a.memberCount || 0);
            const radiusB = getHullRadius(b.memberCount || 0);
            const minDist = radiusA + radiusB + 20;

            if (dist < minDist) {
              const overlap = (minDist - dist) / 2;
              const force = overlap * alpha * 0.5;
              const nx = dx / dist;
              const ny = dy / dist;

              a.vx = (a.vx ?? 0) + nx * force;
              a.vy = (a.vy ?? 0) + ny * force;
              b.vx = (b.vx ?? 0) - nx * force;
              b.vy = (b.vy ?? 0) - ny * force;
            }
          }
        }
      });
    }

    // 3.2.3. communityRepulsion
    const allCommunities = [...collapsedCommunities, ...anchors];
    if (allCommunities.length > 1) {
      graph.d3Force('communityRepulsion', (alpha: number) => {
        for (let i = 0; i < allCommunities.length; i++) {
          for (let j = i + 1; j < allCommunities.length; j++) {
            const a = allCommunities[i];
            const b = allCommunities[j];
            const dx = (a.x ?? 0) - (b.x ?? 0);
            const dy = (a.y ?? 0) - (b.y ?? 0);
            const dist = Math.sqrt(dx * dx + dy * dy) || 1;

            const radiusA = a.isAnchor
              ? getHullRadius(a.memberCount || 0) + 30
              : getCollapsedCollisionRadius(a.memberCount || 0);
            const radiusB = b.isAnchor
              ? getHullRadius(b.memberCount || 0) + 30
              : getCollapsedCollisionRadius(b.memberCount || 0);
            const minDist = radiusA + radiusB + 20;

            if (dist < minDist) {
              const overlap = (minDist - dist) / 2;
              const force = overlap * alpha * 0.5;
              const nx = dx / dist;
              const ny = dy / dist;

              a.vx = (a.vx ?? 0) + nx * force;
              a.vy = (a.vy ?? 0) + ny * force;
              b.vx = (b.vx ?? 0) - nx * force;
              b.vy = (b.vy ?? 0) - ny * force;
            }
          }
        }
      });
    }

    // 3.2.4. peopleRepulsion (отталкиваем людей внутри одного сообщества)
    if (anchors.length > 0) {
      graph.d3Force('peopleRepulsion', (alpha: number) => {
        const peopleByCommunity = new Map<number, any[]>();
        nodes.forEach((node: any) => {
          if (node.isCommunity) return;
          const cid = memberships.get(node.id);
          if (cid === undefined) return;
          if (!expandedCommunities.has(cid)) return;
          if (!peopleByCommunity.has(cid)) peopleByCommunity.set(cid, []);
          peopleByCommunity.get(cid)!.push(node);
        });

        peopleByCommunity.forEach((people) => {
          const minDist = 30;
          for (let i = 0; i < people.length; i++) {
            for (let j = i + 1; j < people.length; j++) {
              const a = people[i];
              const b = people[j];
              const dx = (a.x ?? 0) - (b.x ?? 0);
              const dy = (a.y ?? 0) - (b.y ?? 0);
              const dist = Math.sqrt(dx * dx + dy * dy) || 1;

              if (dist < minDist) {
                const overlap = (minDist - dist) / 2;
                const force = overlap * alpha * 0.5;
                const nx = dx / dist;
                const ny = dy / dist;

                a.vx = (a.vx ?? 0) + nx * force;
                a.vy = (a.vy ?? 0) + ny * force;
                b.vx = (b.vx ?? 0) - nx * force;
                b.vy = (b.vy ?? 0) - ny * force;
              }
            }
          }
        });
      });
    }

    // 👇 3.2.5. keepPeopleInside: не даём людям выйти за пределы hull
    if (anchors.length > 0) {
      graph.d3Force('keepPeopleInside', (alpha: number) => {
        anchors.forEach((anchor: any) => {
          const cid = parseInt(anchor.id.split('-')[1], 10);
          const people = nodes.filter((n: any) => {
            const nodeCid = memberships.get(n.id);
            return nodeCid === cid;
          });
          const hullRadius = getVisualHullRadius(people.length); // визуальный радиус (жёсткая граница)

          people.forEach((person: any) => {
            const dx = (person.x ?? 0) - (anchor.x ?? 0);
            const dy = (person.y ?? 0) - (anchor.y ?? 0);
            const dist = Math.sqrt(dx * dx + dy * dy) || 1;

            // Если человек вышел за пределы визуального hull, возвращаем его внутрь
            if (dist > hullRadius - 10) { // -10, чтобы был запас от края
              const force = (dist - (hullRadius - 10)) * alpha * 2.0; // сильная возвращающая сила
              person.vx = (person.vx ?? 0) - (dx / dist) * force;
              person.vy = (person.vy ?? 0) - (dy / dist) * force;
            }
          });
        });
      });
    }
  }, [expandedCommunities, memberships, displayGraph.nodes]);

  const handleNodeClick = (node: any) => {
    if (node.isCommunity) {
      const cid = parseInt(node.id.split('-')[1], 10);
      setExpandedCommunities(prev => {
        const next = new Set(prev);
        if (next.has(cid)) next.delete(cid);
        else next.add(cid);
        return next;
      });
    } else {
      setSelectedNode(node);
      setRightOpen(true);
    }
  };

  const nodeCanvasObject = (node: any, ctx: CanvasRenderingContext2D, globalScale: number) => {
    if (node.isAnchor) {
      const posX = node.x ?? 0;
      const posY = node.y ?? 0;
      const radius = 8;
      ctx.beginPath();
      ctx.arc(posX, posY, radius, 0, 2 * Math.PI);
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

    const label = node.name || '';
    const fontSize = Math.max(4, 12 / globalScale);
    ctx.font = `${fontSize}px sans-serif`;
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';

    const posX = node.x ?? 0;
    const posY = node.y ?? 0;

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
      ctx.fillText(label, posX, posY - 2);

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
    ctx.fillText(label, posX, posY + 12 / globalScale);
  };

  const nodePointerAreaPaint = (node: any, color: string, ctx: CanvasRenderingContext2D, globalScale: number) => {
    ctx.fillStyle = color;
    const posX = node.x ?? 0;
    const posY = node.y ?? 0;

    if (node.isAnchor) {
      ctx.beginPath();
      ctx.arc(posX, posY, 8, 0, 2 * Math.PI);
      ctx.fill();
      return;
    }

    if (node.isCommunity) {
      const radius = getVisualRadius(node.memberCount || 0);
      ctx.beginPath();
      ctx.arc(posX, posY, radius, 0, 2 * Math.PI);
      ctx.fill();
      return;
    }

    ctx.beginPath();
    ctx.arc(posX, posY, 8, 0, 2 * Math.PI);
    ctx.fill();
  };

  if (!selectedTopic) {
    return <TopicModal onSelect={setSelectedTopic} />;
  }

  const currentTopic = TOPICS.find(t => t.id === selectedTopic);

  return (
    <div className="app-container">
      <aside className={`sidebar search-panel ${leftOpen ? 'open' : 'closed'}`}>
        <button className="sidebar-handle" onClick={() => setLeftOpen(v => !v)}>
          <svg viewBox="0 0 24 24" width="20" height="20" fill="none" stroke="currentColor" strokeWidth="2">
            <circle cx="11" cy="11" r="7" />
            <line x1="21" y1="21" x2="16.65" y2="16.65" />
          </svg>
        </button>

        <div className="sidebar-content">
          <h2>Тема: {currentTopic?.name}</h2>
          <button
            className="btn-toggle"
            onClick={() => { setSelectedTopic(null); setExpandedCommunities(new Set()); }}
            style={{ marginBottom: '20px', width: '100%' }}
          >
            Сменить тему
          </button>

          <div className="analysis-actions-block">
            <h3>Сообщества</h3>
            <p style={{ fontSize: '12px', color: '#888' }}>
              Кликните на сообщество, чтобы развернуть. Клик по центру «×» — свернуть обратно.
            </p>
          </div>
        </div>
      </aside>

      <main className="graph-viewport" ref={viewportRef}>
        {loading && (
          <div className="loading-overlay">
            <span>Загрузка сообществ...</span>
          </div>
        )}

        {viewportSize.w > 0 && viewportSize.h > 0 && (
          <ForceGraph2D
            ref={graphRef}
            graphData={displayGraph}
            width={viewportSize.w}
            height={viewportSize.h}
            nodeCanvasObject={nodeCanvasObject}
            nodePointerAreaPaint={nodePointerAreaPaint}
            linkColor={(link: any) => {
              if (link.isInvisible) return 'rgba(0,0,0,0)';
              return link.isHidden ? '#ff4d4f' : 'rgba(255,255,255,0.3)';
            }}
            linkWidth={(link: any) => {
              if (link.isInvisible) return 0;
              return link.isHidden ? 2 : 1;
            }}
            linkDirectionalParticles={(link: any) => (link.isHidden ? 3 : 0)}
            onNodeClick={handleNodeClick}
            cooldownTicks={200}
            warmupTicks={100}
            onRenderFramePost={(ctx) => {
              drawCommunityHulls({
                ctx,
                nodes: displayGraph.nodes,
                expandedCommunities,
                getRadius: getVisualHullRadius,
              });
            }}
          />
        )}
      </main>

      <aside className={`sidebar info-panel ${rightOpen ? 'open' : 'closed'}`}>
        <button className="sidebar-handle" onClick={() => setRightOpen(v => !v)}>
          <svg viewBox="0 0 24 24" width="20" height="20" fill="none" stroke="currentColor" strokeWidth="2">
            <circle cx="12" cy="12" r="9" />
            <line x1="12" y1="11" x2="12" y2="16" />
            <circle cx="12" cy="8" r="0.6" fill="currentColor" />
          </svg>
        </button>

        <div className="sidebar-content">
          <h2>Информация</h2>
          {selectedNode ? (
            <div className="node-details">
              <h3>{selectedNode.name}</h3>
              <p><strong>ID:</strong> {selectedNode.id}</p>
              {selectedNode.type !== 'community' && (
                <>
                  <p><strong>Тип:</strong> Ученый</p>
                  <hr />
                  <p>{selectedNode.info}</p>
                </>
              )}
            </div>
          ) : (
            <p className="placeholder-text">
              Кликните на узел, чтобы увидеть информацию.
            </p>
          )}
        </div>
      </aside>
    </div>
  );
}