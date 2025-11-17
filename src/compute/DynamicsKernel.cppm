module;

#include <cuda_runtime.h>
#include <memory>
#include <vector>

export module RE.Compute.DynamicsKernel;

import RE.Compute.CudaContext;

export namespace RE::Compute {

/**
 * Base class for CUDA-accelerated dynamics kernels
 *
 * Plugins that need to run dynamics equations on GPU should derive from this
 * and implement the computeDynamics() method.
 */
class DynamicsKernel {
public:
    explicit DynamicsKernel(std::shared_ptr<CudaContext> ctx)
        : cudaContext(ctx) {}

    virtual ~DynamicsKernel() = default;

    /**
     * Compute dynamics for a single time step
     * This method should launch CUDA kernels as needed
     */
    virtual void computeDynamics(double deltaTime) = 0;

    /**
     * Get the CUDA context
     */
    CudaContext& getContext() { return *cudaContext; }
    const CudaContext& getContext() const { return *cudaContext; }

protected:
    std::shared_ptr<CudaContext> cudaContext;
};

// Example: Simple state vector dynamics (position, velocity integration)
// This demonstrates how to structure a CUDA kernel

/**
 * State vector for basic dynamics
 */
struct StateVector {
    double position[3];  // x, y, z
    double velocity[3];  // vx, vy, vz
    double acceleration[3]; // ax, ay, az
};

/**
 * CUDA kernel for simple Euler integration
 */
__global__ void eulerIntegrationKernel(StateVector* states, int count, double deltaTime) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < count) {
        StateVector& state = states[idx];

        // Velocity integration: v = v + a * dt
        state.velocity[0] += state.acceleration[0] * deltaTime;
        state.velocity[1] += state.acceleration[1] * deltaTime;
        state.velocity[2] += state.acceleration[2] * deltaTime;

        // Position integration: p = p + v * dt
        state.position[0] += state.velocity[0] * deltaTime;
        state.position[1] += state.velocity[1] * deltaTime;
        state.position[2] += state.velocity[2] * deltaTime;
    }
}

/**
 * Example dynamics kernel implementation using Euler integration
 */
class EulerDynamicsKernel : public DynamicsKernel {
public:
    explicit EulerDynamicsKernel(std::shared_ptr<CudaContext> ctx, size_t numStates)
        : DynamicsKernel(ctx)
        , stateBuffer(numStates)
        , numStates(numStates) {}

    void computeDynamics(double deltaTime) override {
        if (numStates == 0) {
            return;
        }

        // Launch kernel
        int blockSize = 256;
        int numBlocks = (numStates + blockSize - 1) / blockSize;

        eulerIntegrationKernel<<<numBlocks, blockSize>>>(
            stateBuffer.get(), numStates, deltaTime);

        // Check for kernel errors
        CUDA_CHECK(cudaGetLastError());
    }

    /**
     * Update state data (copy from host to device)
     */
    void setStates(const std::vector<StateVector>& states) {
        if (states.size() != numStates) {
            throw std::runtime_error("State count mismatch");
        }
        stateBuffer.copyToDevice(states.data(), states.size());
    }

    /**
     * Retrieve state data (copy from device to host)
     */
    void getStates(std::vector<StateVector>& states) const {
        states.resize(numStates);
        stateBuffer.copyFromDevice(states.data(), numStates);
    }

private:
    CudaBuffer<StateVector> stateBuffer;
    size_t numStates;
};

} // namespace RE::Compute
