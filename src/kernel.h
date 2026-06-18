#pragma once
#include <cuda_runtime.h>

class Kernel {
public:
    float wall_clock_time = 0.f;

    Kernel(const int n_);

    // This is the interface for running the kernel
    void operator()();

    // Prevent copying and assignment. It's error prone since CPU code can copy
    // these device pointers and cause problems.
    Kernel(const Kernel&) = delete;
    Kernel& operator=(const Kernel&) = delete;

protected:
    // Sizing. By default, use this to launch one thread per particle, but
    // derived classes can override this if they want to use a different launch
    // configuration.
    const int n;

private:
    // Timing events
    cudaEvent_t start_event, stop_event;

    // This is the main function that derived classes must implement to call the
    // kernel
    virtual void call_kernel() = 0;

    // Timing functions
    void start_timer();
    void stop_timer();
};
