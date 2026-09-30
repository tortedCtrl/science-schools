package main

import "testing"

func TestLeidenSeparatesDisconnectedGroups(t *testing.T) {
	edges := []edge{
		{0, 1, 1}, {1, 2, 1}, {0, 2, 1},
		{3, 4, 1}, {4, 5, 1}, {3, 5, 1},
	}
	groups, err := leiden(6, edges, 1, 42)
	if err != nil {
		t.Fatal(err)
	}
	if groups[0] != groups[1] || groups[1] != groups[2] ||
		groups[3] != groups[4] || groups[4] != groups[5] || groups[0] == groups[3] {
		t.Fatalf("unexpected Leiden partition: %v", groups)
	}
}

func TestLeidenRejectsInvalidEdges(t *testing.T) {
	if _, err := leiden(2, []edge{{0, 2, 1}}, 1, 42); err == nil {
		t.Fatal("expected error for out-of-range vertex")
	}
}

func TestLeidenIsolates(t *testing.T) {
	groups, err := leiden(3, nil, 1, 42)
	if err != nil || len(groups) != 3 || groups[0] == groups[1] || groups[1] == groups[2] {
		t.Fatalf("unexpected isolate partition: %v, %v", groups, err)
	}
}
