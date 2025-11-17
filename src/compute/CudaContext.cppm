module;

#include <cuda_runtime.h>
#include <stdexcept>
#include <string>
#include <memory>

export module RE.Compute.CudaContext;

export namespace RE::Compute {

/**
 * CUDA error checking macro
 */
#define CUDA_CHECK(call) \
    do { \
        cudaError_t error = call; \
        if (error != cudaSuccess) { \
            throw std::runtime_error(std::string("CUDA error: ") + \
                                   cudaGetErrorString(error) + \
                                   " at " + __FILE__ + ":" + std::to_string(__LINE__)); \
        } \
    } while(0)

/**
 * RAII wrapper for CUDA memory
 */
template<typename T>
class CudaBuffer {
public:
    CudaBuffer() : devicePtr(nullptr), size(0) {}

    explicit CudaBuffer(size_t count) : size(count) {
        if (count > 0) {
            CUDA_CHECK(cudaMalloc(&devicePtr, count * sizeof(T)));
        }
    }

    ~CudaBuffer() {
        if (devicePtr) {
            cudaFree(devicePtr);
        }
    }

    // Non-copyable
    CudaBuffer(const CudaBuffer&) = delete;
    CudaBuffer& operator=(const CudaBuffer&) = delete;

    // Movable
    CudaBuffer(CudaBuffer&& other) noexcept
        : devicePtr(other.devicePtr), size(other.size) {
        other.devicePtr = nullptr;
        other.size = 0;
    }

    CudaBuffer& operator=(CudaBuffer&& other) noexcept {
        if (this != &other) {
            if (devicePtr) {
                cudaFree(devicePtr);
            }
            devicePtr = other.devicePtr;
            size = other.size;
            other.devicePtr = nullptr;
            other.size = 0;
        }
        return *this;
    }

    void copyToDevice(const T* hostData, size_t count) {
        if (count > size) {
            throw std::runtime_error("copyToDevice: count exceeds buffer size");
        }
        CUDA_CHECK(cudaMemcpy(devicePtr, hostData, count * sizeof(T), cudaMemcpyHostToDevice));
    }

    void copyFromDevice(T* hostData, size_t count) const {
        if (count > size) {
            throw std::runtime_error("copyFromDevice: count exceeds buffer size");
        }
        CUDA_CHECK(cudaMemcpy(hostData, devicePtr, count * sizeof(T), cudaMemcpyDeviceToHost));
    }

    T* get() { return devicePtr; }
    const T* get() const { return devicePtr; }
    size_t getSize() const { return size; }

private:
    T* devicePtr;
    size_t size;
};

/**
 * Manages CUDA device context and operations
 */
class CudaContext {
public:
    CudaContext() : deviceId(-1), initialized(false) {}

    explicit CudaContext(int device) : deviceId(device), initialized(false) {
        initialize(device);
    }

    ~CudaContext() {
        shutdown();
    }

    // Non-copyable
    CudaContext(const CudaContext&) = delete;
    CudaContext& operator=(const CudaContext&) = delete;

    // Movable
    CudaContext(CudaContext&&) = default;
    CudaContext& operator=(CudaContext&&) = default;

    /**
     * Initialize CUDA context with specified device
     */
    void initialize(int device = 0) {
        if (initialized) {
            return;
        }

        int deviceCount = 0;
        CUDA_CHECK(cudaGetDeviceCount(&deviceCount));

        if (deviceCount == 0) {
            throw std::runtime_error("No CUDA devices available");
        }

        if (device < 0 || device >= deviceCount) {
            throw std::runtime_error("Invalid CUDA device ID: " + std::to_string(device));
        }

        deviceId = device;
        CUDA_CHECK(cudaSetDevice(deviceId));
        CUDA_CHECK(cudaGetDeviceProperties(&deviceProps, deviceId));

        initialized = true;
    }

    /**
     * Shutdown CUDA context
     */
    void shutdown() {
        if (initialized) {
            cudaDeviceReset();
            initialized = false;
        }
    }

    /**
     * Synchronize device (wait for all operations to complete)
     */
    void synchronize() const {
        if (initialized) {
            CUDA_CHECK(cudaDeviceSynchronize());
        }
    }

    /**
     * Get device ID
     */
    int getDeviceId() const { return deviceId; }

    /**
     * Get device properties
     */
    const cudaDeviceProp& getDeviceProperties() const { return deviceProps; }

    /**
     * Check if context is initialized
     */
    bool isInitialized() const { return initialized; }

    /**
     * Get device name
     */
    std::string getDeviceName() const {
        return initialized ? std::string(deviceProps.name) : "Not initialized";
    }

    /**
     * Get total device memory in bytes
     */
    size_t getTotalMemory() const {
        return initialized ? deviceProps.totalGlobalMem : 0;
    }

    /**
     * Get available device memory in bytes
     */
    size_t getAvailableMemory() const {
        if (!initialized) {
            return 0;
        }
        size_t free, total;
        CUDA_CHECK(cudaMemGetInfo(&free, &total));
        return free;
    }

private:
    int deviceId;
    bool initialized;
    cudaDeviceProp deviceProps;
};

} // namespace RE::Compute
