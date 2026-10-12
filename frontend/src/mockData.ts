// src/mockData.ts

export interface Node {
  id: string;
  name: string;
  val: number;
  type: 'project' | 'person';
  location: 'RU' | 'INT' | 'HIDDEN'; // РФ, Зарубеж или скрытая связь
  info?: string;
}

export interface Link {
  source: string;
  target: string;
  isHidden?: boolean; // Флаг восстановленной бэкендом скрытой связи
}

export const mockGraphData = {
  nodes: [
    { id: '1', name: 'Иванов И.И.', val: 18, type: 'person', location: 'RU', info: 'Профессор, руководитель лаборатории ИИ в РФ. В зарубежных статьях 2024-2026 гг. указан как "Independent Researcher".' },
    { id: '3', name: 'Петров П.П.', val: 12, type: 'person', location: 'RU', info: 'Аспирант. Публикуется в РФ, но имеет скрытые коммиты в репозитории зарубежных коллег.' },
    { id: '5', name: 'Сидоров С.С.', val: 10, type: 'person', location: 'RU', info: 'Младший научный сотрудник. Специализируется на теории графов.' },
    { id: '6', name: 'Кузнецова А.В.', val: 15, type: 'person', location: 'INT', info: 'Работает в Германии, но исторически связана коллективными грантами с НИУ ВШЭ.' },
    { id: '7', name: 'Смирнов Д.М.', val: 14, type: 'person', location: 'RU', info: 'Старший научный сотрудник. Эксперт по сильно-связным компонентам.' },
    { id: '8', name: 'Попова Е.А.', val: 11, type: 'person', location: 'RU', info: 'Лаборант-исследователь.' },
    
    { id: '2', name: 'Проект "Science Schools"', val: 25, type: 'project', location: 'RU', info: 'Платформа анализа научных школ РФ.' },
    { id: '4', name: 'НИУ ВШЭ', val: 22, type: 'project', location: 'RU', info: 'Университет-партнер.' },
    { id: '9', name: 'Грант РНФ "Графы ИИ"', val: 20, type: 'project', location: 'RU', info: 'Целевой грант Российского научного фонда.' },
    { id: '10', name: 'Институт Системного Программирования', val: 21, type: 'project', location: 'RU', info: 'Академический институт-партнер.' },
    { id: '11', name: 'Конференция "Data Science 2026"', val: 16, type: 'project', location: 'INT', info: 'Международная площадка.' },
    { id: '12', name: 'Лаборатория Сетевых Моделей', val: 19, type: 'project', location: 'RU', info: 'Закрытое подразделение.' }
  ] as Node[],

  links: [
    { source: '1', target: '2' },
    { source: '3', target: '2' },
    { source: '6', target: '2' },
    { source: '8', target: '2' },
    { source: '1', target: '4' }, 
    { source: '6', target: '4' },
    { source: '5', target: '10' },
    { source: '7', target: '10' },
    { source: '1', target: '9' },
    { source: '7', target: '9' },
    { source: '3', target: '12' },
    { source: '5', target: '12' },
    { source: '1', target: '12' },
    { source: '6', target: '11' },
    { source: '3', target: '11' },
    { source: '7', target: '11' },
    
    // Восстановленные скрытые связи (будут подсвечиваться пунктиром)
    { source: '6', target: '1', isHidden: true }, // Скрытая связь Кузнецовой и Иванова
    { source: '6', target: '3', isHidden: true }  // Скрытая связь Кузнецовой и Петрова
  ] as Link[]
};
