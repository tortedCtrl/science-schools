// src/components/InfoPanel/InfoPanel.tsx
import type { CommunityTopicsResponse } from '../../hooks/backend';

type InfoPanelNode = {
  id: string;
  name: string;
  community_id?: number | null;
  location?: string;
  type?: string;
  info?: string;
};

export interface InfoPanelProps {
  selectedNode: InfoPanelNode | null;
  rightOpen: boolean;
  onToggleRight: () => void;
  showCommunityTopics: boolean;
  communityTopics: CommunityTopicsResponse[];
  communityTopicsLoading: boolean;
  communityTopicsError: string | null;
}

export function InfoPanel({
  selectedNode,
  rightOpen,
  onToggleRight,
  showCommunityTopics,
  communityTopics,
  communityTopicsLoading,
  communityTopicsError,
}: InfoPanelProps) {
  const selectedCommunityTopics = communityTopics.find(
    (community) => community.community_id === selectedNode?.community_id
  );

  return (
    <aside className={`sidebar info-panel ${rightOpen ? 'open' : 'closed'}`}>
      <button
        type="button"
        className="sidebar-handle"
        onClick={onToggleRight}
        title={rightOpen ? 'Свернуть информацию' : 'Развернуть информацию'}
        aria-label={
          rightOpen ? 'Свернуть информацию' : 'Развернуть информацию'
        }
      >
        <svg
          className="handle-icon"
          viewBox="0 0 24 24"
          width="20"
          height="20"
          fill="none"
          stroke="currentColor"
          strokeWidth="2"
          strokeLinecap="round"
          strokeLinejoin="round"
        >
          <circle cx="12" cy="12" r="9" />
          <line x1="12" y1="11" x2="12" y2="16" />
          <circle cx="12" cy="8" r="0.6" fill="currentColor" stroke="none" />
        </svg>
      </button>

      <div className="sidebar-content">
        <h2>Информация (Show_info)</h2>
        {selectedNode ? (
          <div className="node-details">
            <h3>{selectedNode.name}</h3>
            <p>
              <strong>ID:</strong> {selectedNode.id}
            </p>
            <p>
              <strong>Регион публикаций:</strong>{' '}
              {selectedNode.location === 'RU'
                ? '🇷🇺 Россия'
                : '🌐 Международный / Скрытый'}
            </p>
            <p>
              <strong>Категория:</strong>{' '}
              {selectedNode.type === 'project'
                ? 'Организация / Проект'
                : 'Ученый / Сотрудник'}
            </p>
            <hr />
            <p style={{ lineHeight: '1.5' }}>{selectedNode.info}</p>
          </div>
        ) : (
          <p className="placeholder-text">
            Кликните на объект, чтобы изучить восстановленные бэкендом
            атрибуты и скрытые аффилиации.
          </p>
        )}
        {showCommunityTopics && (
          <section className="community-topics">
            <hr />
            <h3>Популярные темы научной школы</h3>
            {communityTopicsLoading ? (
              <p className="placeholder-text">Загрузка тем...</p>
            ) : communityTopicsError ? (
              <p className="community-topics-error">{communityTopicsError}</p>
            ) : selectedNode?.community_id == null ? (
              <p className="placeholder-text">
                Выберите ученого, чтобы посмотреть темы его научной школы.
              </p>
            ) : selectedCommunityTopics?.topics.length ? (
              <ol className="community-topic-list">
                {selectedCommunityTopics.topics.map((topic, index) => (
                  <li key={topic.topic_id}>
                    <span className="community-topic-rank">{index + 1}.</span>
                    <span className="community-topic-name">{topic.name}</span>
                    <span className="community-topic-weight">
                      {topic.weight.toFixed(3)}
                    </span>
                  </li>
                ))}
              </ol>
            ) : (
              <p className="placeholder-text">
                Для этого сообщества темы не найдены.
              </p>
            )}
          </section>
        )}
      </div>
    </aside>
  );
}