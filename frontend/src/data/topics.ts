// src/data/topics.ts
export interface Topic {
  id: string;
  name: string;
  color: string;
}

export const TOPICS: Topic[] = [
  { id: 'math', name: 'Математика', color: '#1890ff' },
  { id: 'bio', name: 'Биология', color: '#52c41a' },
  { id: 'cs', name: 'Информатика', color: '#faad14' },
  { id: 'physics', name: 'Физика', color: '#eb2f96' },
  { id: 'chem', name: 'Химия', color: '#722ed1' },
];