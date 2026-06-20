#include "kernel.h"
#include "cuda_check.h"

Kernel::Kernel(const int n_, const int threads_per_block_) : n(n_), threads_per_block(threads_per_block_) {
    CUDA_CHECK(cudaEventCreate(&start_event));
    CUDA_CHECK(cudaEventCreate(&stop_event));
}


Kernel::~Kernel() {
    cudaEventDestroy(start_event);
    cudaEventDestroy(stop_event);
}


void Kernel::start_timer() {
    CUDA_CHECK(cudaEventRecord(start_event));
}


void Kernel::stop_timer() {
    CUDA_CHECK(cudaEventRecord(stop_event));
    CUDA_CHECK(cudaEventSynchronize(stop_event));
    float time = 0.f;
    CUDA_CHECK(cudaEventElapsedTime(&time, start_event, stop_event));
    wall_clock_time_ += time;
}


void Kernel::operator()() {
    int blocks = (n + threads_per_block - 1) / threads_per_block;
    start_timer();
    call_kernel(blocks, threads_per_block);
    CUDA_CHECK(cudaGetLastError());
    CUDA_CHECK(cudaDeviceSynchronize());
    stop_timer();
}
