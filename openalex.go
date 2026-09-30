package main

import (
	"context"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"net/url"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"time"
)

type alexPage struct {
	Meta struct {
		Count      int     `json:"count"`
		NextCursor *string `json:"next_cursor"`
	} `json:"meta"`
	Results []work `json:"results"`
}

type checkpoint struct {
	Filter string `json:"filter"`
	Cursor string `json:"cursor"`
	Done   bool   `json:"done"`
}

func loadCheckpoint(path, filter string) (checkpoint, error) {
	c := checkpoint{Filter: filter, Cursor: "*"}
	b, err := os.ReadFile(path)
	if os.IsNotExist(err) {
		return c, nil
	}
	if err != nil {
		return c, err
	}
	if err := json.Unmarshal(b, &c); err != nil {
		return c, err
	}
	if c.Filter != filter {
		return c, fmt.Errorf("checkpoint filter differs from current filter; use another cursor file")
	}
	return c, nil
}

func saveCheckpoint(path string, c checkpoint) error {
	if err := os.MkdirAll(filepath.Dir(path), 0755); err != nil {
		return err
	}
	b, err := json.MarshalIndent(c, "", "  ")
	if err != nil {
		return err
	}
	tmp := path + ".tmp"
	if err := os.WriteFile(tmp, b, 0600); err != nil {
		return err
	}
	return os.Rename(tmp, path)
}

func fetchWorks(ctx context.Context, client *http.Client, baseURL, apiKey, filter, cursor string) (alexPage, error) {
	var page alexPage
	u, err := url.Parse(strings.TrimRight(baseURL, "/") + "/works")
	if err != nil {
		return page, err
	}
	q := u.Query()
	q.Set("filter", filter)
	q.Set("select", "id,title,doi,publication_year,authorships")
	q.Set("per_page", "100")
	q.Set("cursor", cursor)
	u.RawQuery = q.Encode()
	for attempt := 0; attempt < 6; attempt++ {
		req, err := http.NewRequestWithContext(ctx, http.MethodGet, u.String(), nil)
		if err != nil {
			return page, err
		}
		if apiKey != "" {
			req.Header.Set("Authorization", "Bearer "+apiKey)
		}
		resp, err := client.Do(req)
		if err == nil {
			if resp.StatusCode == http.StatusOK {
				defer resp.Body.Close()
				if err := json.NewDecoder(io.LimitReader(resp.Body, 32<<20)).Decode(&page); err != nil {
					return page, err
				}
				return page, nil
			}
			body, _ := io.ReadAll(io.LimitReader(resp.Body, 1024))
			resp.Body.Close()
			if resp.StatusCode != 429 && resp.StatusCode < 500 {
				return page, fmt.Errorf("OpenAlex HTTP %d: %s", resp.StatusCode, strings.TrimSpace(string(body)))
			}
			if attempt == 5 {
				return page, fmt.Errorf("OpenAlex HTTP %d after retries: %s", resp.StatusCode, strings.TrimSpace(string(body)))
			}
			wait := time.Duration(1<<attempt) * time.Second
			if retry, e := strconv.Atoi(resp.Header.Get("Retry-After")); e == nil && retry > 0 && retry < 300 {
				wait = time.Duration(retry) * time.Second
			}
			if err := waitContext(ctx, wait); err != nil {
				return page, err
			}
			continue
		}
		if attempt == 5 {
			return page, err
		}
		if err := waitContext(ctx, time.Duration(1<<attempt)*time.Second); err != nil {
			return page, err
		}
	}
	return page, fmt.Errorf("OpenAlex retries exhausted")
}

func waitContext(ctx context.Context, d time.Duration) error {
	t := time.NewTimer(d)
	defer t.Stop()
	select {
	case <-ctx.Done():
		return ctx.Err()
	case <-t.C:
		return nil
	}
}

func ingest(ctx context.Context, db *neo4j, client *http.Client, baseURL, key, cursorFile, filter, universityID string, maxPages int) error {
	cp, err := loadCheckpoint(cursorFile, filter)
	if err != nil {
		return err
	}
	if cp.Done {
		fmt.Println("OpenAlex import already complete")
		return nil
	}
	for pageNo := 0; maxPages == 0 || pageNo < maxPages; pageNo++ {
		page, err := fetchWorks(ctx, client, baseURL, key, filter, cp.Cursor)
		if err != nil {
			return err
		}
		rows := make([]workRow, 0, len(page.Results))
		pairs := 0
		for _, w := range page.Results {
			if row, ok := universityWork(w, universityID); ok {
				rows = append(rows, row)
				pairs += len(row.Authors) * (len(row.Authors) - 1) / 2
			}
		}
		if len(rows) > 0 {
			if _, err := db.query(ctx, importCypher, map[string]any{"rows": rows, "universityId": universityID}); err != nil {
				return err
			}
		}
		if page.Meta.NextCursor == nil || *page.Meta.NextCursor == "" || len(page.Results) == 0 {
			cp.Done = true
		} else {
			cp.Cursor = *page.Meta.NextCursor
		}
		if err := saveCheckpoint(cursorFile, cp); err != nil {
			return err
		}
		fmt.Printf("page %d: %d works, %d with selected-university authors, %d coauthor pairs on this page; total matching works: %d\n", pageNo+1, len(page.Results), len(rows), pairs, page.Meta.Count)
		if cp.Done {
			return nil
		}
	}
	return nil
}
