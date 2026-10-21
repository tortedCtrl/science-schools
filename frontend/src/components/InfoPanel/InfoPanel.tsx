// src/components/InfoPanel/InfoPanel.tsx
import React from 'react';

export interface InfoPanelProps {
  selectedNode: any;
  rightOpen: boolean;
  onToggleRight: () => void;
}

export function InfoPanel({
  selectedNode,
  rightOpen,
  onToggleRight,
}: InfoPanelProps) {
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
      </div>
    </aside>
  );
}