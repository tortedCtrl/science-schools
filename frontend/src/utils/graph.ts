// src/utils/graph.ts

/**
 * Безопасное чтение ID связи (для обхода d3-объектов,
 * которыми d3-force заменяет source/target после первого рендера).
 */
export const getLinkId = (linkProp: any): string => {
  if (!linkProp) return '';
  if (typeof linkProp === 'object') return linkProp.id || '';
  return String(linkProp);
};