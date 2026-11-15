// src/components/CommunityHull.tsx
interface CommunityHullProps {
  ctx: CanvasRenderingContext2D;
  nodes: any[];
  expandedCommunities: Set<number>;
  getRadius: (memberCount: number) => number;
}

export function drawCommunityHulls({ ctx, nodes, expandedCommunities, getRadius }: CommunityHullProps) {
  if (!ctx || !nodes.length) return;

  expandedCommunities.forEach(cid => {
    const anchor = nodes.find(n => n.id === `community-${cid}`);
    if (!anchor) return;

    const members = nodes.filter(n => n.parentCommunityId === cid);
    const memberCount = members.length;

    // 👇 Используем переданную функцию (вашу новую формулу)
    const radius = getRadius(memberCount);

    const cx = anchor.x ?? 0;
    const cy = anchor.y ?? 0;

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