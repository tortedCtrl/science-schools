package main

import (
	"os"
	"path/filepath"
	"testing"
)

func TestLoadDotEnvPreservesExistingEnvironment(t *testing.T) {
	t.Setenv("NEO4J_URL", "http://existing:7474")
	const extra = "SCIENCE_SCHOOLS_DOTENV_TEST"
	_ = os.Unsetenv(extra)
	t.Cleanup(func() { _ = os.Unsetenv(extra) })
	path := filepath.Join(t.TempDir(), ".env")
	if err := os.WriteFile(path, []byte("# settings\nNEO4J_URL=http://from-file:7474\n"+extra+"='loaded'\n"), 0600); err != nil {
		t.Fatal(err)
	}
	if err := loadDotEnv(path); err != nil {
		t.Fatal(err)
	}
	if got := os.Getenv("NEO4J_URL"); got != "http://existing:7474" {
		t.Fatalf("existing setting was overwritten: %q", got)
	}
	if got := os.Getenv(extra); got != "loaded" {
		t.Fatalf("file setting was not loaded: %q", got)
	}
}
