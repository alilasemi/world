#pragma once

#include <cstddef>
#include <cuda_runtime.h>

#include "cuda_check.h"
#include "host_vector.h"

// RAII wrapper around a cudaMalloc'd device buffer of T. Move-only: copying
// would alias the same allocation across two objects, both of which would
// cudaFree it on destruction. Deliberately has no operator[] -- dereferencing
// device memory from host code is undefined behavior.
template <typename T>
class DeviceVector {
public:
    DeviceVector() noexcept : ptr_(nullptr), size_(0) {}

    explicit DeviceVector(size_t n) : ptr_(nullptr), size_(n) {
        if (n) {
            CUDA_CHECK(cudaMalloc(reinterpret_cast<void**>(&ptr_), n * sizeof(T)));
        }
    }

    ~DeviceVector() {
        if (ptr_) {
            cudaFree(ptr_);
        }
    }

    DeviceVector(const DeviceVector&) = delete;
    DeviceVector& operator=(const DeviceVector&) = delete;

    DeviceVector(DeviceVector&& other) noexcept : ptr_(other.ptr_), size_(other.size_) {
        other.ptr_ = nullptr;
        other.size_ = 0;
    }

    DeviceVector& operator=(DeviceVector&& other) noexcept {
        if (this != &other) {
            if (ptr_) {
                cudaFree(ptr_);
            }
            ptr_ = other.ptr_;
            size_ = other.size_;
            other.ptr_ = nullptr;
            other.size_ = 0;
        }
        return *this;
    }

    T* data() noexcept { return ptr_; }
    const T* data() const noexcept { return ptr_; }
    size_t size() const noexcept { return size_; }

    void copy_from_host(const HostVector<T>& host) {
        CUDA_CHECK(cudaMemcpy(ptr_, host.data(), host.size() * sizeof(T), cudaMemcpyHostToDevice));
    }

private:
    T* ptr_;
    size_t size_;
};

template <typename T>
inline void HostVector<T>::copy_from_device(const DeviceVector<T>& device) {
    CUDA_CHECK(cudaMemcpy(data(), device.data(), device.size() * sizeof(T), cudaMemcpyDeviceToHost));
}
