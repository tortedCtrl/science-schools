// src/App.tsx
import React, { useState } from 'react';
// @ts-ignore
import ForceGraph2D from 'react-force-graph-2d';
import { mockGraphData } from './mockData';
import './App.css';

export default function App() {
  const [searchName, setSearchName] = useState('');
  const [showAll, setShowAll] = useState(true);
  const [searchDepth, setSearchDepth] = useState(1);
  const [vCap, setVCap] = useState<number | ''>(100);

  // Состояния аналитических фич
  const [showHiddenLinks, setShowHiddenLinks] = useState(false);
  const [colorMode, setColorMode] = useState<'type' | 'geo'>('type');
  const [highlightCluster, setHighlightCluster] = useState(false);

  // Состояние графа
  const [graphData, setGraphData] = useState<any>({
    nodes: mockGraphData.nodes,
    links: mockGraphData.links.filter(l => !l.isHidden)
  });
  
  const [selectedNode, setSelectedNode] = useState<any>(null);

  // Безопасное чтение ID связей (для обхода d3-объектов)
  const getLinkId = (linkProp: any): string => {
    if (!linkProp) return '';
    if (typeof linkProp === 'object') return linkProp.id || '';
    return String(linkProp);
  };

  // Поиск и фильтрация графа
  const handleSearch = (e?: React.FormEvent) => {
    if (e) e.preventDefault();
    
    let baseLinks = showHiddenLinks 
      ? mockGraphData.links 
      : mockGraphData.links.filter(l => !l.isHidden);

    if (showAll || !searchName.trim()) {
      setGraphData({ nodes: mockGraphData.nodes, links: baseLinks });
      return;
    }

    const targetNode = mockGraphData.nodes.find(node => 
      node.name.toLowerCase().includes(searchName.toLowerCase()) || node.id === searchName
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
      
      baseLinks.forEach(link => {
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

    const filteredLinks = baseLinks.filter(link => {
      const s = getLinkId(link.source);
      const t = getLinkId(link.target);
      return visitedNodeIds.has(s) && visitedNodeIds.has(t);
    });

    let filteredNodes = mockGraphData.nodes.filter(node => visitedNodeIds.has(node.id));
    if (vCap !== '' && filteredNodes.length > vCap) {
      filteredNodes = filteredNodes.slice(0, vCap);
    }

    setGraphData({ nodes: filteredNodes, links: filteredLinks });
  };

  // Быстрое включение скрытых связей
  const toggleHiddenLinks = (checked: boolean) => {
    setShowHiddenLinks(checked);
    const baseLinks = checked ? mockGraphData.links : mockGraphData.links.filter(l => !l.isHidden);
    
    if (showAll || !searchName.trim()) {
      setGraphData({ nodes: mockGraphData.nodes, links: baseLinks });
    } else {
      const currentNodeIds = new Set(graphData.nodes.map((n: any) => n.id));
      const filteredLinks = baseLinks.filter(link => {
        const s = getLinkId(link.source);
        const t = getLinkId(link.target);
        return currentNodeIds.has(s) && currentNodeIds.has(t);
      });
      setGraphData((prev: any) => ({ ...prev, links: filteredLinks }));
    }
  };

  return (
    <div className="app-container">
      {/* Левая панель параметров поиска */}
      <aside className="sidebar search-panel">
        <h2>Поиск и Аналитика</h2>
        <form onSubmit={handleSearch}>
          <div className="form-group">
            <label>Имя или ID объекта:</label>
            <input 
              type="text" 
              value={searchName} 
              disabled={showAll}
              onChange={(e) => setSearchName(e.target.value)} 
              placeholder={showAll ? "Снимите галочку для поиска..." : "Например, Кузнецова..."}
            />
          </div>

          <div className="form-group checkbox">
            <label>
              <input 
                type="checkbox" 
                checked={showAll} 
                onChange={(e) => {
                  const checked = e.target.checked;
                  setShowAll(checked);
                  if (checked) {
                    setSearchName('');
                    setGraphData({ 
                      nodes: mockGraphData.nodes, 
                      links: showHiddenLinks ? mockGraphData.links : mockGraphData.links.filter(l => !l.isHidden) 
                    });
                  }
                }} 
              />
              Показать граф полностью
            </label>
          </div>

          <div className="form-group">
            <label>Глубина поиска (слои): {searchDepth}</label>
            <input 
              type="range" min="1" max="3" 
              value={searchDepth} 
              disabled={showAll}
              onChange={(e) => setSearchDepth(Number(e.target.value))}
            />
          </div>

          <div className="form-group">
            <label>Макс. вершин (V_cap):</label>
            <input 
              type="number" 
              value={vCap} 
              onChange={(e) => setVCap(e.target.value === '' ? '' : Number(e.target.value))}
              placeholder="Без ограничений"
            />
          </div>

          <button type="submit" className="btn-submit" disabled={showAll && searchName === ''}>
            Рассчитать граф
          </button>
        </form>

        {/* Инструменты восстановления связей */}
        <div className="analysis-actions-block">
          <h3>Модули восстановления РФ-связей</h3>
          
          <div className="form-group checkbox">
            <label className="toggle-label">
              <input 
                type="checkbox" 
                checked={showHiddenLinks} 
                onChange={(e) => toggleHiddenLinks(e.target.checked)} 
              />
              <span className="badge-alert">NEW</span> Подсветить скрытые связи за рубежом
            </label>
          </div>

          <div className="form-group">
            <label>Режим раскраски графа:</label>
            <div className="radio-group">
              <button 
                type="button" 
                className={`btn-toggle ${colorMode === 'type' ? 'active' : ''}`}
                onClick={() => setColorMode('type')}
              >
                Тип (Человек/Проект)
              </button>
              <button 
                type="button" 
                className={`btn-toggle ${colorMode === 'geo' ? 'active' : ''}`}
                onClick={() => setColorMode('geo')}
              >
                Гео-след (РФ / Зарубеж)
              </button>
            </div>
          </div>

          <button 
            type="button" 
            className={`btn-analytics ${highlightCluster ? 'active' : ''}`}
            onClick={() => setHighlightCluster(!highlightCluster)}
          >
            {highlightCluster ? '⚡ Сбросить кластеры' : '🔍 Найти научные школы (СВК)'}
          </button>
        </div>

        <div className="analytics-notice">
          <small>Система деанонимизации скрытых связей работает в тестовом режиме моков.</small>
        </div>
      </aside>

      {/* Центр: Визуализация графа */}
      <main className="graph-viewport">
        <ForceGraph2D
          graphData={graphData}
          linkColor={(link: any) => link.isHidden ? '#ff4d4f' : '#ffffff'} 
          linkWidth={(link: any) => link.isHidden ? 3 : 2}
          linkCurvature={(link: any) => link.isHidden ? 0.3 : 0}
          
          nodeCanvasObject={(node: any, ctx: CanvasRenderingContext2D, globalScale: number) => {
            const label = node.name || '';
            const fontSize = Math.max(4, 13 / globalScale);
            ctx.font = `bold ${fontSize}px sans-serif`;
            
            const posX = node.x ?? 0;
            const posY = node.y ?? 0;

            ctx.beginPath();
            ctx.arc(posX, posY, 8, 0, 2 * Math.PI, false);
            
            if (highlightCluster) {
              ctx.fillStyle = ['1', '2', '3', '12'].includes(node.id) ? '#faad14' : '#555555';
            } else if (colorMode === 'geo') {
              ctx.fillStyle = node.location === 'INT' ? '#ff4d4f' : '#1890ff';
            } else {
              ctx.fillStyle = node.type === 'project' ? '#52c41a' : '#1890ff';
            }
            
            ctx.fill();
            ctx.strokeStyle = '#ffffff';
            ctx.lineWidth = 1.5 / globalScale;
            ctx.stroke();
            
            ctx.fillStyle = '#ffffff';
            ctx.textAlign = 'center';
            ctx.textBaseline = 'top';
            ctx.fillText(label, posX, posY + 12);
          }}
          onNodeClick={(node: any) => setSelectedNode(node)}
          linkDirectionalParticles={(link: any) => link.isHidden ? 5 : 2}
          linkDirectionalParticleSpeed={0.007}
        />
      </main>

            {/* Правая панель информации (Show_info) */}
      <aside className="sidebar info-panel">
        <h2>Информация (Show_info)</h2>
        {selectedNode ? (
          <div className="node-details">
            <h3>{selectedNode.name}</h3>
            <p><strong>ID:</strong> {selectedNode.id}</p>
            <p><strong>Регион публикаций:</strong> {selectedNode.location === 'RU' ? '🇷🇺 Россия' : '🌐 Международный / Скрытый'}</p>
            <p><strong>Категория:</strong> {selectedNode.type === 'project' ? 'Организация / Проект' : 'Ученый / Сотрудник'}</p>
            <hr style={{ borderColor: '#555', margin: '15px 0' }} />
            <p style={{ lineHeight: '1.5', color: '#f0f0f0' }}>{selectedNode.info}</p>
          </div>
        ) : (
          <p className="placeholder-text">
            Кликните на объект, чтобы изучить восстановленные бэкендом атрибуты и скрытые аффилиации.
          </p>
        )}
      </aside>
    </div>
  );
}
