#include "database/db_client.hpp"   
#include <nlohmann/json.hpp>
#include <vector>
#include <iostream>
#include <curl/curl.h>



DBClient::DBClient(const std::string& connection_string) : sql_(soci::postgresql, connection_string){}

std::vector<CoauthorshipEdge> DBClient::get_coauthorship_edges() {
    std::vector<CoauthorshipEdge> edges;
    soci::rowset<soci::row> rows = (sql_.prepare <<"SELECT source_author_id, target_author_id, weight FROM coauthorship_edges");
    for (const auto& row:rows) {
        CoauthorshipEdge edge;
        edge.source_id = static_cast<uint32_t>(row.get<int>(0));
        edge.target_id = static_cast<uint32_t>(row.get<int>(1));
        edge.weight = row.get<int>(2);
        edges.push_back(edge);
    }
    return edges;
}

void DBClient::update_communities(const std::vector<uint32_t>& communities) {
    soci::transaction tr(sql_);
    int community_id, author_id;
    soci::statement st = (sql_.prepare <<"UPDATE authors SET community_id = :community_id WHERE id = :author_id", soci::use(community_id), soci::use(author_id));
    for (int i = 0; i < communities.size(); i++) {
        author_id = i;
        community_id = static_cast<int>(communities[i]);
        st.execute(true);
    }
    tr.commit();
}

std::string DBClient::get_all_authors_json() {
    nlohmann::json result = nlohmann::json::array();
    soci::rowset<soci::row> rows =
        (sql_.prepare <<
            "SELECT id, openalex_id, name, community_id, institution_name "
            "FROM authors ORDER BY id");

    for (const auto& row : rows) {
        nlohmann::json author;
        author["id"] = row.get<int>(0);
        author["openalex_id"] = row.get<std::string>(1);
        author["name"] = row.get<std::string>(2);
        if (row.get_indicator(3) != soci::i_null)
            author["community_id"] = row.get<int>(3);
        else
            author["community_id"] = nullptr;
        if (row.get_indicator(4) != soci::i_null)
            author["institution_name"] = row.get<std::string>(4);
        else
            author["institution_name"] = nullptr;
        result.push_back(author);
    }
    return result.dump();
}

std::string DBClient::get_author_card_json(uint32_t author_id){
    nlohmann::json result;
    soci::rowset<soci::row> rows = (sql_.prepare << "SELECT openalex_id, name, community_id, institution_name, art.id, art.title, art.year FROM authors a LEFT JOIN author_to_article ata ON ata.author_id = a.id LEFT JOIN author_meta_articles art ON art.id = ata.article_id WHERE a.id = :author_id ORDER BY art.year DESC NULLS LAST LIMIT 5", soci::use(author_id));
    bool fl = false;
    result["articles"] = nlohmann::json::array();
    for (const auto& row: rows) {
        if (!fl) {
            fl = true;
            result["openalex_id"] = row.get<std::string>(0);
            result["name"] = row.get<std::string>(1);
            if (row.get_indicator(2) != soci::i_null) {
                result["community_id"] = row.get<int>(2);
            } else {
                result["community_id"] = nullptr;
            }
            if (row.get_indicator(3) != soci::i_null) {
                result["institution_name"] = row.get<std::string>(3);
            } else {
                result["institution_name"] = nullptr;
            }
        }
        if (row.get_indicator(4) != soci::i_null) {
            nlohmann::json article;
            article["id"] = row.get<std::string>(4);
            article["title"] = row.get<std::string>(5);
            article["year"] = row.get<int>(6);
            result["articles"].push_back(article);
        }
    }
    return result.dump();
}

void DBClient::clear_import_data() {
    soci::transaction tr(sql_);
    sql_ << "DELETE FROM author_to_article";
    sql_ << "DELETE FROM coauthorship_edges";
    sql_ << "DELETE FROM author_meta_articles";
    sql_ << "DELETE FROM authors";
    tr.commit();
}

void DBClient::insert_author(int id, const std::string& openalex_id, const std::string& name, const std::string& institution_name) {
    sql_ << "INSERT INTO authors (id, openalex_id, name, institution_name) VALUES (:id, :openalex_id, :name, :institution_name) on conflict do nothing", soci::use(id), soci::use(openalex_id), soci::use(name), soci::use(institution_name);
}

void DBClient::insert_article(const std::string& id, const std::string& title, int year) {
    sql_ << "INSERT INTO author_meta_articles (id, title, year) VALUES (:id, :title, :year) on conflict (id) do nothing", soci::use(id), soci::use(title), soci::use(year);
}

void DBClient::insert_author_article(int author_id, const std::string& article_id) {
    sql_ << "INSERT INTO author_to_article (author_id, article_id) VALUES (:author_id, :article_id) on conflict do nothing", soci::use(author_id), soci::use(article_id);
}

void DBClient::insert_edge(int source, int target, int weight) {
    sql_ << "INSERT INTO coauthorship_edges (source_author_id, target_author_id, weight) VALUES (:source, :target, :weight) on conflict do nothing", soci::use(source), soci::use(target), soci::use(weight);
}
