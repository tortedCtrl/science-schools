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

CREATE TABLE author_communities (
    author_id INTEGER PRIMARY KEY REFERENCES authors(id) ON DELETE CASCADE,
    community_id INTEGER NOT NULL
);

CREATE INDEX author_communities_community_idx
    ON author_communities (community_id);

CREATE TABLE IF NOT EXISTS article_topics (
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

CREATE INDEX IF NOT EXISTS idx_article_topics_topic_id
    ON article_topics(topic_id);

CREATE TABLE IF NOT EXISTS community_topics (
    community_id INTEGER NOT NULL,
    topic_id TEXT NOT NULL,
    topic_name TEXT NOT NULL,
    weight DOUBLE PRECISION NOT NULL,
    article_count INTEGER NOT NULL,
    PRIMARY KEY (community_id, topic_id)
);

CREATE INDEX IF NOT EXISTS community_topics_weight_idx
    ON community_topics (community_id, weight DESC);

CREATE TABLE IF NOT EXISTS bigclam_memberships (
    author_id INTEGER NOT NULL REFERENCES authors(id) ON DELETE CASCADE,
    community_id INTEGER NOT NULL,
    PRIMARY KEY (author_id, community_id)
);

CREATE INDEX IF NOT EXISTS bigclam_memberships_community_idx
    ON bigclam_memberships (community_id);
