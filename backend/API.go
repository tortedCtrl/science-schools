package main

import (
	"context"
	"encoding/json"
	"fmt"
	"io"
	"net"
	"net/http"
	"net/url"
	"os"

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

func requiredEnv(name string) (string, error) {
	value, ok := os.LookupEnv(name)
	if !ok || value == "" {
		return "", fmt.Errorf("required environment variable %s is not set", name)
	}
	return value, nil
}

func postgresURL() (string, error) {
	host, err := requiredEnv("POSTGRES_HOST")
	if err != nil {
		return "", err
	}
	port, err := requiredEnv("POSTGRES_PORT")
	if err != nil {
		return "", err
	}
	database, err := requiredEnv("POSTGRES_DB")
	if err != nil {
		return "", err
	}
	user, err := requiredEnv("POSTGRES_USER")
	if err != nil {
		return "", err
	}
	password, err := requiredEnv("POSTGRES_PASSWORD")
	if err != nil {
		return "", err
	}

	connectionURL := url.URL{
		Scheme: "postgres",
		User:   url.UserPassword(user, password),
		Host:   net.JoinHostPort(host, port),
		Path:   "/" + database,
	}
	return connectionURL.String(), nil
}

func main() {
	ctx := context.Background()
	connectionString, err := postgresURL()
	if err != nil {
		fmt.Println("Ошибка конфигурации:", err)
		return
	}
	db, err := pgxpool.New(ctx, connectionString)
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
