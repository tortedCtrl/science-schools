package main

import (
	"context"
	"flag"
	"fmt"
	"net/http"
	"os"
	"os/signal"
	"time"
)

func main() {
	if err := loadDotEnv(".env"); err != nil {
		fatal(err)
	}
	step := flag.String("step", "all", "ingest, project, cluster, stats, or all")
	university := flag.String("university", env("UNIVERSITY", "spbu"), "university name or OpenAlex institution ID (default: SPbU)")
	maxPages := flag.Int("max-pages", 0, "maximum OpenAlex pages per run; 0 means all")
	sinceYear := flag.Int("since-year", 0, "optional minimum publication year")
	cursorFile := flag.String("cursor-file", "", "durable OpenAlex cursor; default is per university")
	resolution := flag.Float64("resolution", 1, "Leiden modularity resolution")
	seed := flag.Uint64("seed", 42, "Leiden random seed")
	batch := flag.Int("batch", 100, "Neo4j author batch size")
	flag.Parse()
	if *maxPages < 0 || *sinceYear < 0 || *resolution <= 0 || *batch < 1 || *batch > 1000 {
		fatal(fmt.Errorf("invalid flags"))
	}
	if *step != "all" && *step != "ingest" && *step != "project" && *step != "cluster" && *step != "stats" {
		fatal(fmt.Errorf("unknown step %q", *step))
	}
	password := os.Getenv("NEO4J_PASSWORD")
	if password == "" {
		fatal(fmt.Errorf("NEO4J_PASSWORD is required"))
	}
	client := &http.Client{Timeout: 45 * time.Second}
	ctx, stop := signal.NotifyContext(context.Background(), os.Interrupt)
	defer stop()
	universityID, err := resolveUniversity(ctx, client, env("OPENALEX_URL", "https://api.openalex.org"), os.Getenv("OPENALEX_API_KEY"), *university)
	if err != nil {
		fatal(err)
	}
	if *cursorFile == "" {
		*cursorFile = fmt.Sprintf("data/openalex-%s-cursor.json", universityID)
	}
	fmt.Printf("selected university: %s (%s)\n", *university, universityID)
	db, err := newNeo4j(env("NEO4J_URL", "http://localhost:7474"),
		env("NEO4J_DATABASE", "neo4j"), env("NEO4J_USER", "neo4j"), password, client)
	if err != nil {
		fatal(err)
	}
	filter := "authorships.institutions.id:" + universityID + ",type:article"
	if *sinceYear > 0 {
		filter += fmt.Sprintf(",publication_year:>%d", *sinceYear-1)
	}
	if *step == "all" || *step == "ingest" {
		if err := db.schema(ctx); err != nil {
			fatal(err)
		}
		if err := ingest(ctx, db, client, env("OPENALEX_URL", "https://api.openalex.org"),
			os.Getenv("OPENALEX_API_KEY"), *cursorFile, filter, universityID, *maxPages); err != nil {
			fatal(err)
		}
	}
	if *step == "all" || *step == "project" {
		if err := db.project(ctx, universityID, *batch); err != nil {
			fatal(err)
		}
	}
	if *step == "all" || *step == "cluster" {
		ids, edges, err := db.graph(ctx, universityID, *batch)
		if err != nil {
			fatal(err)
		}
		if len(ids) == 0 {
			fatal(fmt.Errorf("no authors in Neo4j; run ingest first"))
		}
		if len(edges) == 0 {
			fatal(fmt.Errorf("no coauthor edges in Neo4j; run -step=stats, then -step=project or ingest more pages"))
		}
		membership, err := leiden(len(ids), edges, *resolution, *seed)
		if err != nil {
			fatal(err)
		}
		if err := db.saveSchools(ctx, universityID, ids, membership, *batch); err != nil {
			fatal(err)
		}
		communities := make(map[int64]bool)
		for _, c := range membership {
			communities[c] = true
		}
		fmt.Printf("saved graph for %s: %d authors, %d coauthor edges, %d Leiden schools\n", universityID, len(ids), len(edges), len(communities))
	}
	if *step == "stats" {
		if err := db.stats(ctx, universityID); err != nil {
			fatal(err)
		}
	}
}

func env(key, fallback string) string {
	if value := os.Getenv(key); value != "" {
		return value
	}
	return fallback
}

func fatal(err error) {
	fmt.Fprintln(os.Stderr, "error:", err)
	os.Exit(1)
}
