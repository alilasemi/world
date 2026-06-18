#include <stdio.h>
#include <assert.h>
#include "kernel.h"

Kernel::Kernel(const int n_) : n(n_) {
    cudaEventCreate(&start_event);
    cudaEventCreate(&stop_event);
}


void Kernel::start_timer() {
    cudaEventRecord(start_event);
}


void Kernel::stop_timer() {
    cudaEventRecord(stop_event);
    cudaEventSynchronize(stop_event);
    float time = 0.f;
    cudaEventElapsedTime(&time, start_event, stop_event);
    wall_clock_time += time;
}


void Kernel::operator()() {
    int threads_per_block = 256;
    int blocks = (n + threads_per_block - 1) / threads_per_block;
    call_kernel();
    cudaDeviceSynchronize();
}

