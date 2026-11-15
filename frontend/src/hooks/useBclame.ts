// src/hooks/useBclame.ts (ВРЕМЕННЫЙ МОК)
import { useCallback } from 'react';

export type BclameMembership = {
  run_id: number;
  author_id: number;
  community_id: number;
};

export type BclameProject = {
  project_id: string;
  project_name: string;
  author_ids: string[];
};

export type BclameMockResponse = {
  memberships: Map<string, number>;
  projects: BclameProject[];
};

export function useBclame() {
  const fetchMockData = useCallback(async (): Promise<BclameMockResponse> => {
    const memberships = new Map<string, number>();
    const projects: BclameProject[] = [];

    let authorCounter = 1;

    // 👇 Функция для случайного выбора пар (без повторов)
    const getRandomPairs = (arr: string[], count: number): [string, string][] => {
      const pairs: [string, string][] = [];
      const used = new Set<string>();

      // Максимальное число уникальных пар
      const maxPairs = (arr.length * (arr.length - 1)) / 2;
      const targetCount = Math.min(count, maxPairs);

      let attempts = 0;
      while (pairs.length < targetCount && attempts < 100) {
        const i = Math.floor(Math.random() * arr.length);
        const j = Math.floor(Math.random() * arr.length);
        if (i === j) {
          attempts++;
          continue;
        }
        const a = arr[i];
        const b = arr[j];
        const key = a < b ? `${a}-${b}` : `${b}-${a}`;
        if (!used.has(key)) {
          used.add(key);
          pairs.push(a < b ? [a, b] : [b, a]);
        }
        attempts++;
      }

      return pairs;
    };

    for (let cid = 1; cid <= 10; cid++) {
      const peopleCount = cid;
      const authorIds: string[] = [];

      // Создаём людей в сообществе
      for (let j = 0; j < peopleCount; j++) {
        const authorId = String(authorCounter++);
        authorIds.push(authorId);
        memberships.set(authorId, cid);
      }

      // 👇 Случайные внутренние рёбра (примерно n/2 пар)
      if (peopleCount >= 2) {
        const pairCount = Math.max(1, Math.floor(peopleCount / 2));
        const randomPairs = getRandomPairs(authorIds, pairCount);

        randomPairs.forEach(([a, b], idx) => {
          projects.push({
            project_id: `proj-inner-${cid}-${idx}`,
            project_name: `Внутренний проект С${cid}-${idx}`,
            author_ids: [a, b],
          });
        });
      }

      // Межсообществные проекты (связывают соседние сообщества)
      if (cid > 1) {
        let prevFirst = '';
        for (let k = 1; k < authorCounter; k++) {
          if (memberships.get(String(k)) === cid - 1) {
            prevFirst = String(k);
            break;
          }
        }
        if (prevFirst && authorIds[0]) {
          projects.push({
            project_id: `proj-cross-${cid - 1}-${cid}`,
            project_name: `Межсообществный проект С${cid - 1}—С${cid}`,
            author_ids: [prevFirst, authorIds[0]],
          });
        }
      }
    }

    return { memberships, projects };
  }, []);

  return fetchMockData;
}