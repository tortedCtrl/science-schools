package main

import (
	"context"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"github.com/jackc/pgx/v5/pgxpool"
)

type OpenAlexResponse struct {
	Result []Work `json:"results"`
}

type Work struct {
	Title       string       `json:"title"`
	Authorships []Authorship `json:"authorships"`
}

type Authorship struct {
	Author       Author        `json:"author"`
	Institutions []Institution `json:"institutions"`
}

type Author struct {
	Name string `json:"display_name"`
}

type Institution struct {
	Name        string `json:"display_name"`
	CountryCode string `json:"country_code"`
}

func main() {
	ctx := context.Background()
	db, err := pgxpool.New(
		ctx,
		"postgres://science:science@localhost:5432/science_schools?sslmode=disable",
	)
	if err != nil {
		fmt.Println("Ошибка подключения:", err)
		return
	}
	defer db.Close()
	err = db.Ping(ctx)
	if err != nil {
		fmt.Println("PostgreSQL недоступен:", err)
		return
	}
	url := "https://api.openalex.org/works?filter=institutions.country_code:RU&per_page=100"
	resp, err := http.Get(url)
	if err != nil {
		fmt.Println("Ошибка запроса", err)
		return
	}
	if resp.StatusCode != http.StatusOK {
		fmt.Println("Сервер вернул ошибку:", resp.Status)
		return
	}
	defer resp.Body.Close()
	body, err := io.ReadAll(resp.Body)
	if err != nil {
		fmt.Println("Ошибка чтения", err)
		return
	}
	var data OpenAlexResponse
	err = json.Unmarshal(body, &data)
	if err != nil {
		fmt.Println("Ошибка", err)
		return
	}
	var coauthorGroups [][]Author
	for _, work := range data.Result {
		var stationAuthors []Author
		for _, authorship := range work.Authorships {
			for _, institution := range authorship.Institutions {
				if institution.CountryCode == "RU" {
					stationAuthors = append(stationAuthors, authorship.Author)
				}
			}
		}
		coauthorGroups = append(coauthorGroups, stationAuthors)
	}
	fmt.Print(coauthorGroups)
}
