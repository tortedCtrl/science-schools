package main

import (
	"context"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"net/url"
	"regexp"
	"strings"
)

const defaultUniversityID = "I172901346" // St Petersburg University (SPbU)

var institutionID = regexp.MustCompile(`^I[0-9]+$`)

func shortOpenAlexID(id string) string {
	return strings.TrimPrefix(id, "https://openalex.org/")
}

func resolveUniversity(ctx context.Context, client *http.Client, baseURL, apiKey, choice string) (string, error) {
	choice = strings.TrimSpace(choice)
	switch strings.ToLower(choice) {
	case "", "spbu", "спбгу", "санкт-петербургский государственный университет":
		return defaultUniversityID, nil
	}
	if id := shortOpenAlexID(choice); institutionID.MatchString(id) {
		return id, nil
	}
	u, err := url.Parse(strings.TrimRight(baseURL, "/") + "/institutions")
	if err != nil {
		return "", err
	}
	q := u.Query()
	q.Set("search", choice)
	q.Set("filter", "country_code:RU,type:education")
	q.Set("select", "id,display_name,country_code,type")
	q.Set("per_page", "10")
	u.RawQuery = q.Encode()
	req, err := http.NewRequestWithContext(ctx, http.MethodGet, u.String(), nil)
	if err != nil {
		return "", err
	}
	if apiKey != "" {
		req.Header.Set("Authorization", "Bearer "+apiKey)
	}
	resp, err := client.Do(req)
	if err != nil {
		return "", err
	}
	defer resp.Body.Close()
	if resp.StatusCode != http.StatusOK {
		body, _ := io.ReadAll(io.LimitReader(resp.Body, 1024))
		return "", fmt.Errorf("OpenAlex institution search HTTP %d: %s", resp.StatusCode, strings.TrimSpace(string(body)))
	}
	var result struct {
		Results []institution `json:"results"`
	}
	if err := json.NewDecoder(io.LimitReader(resp.Body, 1<<20)).Decode(&result); err != nil {
		return "", err
	}
	if len(result.Results) == 0 {
		return "", fmt.Errorf("university %q not found in OpenAlex", choice)
	}
	for _, i := range result.Results {
		if strings.EqualFold(i.Name, choice) {
			return shortOpenAlexID(i.ID), nil
		}
	}
	if len(result.Results) == 1 {
		return shortOpenAlexID(result.Results[0].ID), nil
	}
	options := make([]string, 0, len(result.Results))
	for _, i := range result.Results {
		options = append(options, fmt.Sprintf("%s (%s)", i.Name, shortOpenAlexID(i.ID)))
	}
	return "", fmt.Errorf("university %q is ambiguous; choose an OpenAlex ID: %s", choice, strings.Join(options, ", "))
}
