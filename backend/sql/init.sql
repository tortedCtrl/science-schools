CREATE TABLE authors (
    id integer PRIMARY KEY,
    openalex_id VARCHAR(64) NOT NULL UNIQUE,
    name VARCHAR(512) NOT NULL,
    commuinity_id INTEGER DEFAULT NULL,
    institution_name VARCHAR(512)
);

CREATE TABLE coauthorship_edges (
    source_author_id INTEGER NOT NULL REFERENCES authors(id),
    target_author_id INTEGER NOT NULL REFERENCES authors(id),
    weight INTEGER,
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
