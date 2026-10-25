// src/components/GraphView/GraphView.tsx

import ForceGraph2D from 'react-force-graph-2d';
import type { GraphViewProps } from './GraphView.types';

export function GraphView({
  graphData,
  viewportSize,
  viewportRef,
  colorMode,
  highlightCluster,
  onNodeClick,
}: GraphViewProps) {
  return (
    <main className="graph-viewport" ref={viewportRef}>
      {viewportSize.w > 0 && viewportSize.h > 0 && (
        <ForceGraph2D
          graphData={graphData}
          width={viewportSize.w}
          height={viewportSize.h}
          linkColor={(link: any) => (link.isHidden ? '#ff4d4f' : '#ffffff')}
          linkWidth={(link: any) => (link.isHidden ? 3 : 2)}
          linkCurvature={(link: any) => (link.isHidden ? 0.3 : 0)}
          nodeCanvasObject={(
            node: any,
            ctx: CanvasRenderingContext2D,
            globalScale: number
          ) => {
            const label = node.name || '';
            const fontSize = Math.max(4, 13 / globalScale);
            ctx.font = `bold ${fontSize}px sans-serif`;

            const posX = node.x ?? 0;
            const posY = node.y ?? 0;

            ctx.beginPath();
            ctx.arc(posX, posY, 8, 0, 2 * Math.PI, false);

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
            ctx.lineWidth = 1.5 / globalScale;
            ctx.stroke();

            ctx.fillStyle = '#ffffff';
            ctx.textAlign = 'center';
            ctx.textBaseline = 'top';
            ctx.fillText(label, posX, posY + 12);
          }}
          onNodeClick={onNodeClick}
          linkDirectionalParticles={(link: any) => (link.isHidden ? 5 : 2)}
          linkDirectionalParticleSpeed={0.007}
        />
      )}
    </main>
  );
}