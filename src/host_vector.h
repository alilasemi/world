#pragma once

#include <cstddef>
#include <cstdlib>

template <typename T> class DeviceVector;

// RAII wrapper around a malloc'd host buffer of T. Move-only: copying would
// double-free the same buffer on destruction.
template <typename T>
class HostVector {
public:
    HostVector() noexcept : ptr_(nullptr), size_(0) {}

    explicit HostVector(size_t n)
        : ptr_(n ? static_cast<T*>(malloc(n * sizeof(T))) : nullptr), size_(n) {
        if (n && !ptr_) {
            abort();
        }
    }

    ~HostVector() { free(ptr_); }

    HostVector(const HostVector&) = delete;
    HostVector& operator=(const HostVector&) = delete;

    HostVector(HostVector&& other) noexcept : ptr_(other.ptr_), size_(other.size_) {
        other.ptr_ = nullptr;
        other.size_ = 0;
    }

    HostVector& operator=(HostVector&& other) noexcept {
        if (this != &other) {
            free(ptr_);
            ptr_ = other.ptr_;
            size_ = other.size_;
            other.ptr_ = nullptr;
            other.size_ = 0;
        }
        return *this;
    }

    T& operator[](size_t i) { return ptr_[i]; }
    const T& operator[](size_t i) const { return ptr_[i]; }

    T* data() noexcept { return ptr_; }
    const T* data() const noexcept { return ptr_; }
    size_t size() const noexcept { return size_; }

    // Defined in device_vector.h, once DeviceVector is fully declared.
    void copy_from_device(const DeviceVector<T>& device);

private:
    T* ptr_;
    size_t size_;
};
