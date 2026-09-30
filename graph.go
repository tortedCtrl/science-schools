package main

import (
	"fmt"
	"sort"
	"strings"
)

type institution struct {
	ID          string `json:"id"`
	Name        string `json:"display_name"`
	CountryCode string `json:"country_code"`
	Type        string `json:"type"`
}

type author struct {
	ID    string `json:"id"`
	Name  string `json:"display_name"`
	ORCID string `json:"orcid"`
}

type authorship struct {
	Author       author        `json:"author"`
	Institutions []institution `json:"institutions"`
	Countries    []string      `json:"countries"`
}

type work struct {
	ID          string       `json:"id"`
	Title       string       `json:"title"`
	DOI         string       `json:"doi"`
	Year        int          `json:"publication_year"`
	Authorships []authorship `json:"authorships"`
}

type authorRow struct {
	ID           string        `json:"id"`
	Name         string        `json:"name"`
	FirstName    string        `json:"firstName"`
	LastName     string        `json:"lastName"`
	ORCID        string        `json:"orcid"`
	Institutions []institution `json:"institutions"`
}

type workRow struct {
	ID      string      `json:"id"`
	Title   string      `json:"title"`
	DOI     string      `json:"doi"`
	Year    int         `json:"year"`
	Authors []authorRow `json:"authors"`
}

// universityWork includes only authors affiliated with the selected Russian
// university on this work. Institution IDs are stable even when names vary.
func universityWork(w work, universityID string) (workRow, bool) {
	row := workRow{ID: w.ID, Title: w.Title, DOI: w.DOI, Year: w.Year, Authors: []authorRow{}}
	if w.ID == "" {
		return row, false
	}
	seen := make(map[string]bool)
	for _, byline := range w.Authorships {
		if byline.Author.ID == "" || seen[byline.Author.ID] || !atUniversity(byline, universityID) {
			continue
		}
		seen[byline.Author.ID] = true
		name := strings.TrimSpace(byline.Author.Name)
		parts := strings.Fields(name)
		first, last := "", ""
		if len(parts) >= 2 {
			first, last = parts[0], parts[len(parts)-1]
		}
		institutions := make([]institution, 0, len(byline.Institutions))
		for _, inst := range byline.Institutions {
			if inst.ID != "" {
				institutions = append(institutions, inst)
			}
		}
		row.Authors = append(row.Authors, authorRow{
			ID: byline.Author.ID, Name: name, FirstName: first, LastName: last,
			ORCID: byline.Author.ORCID, Institutions: institutions,
		})
	}
	return row, len(row.Authors) > 0
}

func atUniversity(a authorship, universityID string) bool {
	for _, i := range a.Institutions {
		if i.CountryCode == "RU" && shortOpenAlexID(i.ID) == universityID {
			return true
		}
	}
	return false
}

type edge struct {
	From   int
	To     int
	Weight float64
}

func sortedIDs(ids []string) ([]string, map[string]int, error) {
	sort.Strings(ids)
	index := make(map[string]int, len(ids))
	for i, id := range ids {
		if id == "" || (i > 0 && ids[i-1] == id) {
			return nil, nil, fmt.Errorf("invalid or duplicate author id %q", id)
		}
		index[id] = i
	}
	return ids, index, nil
}
