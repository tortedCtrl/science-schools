// src/components/GraphView/GraphView.types.ts
import type { ColorMode } from '../SearchPanel';

export interface GraphViewProps {
  graphData: { nodes: any[]; links: any[] };
  viewportSize: { w: number; h: number };
  viewportRef: React.RefObject<HTMLDivElement>;

  colorMode: ColorMode;
  highlightCluster: boolean;
  onNodeClick: (node: any) => void;
  graphRef?: React.RefObject<any>;
  nodeCanvasObject?: (
    node: any,
    ctx: CanvasRenderingContext2D,
    globalScale: number
  ) => void;
  onRenderFramePost?: (ctx: CanvasRenderingContext2D) => void;
  linkColor?: (link: any) => string;
  linkWidth?: (link: any) => number;
  linkDirectionalParticles?: (link: any) => number;
}