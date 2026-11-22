#pragma once

// Domain DB Entities (map 1:1 to database tables)
#include "domain/db/author.hpp"
#include "domain/db/edge.hpp"
#include "domain/db/article.hpp"
#include "domain/db/author_article.hpp"
#include "domain/db/article_topic.hpp"
#include "domain/db/membership.hpp"
#include "domain/db/community_topic.hpp"
#include "domain/db/clustering_run.hpp"

// DTO / API Models (sent to frontend)
#include "domain/dto/author_summary.hpp"
#include "domain/dto/author_card.hpp"
#include "domain/dto/graph_data.hpp"
#include "domain/dto/community_summary.hpp"
#include "domain/dto/api_response.hpp"

// Internal / Processing Models
#include "domain/internal/work.hpp"
#include "domain/internal/article_topic_input.hpp"
#include "domain/internal/ingest_result.hpp"

// Common Types & Enums
#include "domain/common/types.hpp"
#include "domain/common/enums.hpp"
#include "domain/common/topic_hierarchy.hpp"