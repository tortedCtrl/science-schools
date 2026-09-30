package main

/*
#cgo pkg-config: igraph
#include <igraph.h>
#include <stdint.h>

static int run_leiden(int64_t n, int64_t m, const int64_t *from,
                      const int64_t *to, const double *weight,
                      int64_t *out, double resolution, uint64_t seed) {
    igraph_t graph;
    igraph_vector_int_t pairs, membership;
    igraph_vector_t weights;
    igraph_int_t count;
    igraph_real_t quality;
    int rc;

    igraph_set_error_handler(igraph_error_handler_ignore);
    igraph_setup();
    if ((rc = igraph_vector_int_init(&pairs, 2*m))) return rc;
    for (int64_t i = 0; i < m; i++) {
        VECTOR(pairs)[2*i] = from[i];
        VECTOR(pairs)[2*i+1] = to[i];
    }
    rc = igraph_create(&graph, &pairs, n, IGRAPH_UNDIRECTED);
    igraph_vector_int_destroy(&pairs);
    if (rc) return rc;
    if ((rc = igraph_vector_init(&weights, m))) { igraph_destroy(&graph); return rc; }
    for (int64_t i = 0; i < m; i++) VECTOR(weights)[i] = weight[i];
    if ((rc = igraph_vector_int_init(&membership, n))) {
        igraph_vector_destroy(&weights); igraph_destroy(&graph); return rc;
    }
    igraph_rng_seed(igraph_rng_default(), seed);
    rc = igraph_community_leiden_simple(&graph, &weights,
        IGRAPH_LEIDEN_OBJECTIVE_MODULARITY, resolution, 0.01,
        0, 2, &membership, &count, &quality);
    if (!rc) for (int64_t i = 0; i < n; i++) out[i] = VECTOR(membership)[i];
    igraph_vector_int_destroy(&membership);
    igraph_vector_destroy(&weights);
    igraph_destroy(&graph);
    return rc;
}
*/
import "C"

import (
	"fmt"
	"unsafe"
)

func leiden(n int, edges []edge, resolution float64, seed uint64) ([]int64, error) {
	if n < 0 || resolution <= 0 {
		return nil, fmt.Errorf("invalid graph size or resolution")
	}
	if n == 0 {
		return []int64{}, nil
	}
	if len(edges) == 0 {
		isolates := make([]int64, n)
		for i := range isolates {
			isolates[i] = int64(i)
		}
		return isolates, nil
	}
	from := make([]int64, len(edges))
	to := make([]int64, len(edges))
	weights := make([]float64, len(edges))
	for i, e := range edges {
		if e.From < 0 || e.To < 0 || e.From >= n || e.To >= n || e.From == e.To || e.Weight <= 0 {
			return nil, fmt.Errorf("invalid edge %d", i)
		}
		from[i], to[i], weights[i] = int64(e.From), int64(e.To), e.Weight
	}
	out := make([]int64, n)
	var fp, tp *C.int64_t
	var wp *C.double
	if len(edges) > 0 {
		fp = (*C.int64_t)(unsafe.Pointer(&from[0]))
		tp = (*C.int64_t)(unsafe.Pointer(&to[0]))
		wp = (*C.double)(unsafe.Pointer(&weights[0]))
	}
	rc := C.run_leiden(C.int64_t(n), C.int64_t(len(edges)), fp, tp, wp,
		(*C.int64_t)(unsafe.Pointer(&out[0])), C.double(resolution), C.uint64_t(seed))
	if rc != 0 {
		return nil, fmt.Errorf("igraph Leiden error %d", int(rc))
	}
	return out, nil
}
