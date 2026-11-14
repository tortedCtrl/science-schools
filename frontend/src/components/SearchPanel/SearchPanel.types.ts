// src/components/SearchPanel/SearchPanel.types.ts

export type ColorMode = 'type' | 'geo';

export interface SearchPanelProps {
  searchName: string;
  setSearchName: (v: string) => void;

  showAll: boolean;
  onShowAllChange: (checked: boolean) => void;

  searchDepth: number;
  setSearchDepth: (v: number) => void;

  vCap: number | '';
  setVCap: (v: number | '') => void;

  loading: boolean;
  onSubmit: (e?: React.FormEvent) => void;

  showHiddenLinks: boolean;
  onToggleHiddenLinks: (checked: boolean) => void;

  colorMode: ColorMode;
  setColorMode: (m: ColorMode) => void;

  highlightCluster: boolean;
  onToggleHighlightCluster: () => void;

  leftOpen: boolean;
  onToggleLeft: () => void;
}