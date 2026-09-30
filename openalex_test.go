package main

import (
	"context"
	"io"
	"net/http"
	"path/filepath"
	"strings"
	"testing"
)

func TestFetchWorksUsesCursorFilterAndKey(t *testing.T) {
	client := &http.Client{Transport: roundTripFunc(func(r *http.Request) (*http.Response, error) {
		if r.URL.Path != "/works" || r.URL.Query().Get("cursor") != "next" ||
			r.URL.Query().Get("filter") != "authorships.countries:RU,type:article" ||
			r.URL.Query().Get("per_page") != "100" || r.Header.Get("Authorization") != "Bearer test-key" {
			t.Errorf("unexpected OpenAlex request: %s", r.URL)
		}
		return &http.Response{StatusCode: 200, Body: io.NopCloser(strings.NewReader(`{"meta":{"count":1,"next_cursor":null},"results":[{"id":"W1","authorships":[{"author":{"id":"A1"},"countries":["RU"]}]}]}`))}, nil
	})}
	page, err := fetchWorks(context.Background(), client, "https://api.openalex.org", "test-key", "authorships.countries:RU,type:article", "next")
	if err != nil || len(page.Results) != 1 || page.Results[0].ID != "W1" {
		t.Fatalf("page=%#v err=%v", page, err)
	}
}

func TestIngestDoesNotAdvanceCursorWhenDatabaseFails(t *testing.T) {
	client := &http.Client{Transport: roundTripFunc(func(r *http.Request) (*http.Response, error) {
		if r.URL.Host == "api.openalex.org" {
			return &http.Response{StatusCode: 200, Body: io.NopCloser(strings.NewReader(`{"meta":{"count":2,"next_cursor":"page-2"},"results":[{"id":"W1","authorships":[{"author":{"id":"A1"},"institutions":[{"id":"https://openalex.org/I172901346","country_code":"RU"}]}]}]}`))}, nil
		}
		return &http.Response{StatusCode: 202, Body: io.NopCloser(strings.NewReader(`{"errors":[{"code":"Neo.ClientError.Statement.SyntaxError","message":"bad query"}]}`))}, nil
	})}
	db, err := newNeo4j("http://localhost:7474", "neo4j", "neo4j", "test", client)
	if err != nil {
		t.Fatal(err)
	}
	path := filepath.Join(t.TempDir(), "cursor.json")
	filter := "authorships.institutions.id:I172901346,type:article"
	if err := ingest(context.Background(), db, client, "https://api.openalex.org", "", path, filter, defaultUniversityID, 1); err == nil {
		t.Fatal("expected failed import")
	}
	cp, err := loadCheckpoint(path, filter)
	if err != nil || cp.Cursor != "*" {
		t.Fatalf("checkpoint moved after failed write: %#v, %v", cp, err)
	}
}

type roundTripFunc func(*http.Request) (*http.Response, error)

func (f roundTripFunc) RoundTrip(r *http.Request) (*http.Response, error) { return f(r) }
