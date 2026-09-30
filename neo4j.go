package main

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"net/url"
	"strings"
)

type neo4j struct {
	endpoint string
	user     string
	password string
	client   *http.Client
}

func newNeo4j(baseURL, database, user, password string, client *http.Client) (*neo4j, error) {
	u, err := url.Parse(strings.TrimRight(baseURL, "/"))
	if err != nil || (u.Scheme != "http" && u.Scheme != "https") || u.Host == "" {
		return nil, fmt.Errorf("invalid Neo4j HTTP URL")
	}
	if database == "" || strings.ContainsAny(database, "/?#") {
		return nil, fmt.Errorf("invalid Neo4j database")
	}
	u.Path = strings.TrimRight(u.Path, "/") + "/db/" + url.PathEscape(database) + "/query/v2"
	return &neo4j{endpoint: u.String(), user: user, password: password, client: client}, nil
}

func (db *neo4j) query(ctx context.Context, statement string, parameters map[string]any) ([][]json.RawMessage, error) {
	request := struct {
		Statement  string         `json:"statement"`
		Parameters map[string]any `json:"parameters"`
	}{statement, parameters}
	b, err := json.Marshal(request)
	if err != nil {
		return nil, err
	}
	req, err := http.NewRequestWithContext(ctx, http.MethodPost, db.endpoint, bytes.NewReader(b))
	if err != nil {
		return nil, err
	}
	req.SetBasicAuth(db.user, db.password)
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Accept", "application/json")
	resp, err := db.client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	if resp.StatusCode != http.StatusAccepted {
		body, _ := io.ReadAll(io.LimitReader(resp.Body, 2048))
		return nil, fmt.Errorf("Neo4j HTTP %d: %s", resp.StatusCode, strings.TrimSpace(string(body)))
	}
	var result struct {
		Data struct {
			Values [][]json.RawMessage `json:"values"`
		} `json:"data"`
		Errors []struct {
			Code    string `json:"code"`
			Message string `json:"message"`
		} `json:"errors"`
	}
	if err := json.NewDecoder(io.LimitReader(resp.Body, 32<<20)).Decode(&result); err != nil {
		return nil, err
	}
	if len(result.Errors) > 0 {
		return nil, fmt.Errorf("Neo4j %s: %s", result.Errors[0].Code, result.Errors[0].Message)
	}
	return result.Data.Values, nil
}

const importCypher = `UNWIND $rows AS row MERGE (w:Work {id:row.id}) SET w.title=row.title, w.doi=row.doi, w.year=row.year WITH w,row UNWIND row.authors AS author MERGE (a:Author {id:author.id}) SET a.name=author.name, a.firstName=author.firstName, a.lastName=author.lastName, a.orcid=CASE WHEN author.orcid <> '' THEN author.orcid ELSE a.orcid END, a.country='RUSSIA' MERGE (a)-[:AUTHORED]->(w) MERGE (a)-[:AUTHORED_AT {universityId:$universityId}]->(w) FOREACH (inst IN author.institutions | MERGE (i:Institution {id:inst.id}) SET i.name=inst.display_name, i.countryCode=inst.country_code, i.type=inst.type MERGE (a)-[:AFFILIATED_WITH]->(i))`

func (db *neo4j) schema(ctx context.Context) error {
	queries := []string{
		"CREATE CONSTRAINT author_id IF NOT EXISTS FOR (a:Author) REQUIRE a.id IS UNIQUE",
		"CREATE CONSTRAINT work_id IF NOT EXISTS FOR (w:Work) REQUIRE w.id IS UNIQUE",
		"CREATE CONSTRAINT institution_id IF NOT EXISTS FOR (i:Institution) REQUIRE i.id IS UNIQUE",
		"CREATE CONSTRAINT school_id IF NOT EXISTS FOR (s:School) REQUIRE s.id IS UNIQUE",
	}
	for _, q := range queries {
		if _, err := db.query(ctx, q, nil); err != nil {
			return err
		}
	}
	return nil
}

func decodeString(row []json.RawMessage, col int) (string, error) {
	if len(row) <= col {
		return "", fmt.Errorf("missing Neo4j column %d", col)
	}
	var value string
	err := json.Unmarshal(row[col], &value)
	return value, err
}

func (db *neo4j) authorPage(ctx context.Context, universityID, after string, limit int) ([]string, error) {
	rows, err := db.query(ctx, "MATCH (a:Author)-[:AUTHORED_AT {universityId:$universityId}]->(:Work) WHERE a.id > $after RETURN DISTINCT a.id AS id ORDER BY id LIMIT $limit", map[string]any{"universityId": universityID, "after": after, "limit": limit})
	if err != nil {
		return nil, err
	}
	ids := make([]string, 0, len(rows))
	for _, row := range rows {
		id, err := decodeString(row, 0)
		if err != nil {
			return nil, err
		}
		ids = append(ids, id)
	}
	return ids, nil
}

func (db *neo4j) project(ctx context.Context, universityID string, batch int) error {
	const q = `MATCH (a:Author)-[:AUTHORED_AT {universityId:$universityId}]->(w:Work)<-[:AUTHORED_AT {universityId:$universityId}]-(b:Author) WHERE a.id > $after AND a.id <= $end AND a.id < b.id WITH a,b,count(DISTINCT w) AS weight MERGE (a)-[r:COAUTHORED {universityId:$universityId}]->(b) SET r.weight=weight RETURN count(r)`
	last := ""
	for {
		ids, err := db.authorPage(ctx, universityID, last, batch)
		if err != nil {
			return err
		}
		if len(ids) == 0 {
			count, err := db.count(ctx, "MATCH (:Author)-[r:COAUTHORED {universityId:$universityId}]->(:Author) RETURN count(r)", map[string]any{"universityId": universityID})
			if err != nil {
				return err
			}
			fmt.Printf("coauthor edges in Neo4j: %d\n", count)
			return nil
		}
		end := ids[len(ids)-1]
		if _, err := db.query(ctx, q, map[string]any{"universityId": universityID, "after": last, "end": end}); err != nil {
			return err
		}
		last = end
		fmt.Printf("projected coauthor edges through %s\n", last)
	}
}

func (db *neo4j) count(ctx context.Context, statement string, parameters map[string]any) (int64, error) {
	rows, err := db.query(ctx, statement, parameters)
	if err != nil {
		return 0, err
	}
	if len(rows) != 1 || len(rows[0]) != 1 {
		return 0, fmt.Errorf("unexpected Neo4j count result")
	}
	var value int64
	if err := json.Unmarshal(rows[0][0], &value); err != nil {
		return 0, err
	}
	return value, nil
}

func (db *neo4j) stats(ctx context.Context, universityID string) error {
	queries := []struct {
		label string
		query string
	}{
		{"authors", "MATCH (a:Author)-[:AUTHORED_AT {universityId:$universityId}]->(:Work) RETURN count(DISTINCT a)"},
		{"works", "MATCH (:Author)-[:AUTHORED_AT {universityId:$universityId}]->(w:Work) RETURN count(DISTINCT w)"},
		{"authorships", "MATCH (:Author)-[r:AUTHORED_AT {universityId:$universityId}]->(:Work) RETURN count(r)"},
		{"coauthor edges", "MATCH (:Author)-[r:COAUTHORED {universityId:$universityId}]->(:Author) RETURN count(r)"},
		{"school memberships", "MATCH (:Author)-[r:MEMBER_OF {universityId:$universityId}]->(:School) RETURN count(r)"},
	}
	for _, item := range queries {
		value, err := db.count(ctx, item.query, map[string]any{"universityId": universityID})
		if err != nil {
			return err
		}
		fmt.Printf("%s: %d\n", item.label, value)
	}
	rows, err := db.query(ctx, "MATCH (a:Author)-[:AUTHORED_AT {universityId:$universityId}]->(w:Work)<-[:AUTHORED_AT {universityId:$universityId}]-(b:Author) WHERE a.id < b.id RETURN a.id LIMIT 1", map[string]any{"universityId": universityID})
	if err != nil {
		return err
	}
	fmt.Printf("at least one shared work between selected-university authors: %t\n", len(rows) > 0)
	return nil
}

func (db *neo4j) graph(ctx context.Context, universityID string, batch int) ([]string, []edge, error) {
	ids := make([]string, 0)
	last := ""
	for {
		page, err := db.authorPage(ctx, universityID, last, batch)
		if err != nil {
			return nil, nil, err
		}
		if len(page) == 0 {
			break
		}
		ids = append(ids, page...)
		last = page[len(page)-1]
	}
	ids, index, err := sortedIDs(ids)
	if err != nil {
		return nil, nil, err
	}
	edges := make([]edge, 0)
	last = ""
	for {
		page, err := db.authorPage(ctx, universityID, last, batch)
		if err != nil {
			return nil, nil, err
		}
		if len(page) == 0 {
			break
		}
		end := page[len(page)-1]
		rows, err := db.query(ctx, "MATCH (a:Author)-[r:COAUTHORED {universityId:$universityId}]->(b:Author) WHERE a.id > $after AND a.id <= $end RETURN a.id,b.id,r.weight", map[string]any{"universityId": universityID, "after": last, "end": end})
		if err != nil {
			return nil, nil, err
		}
		for _, row := range rows {
			from, err := decodeString(row, 0)
			if err != nil {
				return nil, nil, err
			}
			to, err := decodeString(row, 1)
			if err != nil {
				return nil, nil, err
			}
			var weight float64
			if len(row) < 3 || json.Unmarshal(row[2], &weight) != nil {
				return nil, nil, fmt.Errorf("invalid coauthor weight")
			}
			fi, fok := index[from]
			ti, tok := index[to]
			if !fok || !tok {
				return nil, nil, fmt.Errorf("edge refers to unknown author")
			}
			edges = append(edges, edge{From: fi, To: ti, Weight: weight})
		}
		last = end
	}
	return ids, edges, nil
}

func (db *neo4j) saveSchools(ctx context.Context, universityID string, ids []string, membership []int64, batch int) error {
	if len(ids) != len(membership) {
		return fmt.Errorf("membership size mismatch")
	}
	key := map[int64]string{}
	for i, cluster := range membership {
		if first, ok := key[cluster]; !ok || ids[i] < first {
			key[cluster] = ids[i]
		}
	}
	const q = `UNWIND $rows AS row MATCH (a:Author {id:row.id}) OPTIONAL MATCH (a)-[old:MEMBER_OF {universityId:$universityId}]->(:School) DELETE old WITH a,row MERGE (s:School {id:row.schoolId}) SET s.method='Leiden', s.country='RUSSIA', s.universityId=$universityId MERGE (a)-[:MEMBER_OF {universityId:$universityId}]->(s)`
	for start := 0; start < len(ids); start += batch {
		end := start + batch
		if end > len(ids) {
			end = len(ids)
		}
		rows := make([]map[string]string, 0, end-start)
		for i := start; i < end; i++ {
			rows = append(rows, map[string]string{"id": ids[i], "schoolId": universityID + "/" + key[membership[i]]})
		}
		if _, err := db.query(ctx, q, map[string]any{"universityId": universityID, "rows": rows}); err != nil {
			return err
		}
	}
	_, err := db.query(ctx, "MATCH (s:School {universityId:$universityId}) WHERE NOT EXISTS { MATCH (:Author)-[:MEMBER_OF {universityId:$universityId}]->(s) } DELETE s", map[string]any{"universityId": universityID})
	return err
}
