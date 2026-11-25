// src/components/GraphView/GraphView.types.ts
import type { RefObject } from 'react';
import type { ColorMode } from '../SearchPanel';

export interface GraphViewProps {
  graphData: { nodes: any[]; links: any[] };
  viewportSize: { w: number; h: number };
  viewportRef: RefObject<HTMLDivElement>;

  colorMode: ColorMode;
  highlightCluster: boolean;
  onNodeClick: (node: any) => void;
  graphRef?: RefObject<any>;
  nodeCanvasObject?: (
    node: any,
    ctx: CanvasRenderingContext2D,
    globalScale: number
  ) => void;
  onRenderFramePost?: (
    ctx: CanvasRenderingContext2D,
    globalScale: number
  ) => void;
  linkColor?: (link: any) => string;
  linkWidth?: (link: any) => number;
  linkDirectionalParticles?: (link: any) => number;

  minZoom?: number;
  maxZoom?: number;
  onZoom?: (transform: { k: number; x: number; y: number }) => void;
}
