package main

import "testing"

func TestUniversityWorkSelectsOnlyAuthorsAtChosenUniversity(t *testing.T) {
	w := work{ID: "W1", Authorships: []authorship{
		{Author: author{ID: "A1", Name: "Ivan Petrov", ORCID: "https://orcid.org/0000-0000-0000-0001"},
			Countries: []string{"RU"}, Institutions: []institution{{ID: "https://openalex.org/I172901346", Name: "SPbU", CountryCode: "RU"}}},
		{Author: author{ID: "A2", Name: "Jane Smith"}, Countries: []string{"RU"}, Institutions: []institution{{ID: "I2", CountryCode: "RU"}}},
		{Author: author{ID: "A1", Name: "Ivan Petrov"}, Countries: []string{"RU"}, Institutions: []institution{{ID: "I172901346", CountryCode: "RU"}}},
	}}
	row, ok := universityWork(w, defaultUniversityID)
	if !ok || len(row.Authors) != 1 || row.Authors[0].ID != "A1" || row.Authors[0].LastName != "Petrov" || len(row.Authors[0].Institutions) != 1 {
		t.Fatalf("unexpected university work row: %#v", row)
	}
}

func TestUniversityWorkRequiresMatchingInstitution(t *testing.T) {
	w := work{ID: "W1", Authorships: []authorship{{Author: author{ID: "A1"}, Countries: []string{"RU"}}}}
	row, ok := universityWork(w, defaultUniversityID)
	if ok || len(row.Authors) != 0 {
		t.Fatalf("unexpected row: %#v", row)
	}
}
