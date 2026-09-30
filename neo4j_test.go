package main

import (
	"context"
	"encoding/json"
	"io"
	"net/http"
	"strings"
	"testing"
)

func TestNeo4jChecksErrorsEvenOn202(t *testing.T) {
	client := &http.Client{Transport: roundTripFunc(func(r *http.Request) (*http.Response, error) {
		return &http.Response{StatusCode: http.StatusAccepted, Body: io.NopCloser(strings.NewReader(`{"errors":[{"code":"Neo.ClientError.Statement.SyntaxError","message":"bad query"}]}`))}, nil
	})}
	db, err := newNeo4j("http://localhost:7474", "neo4j", "neo4j", "test", client)
	if err != nil {
		t.Fatal(err)
	}
	if _, err := db.query(context.Background(), "bad", nil); err == nil {
		t.Fatal("expected Neo4j query error")
	}
}

func TestProjectUsesSelectedUniversityOnly(t *testing.T) {
	pageCalls := 0
	client := &http.Client{Transport: roundTripFunc(func(r *http.Request) (*http.Response, error) {
		var request struct {
			Statement  string         `json:"statement"`
			Parameters map[string]any `json:"parameters"`
		}
		if err := json.NewDecoder(r.Body).Decode(&request); err != nil {
			t.Fatal(err)
		}
		if request.Parameters["universityId"] != defaultUniversityID || !strings.Contains(request.Statement, "universityId:$universityId") {
			t.Fatalf("query is not scoped: %q %#v", request.Statement, request.Parameters)
		}
		response := `{"data":{"values":[]}}`
		switch {
		case strings.Contains(request.Statement, "RETURN DISTINCT a.id AS id"):
			pageCalls++
			if pageCalls == 1 {
				response = `{"data":{"values":[["A1"],["A2"]]}}`
			}
		case strings.Contains(request.Statement, "RETURN count(r)"):
			response = `{"data":{"values":[[1]]}}`
		}
		return &http.Response{StatusCode: 202, Body: io.NopCloser(strings.NewReader(response))}, nil
	})}
	db, err := newNeo4j("http://localhost:7474", "neo4j", "neo4j", "test", client)
	if err != nil {
		t.Fatal(err)
	}
	if err := db.project(context.Background(), defaultUniversityID, 100); err != nil {
		t.Fatal(err)
	}
	if pageCalls != 2 {
		t.Fatalf("expected two author-page queries, got %d", pageCalls)
	}
}
