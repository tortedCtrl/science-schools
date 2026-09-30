package main

import (
	"context"
	"io"
	"net/http"
	"strings"
	"testing"
)

func TestResolveUniversityDefaultAndID(t *testing.T) {
	for _, choice := range []string{"spbu", "СПбГУ", "I172901346", "https://openalex.org/I172901346"} {
		id, err := resolveUniversity(context.Background(), nil, "", "", choice)
		if err != nil || id != defaultUniversityID {
			t.Fatalf("choice %q: id=%q err=%v", choice, id, err)
		}
	}
}

func TestResolveUniversityByName(t *testing.T) {
	client := &http.Client{Transport: roundTripFunc(func(r *http.Request) (*http.Response, error) {
		if r.URL.Query().Get("search") != "ITMO University" || r.URL.Query().Get("filter") != "country_code:RU,type:education" {
			t.Fatalf("unexpected institution search URL: %s", r.URL)
		}
		return &http.Response{StatusCode: 200, Body: io.NopCloser(strings.NewReader(`{"results":[{"id":"https://openalex.org/I173089394","display_name":"ITMO University","country_code":"RU","type":"education"}]}`))}, nil
	})}
	id, err := resolveUniversity(context.Background(), client, "https://api.openalex.org", "", "ITMO University")
	if err != nil || id != "I173089394" {
		t.Fatalf("id=%q err=%v", id, err)
	}
}
