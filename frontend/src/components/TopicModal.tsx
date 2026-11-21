import { TOPICS } from '../data/topics';

interface TopicModalProps {
  onSelect: (topicId: string) => void;
}

export function TopicModal({ onSelect }: TopicModalProps) {
  return (
    <div className="modal-overlay">
      <div className="modal-content">
        <h2>Выберите научную область</h2>
        <p>Для отображения графа сообществ выберите тему:</p>
        <div className="topic-grid">
          {TOPICS.map(topic => (
            <button
              key={topic.id}
              className="topic-btn"
              style={{ borderColor: topic.color }}
              onClick={() => onSelect(topic.id)}
            >
              <span className="topic-color" style={{ backgroundColor: topic.color }} />
              {topic.name}
            </button>
          ))}
        </div>
      </div>
    </div>
  );
}