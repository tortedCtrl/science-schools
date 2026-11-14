// src/components/GraphView/GraphView.tsx

import { useEffect } from 'react';
import ForceGraph2D from 'react-force-graph-2d';
import type { GraphViewProps } from './GraphView.types';

// --- Параметры отображения узлов и подписей ---
// Порог масштаба, ниже которого подписи начинают затухать, и ниже которого исчезают совсем.
const LABEL_FADE_START = 1;
const LABEL_FADE_END = 1.5;
// Базовый радиус узла в экранных пикселях (при globalScale = 1).
const BASE_RADIUS_PX = 8;
// Насколько сильно узел сжимается при отдалении:
//   0 = размер на экране фиксированный, 1 = ведёт себя как мировой радиус.
const RADIUS_SHRINK = -0.25;
// Максимальный уровень приближения (1 = базовый масштаб).
const MAX_ZOOM = 4;

export function GraphView({
  graphData,
  viewportSize,
  viewportRef,
  colorMode,
  highlightCluster,
  onNodeClick,
}: GraphViewProps) {
  // Блокируем зум страницы, когда курсор над графом.
  // Без этого при достижении maxZoom браузер начинает масштабировать весь документ.
  useEffect(() => {
    const el = viewportRef.current;
    if (!el) return;

    const handler = (e: WheelEvent) => {
      e.preventDefault();
    };

    // passive: false — иначе preventDefault игнорируется браузером.
    el.addEventListener('wheel', handler, { passive: false });

    return () => {
      el.removeEventListener('wheel', handler);
    };
  }, [viewportRef]);

  return (
    <main className="graph-viewport" ref={viewportRef}>
      {viewportSize.w > 0 && viewportSize.h > 0 && (
        <ForceGraph2D
          graphData={graphData}
          width={viewportSize.w}
          height={viewportSize.h}
          maxZoom={MAX_ZOOM}
          linkColor={(link: any) => (link.isHidden ? '#ff4d4f' : '#ffffff')}
          linkWidth={(link: any) => (link.isHidden ? 2.5 : 1.5)}
          linkCurvature={(link: any) => (link.isHidden ? 0.3 : 0)}
          nodeCanvasObject={(
            node: any,
            ctx: CanvasRenderingContext2D,
            globalScale: number
          ) => {
            const label = node.name || '';
            const posX = node.x ?? 0;
            const posY = node.y ?? 0;

            // Радиус в мировых координатах: при отдалении уменьшается медленнее,
            // чем сам масштаб, поэтому на экране узел «сжимается» плавнее.
            const radius =
              BASE_RADIUS_PX / Math.pow(Math.max(globalScale, 0.05), RADIUS_SHRINK);

            ctx.beginPath();
            ctx.arc(posX, posY, radius, 0, 2 * Math.PI, false);

            if (highlightCluster) {
              const communityId = node.community_id;
              ctx.fillStyle =
                communityId == null
                  ? '#555555'
                  : `hsl(${(communityId * 137.508) % 360} 70% 55%)`;
            } else if (colorMode === 'geo') {
              ctx.fillStyle = node.location === 'INT' ? '#ff4d4f' : '#1890ff';
            } else {
              ctx.fillStyle = node.type === 'project' ? '#52c41a' : '#1890ff';
            }

            ctx.fill();
            ctx.strokeStyle = '#ffffff';
            ctx.lineWidth = Math.max(0.5, 1.5 / globalScale);
            ctx.stroke();

            // Плавное затухание подписей при отдалении.
            const labelAlpha = Math.max(
              0,
              Math.min(
                1,
                (globalScale - LABEL_FADE_START) /
                  (LABEL_FADE_END - LABEL_FADE_START)
              )
            );
            if (label && labelAlpha > 0.01) {
              ctx.save();
              ctx.globalAlpha = labelAlpha;
              const fontSize = Math.max(4, 13 / globalScale);
              ctx.font = `bold ${fontSize}px sans-serif`;
              ctx.fillStyle = '#ffffff';
              ctx.textAlign = 'center';
              ctx.textBaseline = 'top';
              ctx.fillText(label, posX, posY + radius + 4);
              ctx.restore();
            }
          }}
          onNodeClick={onNodeClick}
          linkDirectionalParticles={(link: any) => (link.isHidden ? 5 : 2)}
          linkDirectionalParticleSpeed={0.007}
        />
      )}
    </main>
  );
}