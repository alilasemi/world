#pragma once
#include <cuda_runtime.h>

class Kernel {
public:
    explicit Kernel(const int n_, const int threads_per_block_ = 256);
    virtual ~Kernel();

    // This is the interface for running the kernel
    void operator()();

    float wall_clock_time() const { return wall_clock_time_; }

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
    const int threads_per_block;

    // Timing events
    cudaEvent_t start_event, stop_event;
    float wall_clock_time_ = 0.f;

    // This is the main function that derived classes must implement to call the
    // kernel. blocks/threads_per_block are computed once by Kernel::operator()
    // and handed down so every derived class shares the same launch-config logic.
    virtual void call_kernel(int blocks, int threads_per_block) = 0;

    // Timing functions
    void start_timer();
    void stop_timer();
};
