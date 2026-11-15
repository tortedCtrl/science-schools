CREATE TABLE authors (
    id INTEGER PRIMARY KEY,
    openalex_id VARCHAR(64) NOT NULL UNIQUE,
    name VARCHAR(512) NOT NULL,
    institution_name VARCHAR(512)
);

CREATE TABLE coauthorship_edges (
    source_author_id INTEGER NOT NULL REFERENCES authors(id),
    target_author_id INTEGER NOT NULL REFERENCES authors(id),
    weight INTEGER NOT NULL,
    CHECK (source_author_id < target_author_id),
    UNIQUE (source_author_id, target_author_id)
);

CREATE TABLE author_meta_articles (
    id VARCHAR(64) PRIMARY KEY,
    title VARCHAR(512) NOT NULL,
    year INTEGER
);

CREATE TABLE author_to_article (
    author_id INTEGER NOT NULL REFERENCES authors(id),
    article_id VARCHAR(64) NOT NULL REFERENCES author_meta_articles(id),
    PRIMARY KEY (author_id, article_id)
);

CREATE TABLE article_topics (
    article_id TEXT NOT NULL
        REFERENCES author_meta_articles(id) ON DELETE CASCADE,
    topic_id TEXT NOT NULL,
    topic_name TEXT NOT NULL,
    score DOUBLE PRECISION NOT NULL,
    is_primary BOOLEAN NOT NULL DEFAULT FALSE,
    subfield_id TEXT NOT NULL DEFAULT '',
    subfield_name TEXT NOT NULL DEFAULT '',
    field_id TEXT NOT NULL DEFAULT '',
    field_name TEXT NOT NULL DEFAULT '',
    domain_id TEXT NOT NULL DEFAULT '',
    domain_name TEXT NOT NULL DEFAULT '',
    PRIMARY KEY (article_id, topic_id)
);

CREATE TABLE clustering_runs (
    id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    algorithm TEXT NOT NULL CHECK (algorithm IN ('leiden', 'bigclam')),
    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE author_communities (
    run_id BIGINT NOT NULL REFERENCES clustering_runs(id) ON DELETE CASCADE,
    author_id INTEGER NOT NULL REFERENCES authors(id) ON DELETE CASCADE,
    community_id INTEGER NOT NULL,
    PRIMARY KEY (run_id, author_id, community_id)
);

CREATE TABLE community_topics (
    run_id BIGINT NOT NULL REFERENCES clustering_runs(id) ON DELETE CASCADE,
    community_id INTEGER NOT NULL,
    topic_id TEXT NOT NULL,
    topic_name TEXT NOT NULL,
    weight DOUBLE PRECISION NOT NULL,
    article_count INTEGER NOT NULL,
    PRIMARY KEY (run_id, community_id, topic_id)
);

CREATE INDEX idx_article_topics_topic_id
    ON article_topics (topic_id);

CREATE INDEX idx_author_to_article_article_id
    ON author_to_article (article_id);

CREATE INDEX idx_author_communities_author_run
    ON author_communities (author_id, run_id);

CREATE INDEX idx_author_communities_run_community
    ON author_communities (run_id, community_id);

CREATE INDEX idx_community_topics_run_weight
    ON community_topics (run_id, community_id, weight DESC);

CREATE INDEX idx_coauthorship_edges_target
    ON coauthorship_edges (target_author_id);

CREATE INDEX idx_clustering_runs_algorithm_created
    ON clustering_runs (algorithm, id DESC);

CREATE VIEW article_coauthorship_edges AS
SELECT ata1.article_id,
       ata1.author_id AS source_author_id,
       ata2.author_id AS target_author_id,
       art.year,
       at.topic_id,
       at.topic_name,
       at.score AS topic_score
FROM author_to_article ata1
JOIN author_to_article ata2
  ON ata2.article_id = ata1.article_id
 AND ata1.author_id < ata2.author_id
JOIN author_meta_articles art ON art.id = ata1.article_id
LEFT JOIN article_topics at ON at.article_id = ata1.article_id;
