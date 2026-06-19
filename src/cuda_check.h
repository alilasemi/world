#pragma once

#include <cstdio>
#include <cstdlib>
#include <cuda_runtime.h>

inline void cuda_check(cudaError_t err, const char* file, int line, const char* expr) {
    if (err != cudaSuccess) {
        fprintf(stderr, "CUDA error at %s:%d: %s failed: %s\n", file, line, expr, cudaGetErrorString(err));
        abort();
    }
}

#define CUDA_CHECK(expr) cuda_check((expr), __FILE__, __LINE__, #expr)
