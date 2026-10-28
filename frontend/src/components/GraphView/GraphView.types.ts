// src/components/GraphView/GraphView.types.ts
import type { ColorMode } from '../SearchPanel';

export interface GraphViewProps {
  graphData: { nodes: any[]; links: any[] };
  viewportSize: { w: number; h: number };
  viewportRef: React.RefObject<HTMLDivElement>;

  colorMode: ColorMode;
  highlightCluster: boolean;
  onNodeClick: (node: any) => void;
}