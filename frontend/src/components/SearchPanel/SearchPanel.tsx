// src/components/SearchPanel/SearchPanel.tsx
import React from 'react';
import type { SearchPanelProps } from './SearchPanel.types';

export function SearchPanel({
  searchName,
  setSearchName,
  showAll,
  onShowAllChange,
  searchDepth,
  setSearchDepth,
  vCap,
  setVCap,
  loading,
  onSubmit,
  showHiddenLinks,
  onToggleHiddenLinks,
  colorMode,
  setColorMode,
  highlightCluster,
  onToggleHighlightCluster,
  leftOpen,
  onToggleLeft,
}: SearchPanelProps) {
  return (
    <aside className={`sidebar search-panel ${leftOpen ? 'open' : 'closed'}`}>
      <button
        type="button"
        className="sidebar-handle"
        onClick={onToggleLeft}
        title={leftOpen ? 'Свернуть поиск' : 'Развернуть поиск'}
        aria-label={leftOpen ? 'Свернуть поиск' : 'Развернуть поиск'}
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
          <circle cx="11" cy="11" r="7" />
          <line x1="21" y1="21" x2="16.65" y2="16.65" />
        </svg>
      </button>

      <div className="sidebar-content">
        <h2>Поиск и Аналитика</h2>
        <form onSubmit={onSubmit}>
          <div className="form-group">
            <label>Имя или ID объекта:</label>
            <input
              type="text"
              value={searchName}
              disabled={showAll}
              onChange={(e) => setSearchName(e.target.value)}
              placeholder={
                showAll
                  ? 'Снимите галочку для поиска...'
                  : 'Например, Кузнецова...'
              }
            />
          </div>

          <div className="form-group checkbox">
            <label>
              <input
                type="checkbox"
                checked={showAll}
                onChange={(e) => onShowAllChange(e.target.checked)}
              />
              Показать граф полностью
            </label>
          </div>

          <div className="form-group">
            <label>Глубина поиска (слои): {searchDepth}</label>
            <input
              type="range"
              min="1"
              max="3"
              value={searchDepth}
              disabled={showAll}
              onChange={(e) => setSearchDepth(Number(e.target.value))}
            />
          </div>

          <div className="form-group">
            <label>Макс. вершин (V_cap):</label>
            <input
              type="number"
              value={vCap}
              onChange={(e) =>
                setVCap(e.target.value === '' ? '' : Number(e.target.value))
              }
              placeholder="Без ограничений"
            />
          </div>

          <button type="submit" className="btn-submit" disabled={loading}>
            {loading ? 'Загрузка...' : 'Рассчитать граф'}
          </button>
        </form>

        <div className="analysis-actions-block">
          <h3>Модули восстановления РФ-связей</h3>

          <div className="form-group checkbox">
            <label className="toggle-label">
              <input
                type="checkbox"
                checked={showHiddenLinks}
                onChange={(e) => onToggleHiddenLinks(e.target.checked)}
              />
              <span className="badge-alert">NEW</span> Подсветить скрытые
              связи за рубежом
            </label>
          </div>

          <div className="form-group">
            <label>Режим раскраски графа:</label>
            <div className="radio-group">
              <button
                type="button"
                className={`btn-toggle ${colorMode === 'type' ? 'active' : ''}`}
                onClick={() => setColorMode('type')}
              >
                Тип (Человек/Проект)
              </button>
              <button
                type="button"
                className={`btn-toggle ${colorMode === 'geo' ? 'active' : ''}`}
                onClick={() => setColorMode('geo')}
              >
                Гео-след (РФ / Зарубеж)
              </button>
            </div>
          </div>

          <button
            type="button"
            className={`btn-analytics ${highlightCluster ? 'active' : ''}`}
            onClick={onToggleHighlightCluster}
          >
            {highlightCluster
              ? '⚡ Сбросить кластеры'
              : '🔍 Найти научные школы (СВК)'}
          </button>
        </div>

        <div className="analytics-notice">
          <small>
            Система деанонимизации скрытых связей работает в тестовом режиме
            моков.
          </small>
        </div>
      </div>
    </aside>
  );
}