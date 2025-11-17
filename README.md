# Reign Spacecraft Plugin System

A modular, extensible plugin system for simulating spacecraft and vehicle dynamics in C++20, designed for the Reign simulation environment.

## Overview

Reign is a high-performance simulation framework that uses a plugin-based architecture to model complex vehicle systems including spacecraft, drones, rovers, and autonomous ground vehicles. The system supports CUDA-accelerated dynamics computations and provides a flexible dependency injection mechanism for plugin composition.

## Features

- **C++20 Modules**: Modern C++ with full module support
- **Plugin Architecture**: Extensible system with automatic dependency injection
- **CUDA Support**: GPU-accelerated dynamics equations
- **Vehicle Configurator**: Code-based and JSON-based configuration
- **Cross-Platform**: Linux, macOS, and Windows support
- **Comprehensive Testing**: Full test coverage with Google Test
- **Type-Safe**: Strong typing with clear interfaces

## Architecture

### Core Components

```
RE::Core
├── IPlugin              - Base plugin interface
├── PluginManager        - Lifecycle and dependency management
├── Vehicle              - Top-level vehicle configurator
└── SimulationContext    - Shared simulation state

RE::Compute
├── CudaContext          - CUDA device management
└── DynamicsKernel       - GPU dynamics computation base

RE::Plugins
├── MassProperties       - Mass, inertia, center of gravity
├── ReactionWheel        - Attitude control wheel (depends on MassProperties)
└── RocketPropulsion     - Thrust and fuel consumption (depends on MassProperties)
```

### Plugin System

Plugins follow a well-defined lifecycle:

1. **Construction** - Plugin object created
2. **Registration** - Added to PluginManager with configuration
3. **Dependency Injection** - Required dependencies automatically injected
4. **Initialization** - `initialize()` called in dependency order
5. **Update Loop** - `update(deltaTime)` called each simulation step
6. **Shutdown** - `shutdown()` called for cleanup

### Dependency Injection

Plugins can declare dependencies on other plugins:

```cpp
class ReactionWheel : public IPlugin {
    PluginMetadata getMetadata() const override {
        return {
            .name = "ReactionWheel",
            .dependencies = {"MassProperties"}
        };
    }

    void injectDependency(const std::string& name,
                         std::shared_ptr<IPlugin> plugin) override {
        if (name == "MassProperties") {
            massPropsPlugin = std::dynamic_pointer_cast<MassProperties>(plugin);
        }
    }
};
```

The PluginManager automatically:
- Resolves dependency graphs
- Detects circular dependencies
- Initializes plugins in the correct order

## Building

### Prerequisites

- CMake 3.28 or later
- C++20 compatible compiler (GCC 11+, Clang 15+, MSVC 2022+)
- CUDA Toolkit 11.0+ (optional, for GPU support)
- Git

### Quick Start

```bash
# Clone the repository
git clone <repository-url>
cd Modeling-Workspace

# Build in release mode
./build.sh

# Run the example simulation
./run.sh
```

### Build Options

```bash
# Debug build
./build.sh --debug

# Clean build
./build.sh --clean

# Verbose output
./build.sh --verbose

# Build and run tests
./build.sh --test
```

## Usage

### Code-Based Configuration

```cpp
#include <memory>
import RE.Core.Vehicle;
import RE.Plugins.MassProperties;
import RE.Plugins.ReactionWheel;
import RE.Plugins.RocketPropulsion;

int main() {
    // Create a spacecraft
    Vehicle spacecraft(VehicleType::Spacecraft, "MySat");

    // Configure mass properties
    auto massProps = std::make_shared<MassProperties>();
    nlohmann::json massConfig = {
        {"mass", 1500.0},
        {"centerOfGravity", {0.0, 0.0, 0.0}},
        {"inertiaTensor", {
            {100.0, 0.0, 0.0},
            {0.0, 150.0, 0.0},
            {0.0, 0.0, 120.0}
        }}
    };
    spacecraft.addPlugin(massProps, massConfig);

    // Add reaction wheel
    auto wheel = std::make_shared<ReactionWheel>();
    nlohmann::json wheelConfig = {
        {"wheelInertia", 0.05},
        {"maxTorque", 0.1},
        {"maxSpeed", 6000.0}
    };
    spacecraft.addPlugin(wheel, wheelConfig);

    // Add propulsion
    auto propulsion = std::make_shared<RocketPropulsion>();
    nlohmann::json propConfig = {
        {"maxThrust", 10000.0},
        {"specificImpulse", 300.0},
        {"fuelMass", 500.0}
    };
    spacecraft.addPlugin(propulsion, propConfig);

    // Initialize all plugins
    spacecraft.initialize();

    // Command thrust and torque
    propulsion->setThrustLevel(0.5);  // 50% thrust
    wheel->setCommandedTorque(0.05);  // Small torque

    // Run simulation
    double dt = 0.1;  // 100ms timestep
    for (int i = 0; i < 100; ++i) {
        spacecraft.update(dt);
    }

    // Save configuration
    spacecraft.saveToFile("spacecraft_config.json");

    // Cleanup
    spacecraft.shutdown();

    return 0;
}
```

### JSON-Based Configuration

```json
{
  "name": "MySpacecraft",
  "type": "Spacecraft",
  "simulation": {
    "useCuda": true,
    "cudaDeviceId": 0
  },
  "plugins": [
    {
      "type": "MassProperties",
      "config": {
        "mass": 1500.0,
        "centerOfGravity": [0.0, 0.0, 0.0],
        "inertiaTensor": [
          [100.0, 0.0, 0.0],
          [0.0, 150.0, 0.0],
          [0.0, 0.0, 120.0]
        ]
      }
    }
  ]
}
```

## Creating Custom Plugins

### 1. Define Your Plugin

```cpp
export module RE.Plugins.MyPlugin;

import RE.Core.IPlugin;
import RE.Core.SimulationContext;

export namespace RE::Plugins {

class MyPlugin : public RE::Core::IPlugin {
public:
    PluginMetadata getMetadata() const override {
        return {
            .name = "MyPlugin",
            .version = "1.0.0",
            .description = "My custom plugin",
            .dependencies = {"MassProperties"}  // Optional
        };
    }

    void initialize(const json& config, SimulationContext& context) override {
        // Initialize from config
    }

    void update(double deltaTime, SimulationContext& context) override {
        // Update plugin state each timestep
    }

    json getState() const override {
        // Return current state
        return json::object();
    }

    void setState(const json& state) override {
        // Restore state
    }

    void shutdown() override {
        // Cleanup
    }

    void injectDependency(const std::string& name,
                         std::shared_ptr<IPlugin> plugin) override {
        // Handle dependency injection
    }

private:
    // Your plugin state
};

} // namespace RE::Plugins
```

### 2. Add to CMakeLists.txt

```cmake
target_sources(reign_plugins
    PUBLIC
        FILE_SET CXX_MODULES FILES
            plugins/MyPlugin.cppm
)
```

### 3. Use Your Plugin

```cpp
auto myPlugin = std::make_shared<MyPlugin>();
vehicle.addPlugin(myPlugin, config);
```

## Testing

The project includes comprehensive unit and integration tests:

```bash
# Build and run all tests
./build.sh --test

# Or run tests manually
cd build
ctest --output-on-failure
```

### Test Coverage

- `test_simulation_context` - SimulationContext property storage
- `test_plugin_manager` - Plugin lifecycle and dependency injection
- `test_mass_properties` - MassProperties plugin
- `test_reaction_wheel` - ReactionWheel plugin
- `test_rocket_propulsion` - RocketPropulsion plugin
- `test_vehicle` - Vehicle configuration and management
- `test_integration` - End-to-end integration tests

## CUDA Support

The system includes CUDA support for GPU-accelerated dynamics:

```cpp
import RE.Compute.CudaContext;
import RE.Compute.DynamicsKernel;

// Initialize CUDA
auto cudaContext = std::make_shared<CudaContext>(0); // Device 0

// Create dynamics kernel
auto dynamics = std::make_shared<EulerDynamicsKernel>(cudaContext, numStates);

// Set initial states
std::vector<StateVector> states = /* ... */;
dynamics->setStates(states);

// Compute dynamics on GPU
dynamics->computeDynamics(deltaTime);

// Retrieve results
dynamics->getStates(states);
```

### Custom CUDA Kernels

Extend `DynamicsKernel` to implement custom GPU-accelerated dynamics:

```cpp
class MyDynamicsKernel : public DynamicsKernel {
public:
    void computeDynamics(double deltaTime) override {
        // Launch your CUDA kernels
        myKernel<<<blocks, threads>>>(deviceData, deltaTime);
        CUDA_CHECK(cudaGetLastError());
    }
};
```

## Coding Style

- **Variables**: camelCase (`massValue`, `deltaTime`)
- **Types**: UpperCamelCase (`MassProperties`, `VehicleType`)
- **Namespaces**: `RE::Core`, `RE::Plugins`, `RE::Compute`
- **Modules**: `RE.Core.Vehicle`, `RE.Plugins.MassProperties`

## Directory Structure

```
Modeling-Workspace/
├── CMakeLists.txt           # Top-level build configuration
├── build.sh                 # Build script
├── run.sh                   # Run script
├── README.md                # This file
├── src/
│   ├── CMakeLists.txt
│   ├── core/                # Core plugin system
│   │   ├── IPlugin.cppm
│   │   ├── PluginManager.cppm
│   │   ├── Vehicle.cppm
│   │   └── SimulationContext.cppm
│   ├── compute/             # CUDA integration
│   │   ├── CudaContext.cppm
│   │   └── DynamicsKernel.cppm
│   ├── plugins/             # Plugin implementations
│   │   ├── MassProperties.cppm
│   │   ├── ReactionWheel.cppm
│   │   └── RocketPropulsion.cppm
│   └── main.cpp             # Example application
└── tests/                   # Test suite
    ├── CMakeLists.txt
    ├── test_simulation_context.cpp
    ├── test_plugin_manager.cpp
    ├── test_mass_properties.cpp
    ├── test_reaction_wheel.cpp
    ├── test_rocket_propulsion.cpp
    ├── test_vehicle.cpp
    └── test_integration.cpp
```

## Planned Features

- [ ] Dynamic plugin loading (shared libraries)
- [ ] Plugin factory registry for JSON loading
- [ ] Fuel slosh modeling
- [ ] Flexible body dynamics
- [ ] Multi-vehicle simulations
- [ ] Real-time visualization
- [ ] Distributed simulation support

## Contributing

When contributing new plugins or features:

1. Follow the coding style guidelines
2. Add comprehensive tests (aim for 100% coverage)
3. Document your plugin interface and configuration
4. Update this README with new features

## License

[Specify your license here]

## Contact

[Your contact information]
