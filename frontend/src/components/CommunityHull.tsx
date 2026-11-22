// src/components/CommunityHull.tsx
interface CommunityHullProps {
  ctx: CanvasRenderingContext2D;
  nodes: any[];
  expandedCommunities: Set<number>;
  getRadius: (memberCount: number) => number;
}

export function getCommunityHullBounds(
  nodes: any[],
  communityId: number,
  fallbackRadius: (memberCount: number) => number
) {
  const members = nodes.filter(
    (node) =>
      node.parentCommunityId === communityId &&
      Number.isFinite(node.x) &&
      Number.isFinite(node.y)
  );
  const anchor = nodes.find(
    (node) => node.id === `community-${communityId}`
  );

  if (members.length === 0) {
    return {
      cx: anchor?.x ?? 0,
      cy: anchor?.y ?? 0,
      radius: fallbackRadius(anchor?.memberCount ?? 0),
    };
  }

  const cx =
    members.reduce((sum, member) => sum + (member.x as number), 0) /
    members.length;
  const cy =
    members.reduce((sum, member) => sum + (member.y as number), 0) /
    members.length;
  const farthestMemberDistance = members.reduce((maximum, member) => {
    const dx = (member.x as number) - cx;
    const dy = (member.y as number) - cy;
    return Math.max(maximum, Math.sqrt(dx * dx + dy * dy));
  }, 0);

  return {
    cx,
    cy,
    radius: Math.max(24, farthestMemberDistance + 12),
  };
}

export function drawCommunityHulls({ ctx, nodes, expandedCommunities, getRadius }: CommunityHullProps) {
  if (!ctx || !nodes.length) return;

  expandedCommunities.forEach(cid => {
    const anchor = nodes.find(n => n.id === `community-${cid}`);
    if (!anchor) return;

    const members = nodes.filter(n => n.parentCommunityId === cid);
    const memberCount = members.length;
    const bounds = getCommunityHullBounds(nodes, cid, getRadius);
    const cx = bounds.cx;
    const cy = bounds.cy;
    const radius = bounds.radius;

    if (memberCount > 0) {
      anchor.x = cx;
      anchor.y = cy;
    }

    ctx.save();
    ctx.beginPath();
    ctx.arc(cx, cy, radius, 0, 2 * Math.PI);
    ctx.fillStyle = 'rgba(24, 144, 255, 0.1)';
    ctx.fill();
    ctx.strokeStyle = '#1890ff';
    ctx.lineWidth = 2;
    ctx.setLineDash([6, 4]);
    ctx.stroke();
    ctx.setLineDash([]);
    ctx.restore();

    ctx.fillStyle = '#1890ff';
    ctx.font = 'bold 12px sans-serif';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';
    ctx.fillText(anchor.name, cx, cy - radius - 10);
  });
}