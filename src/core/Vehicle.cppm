module;

#include <string>
#include <memory>
#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>

export module RE.Core.Vehicle;

import RE.Core.IPlugin;
import RE.Core.PluginManager;
import RE.Core.SimulationContext;

export namespace RE::Core {

using json = nlohmann::json;

/**
 * Vehicle types supported by the system
 */
enum class VehicleType {
    Spacecraft,
    Drone,
    Rover,
    Custom
};

/**
 * Vehicle configurator and top-level simulation manager
 *
 * The Vehicle class represents a complete simulated vehicle (spacecraft, drone, rover, etc.)
 * It manages a collection of plugins that model different subsystems and provides
 * both code-based and file-based configuration.
 */
class Vehicle {
public:
    explicit Vehicle(VehicleType type = VehicleType::Custom, const std::string& name = "Vehicle")
        : vehicleType(type)
        , vehicleName(name)
        , context(std::make_shared<SimulationContext>())
        , pluginManager(std::make_shared<PluginManager>()) {}

    ~Vehicle() = default;

    // Non-copyable
    Vehicle(const Vehicle&) = delete;
    Vehicle& operator=(const Vehicle&) = delete;

    // Movable
    Vehicle(Vehicle&&) = default;
    Vehicle& operator=(Vehicle&&) = default;

    /**
     * Add a plugin to this vehicle (code-based configuration)
     */
    void addPlugin(std::shared_ptr<IPlugin> plugin, const json& config = json::object()) {
        pluginManager->registerPlugin(plugin, config);
    }

    /**
     * Load vehicle configuration from JSON file
     * File format:
     * {
     *   "name": "MySpacecraft",
     *   "type": "Spacecraft",
     *   "simulation": {
     *     "useCuda": true,
     *     "cudaDeviceId": 0
     *   },
     *   "plugins": [
     *     {
     *       "type": "MassProperties",
     *       "config": { ... }
     *     },
     *     ...
     *   ]
     * }
     */
    void loadFromFile(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open configuration file: " + filename);
        }

        json config;
        file >> config;

        if (config.contains("name")) {
            vehicleName = config["name"].get<std::string>();
        }

        if (config.contains("simulation")) {
            auto simConfig = config["simulation"];
            if (simConfig.contains("useCuda")) {
                context->useCuda = simConfig["useCuda"].get<bool>();
            }
            if (simConfig.contains("cudaDeviceId")) {
                context->cudaDeviceId = simConfig["cudaDeviceId"].get<int>();
            }
        }

        // Note: Plugin loading from file requires a plugin factory registry
        // For now, this is a placeholder for future implementation
        if (config.contains("plugins")) {
            throw std::runtime_error("Plugin loading from file not yet implemented. "
                                   "Use addPlugin() API for now.");
        }
    }

    /**
     * Save vehicle configuration to JSON file
     */
    void saveToFile(const std::string& filename) const {
        json config;
        config["name"] = vehicleName;
        config["type"] = vehicleTypeToString(vehicleType);

        // Simulation settings
        config["simulation"]["useCuda"] = context->useCuda;
        config["simulation"]["cudaDeviceId"] = context->cudaDeviceId;

        // Plugin states
        auto pluginNames = pluginManager->getPluginNames();
        config["plugins"] = json::array();
        for (const auto& name : pluginNames) {
            auto plugin = pluginManager->getPlugin(name);
            json pluginConfig;
            pluginConfig["name"] = name;
            pluginConfig["state"] = plugin->getState();
            config["plugins"].push_back(pluginConfig);
        }

        std::ofstream file(filename);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file for writing: " + filename);
        }
        file << config.dump(2);
    }

    /**
     * Initialize all plugins and prepare for simulation
     */
    void initialize() {
        pluginManager->initializeAll(*context);
    }

    /**
     * Update simulation for one time step
     */
    void update(double deltaTime) {
        context->deltaTime = deltaTime;
        pluginManager->updateAll(deltaTime, *context);
        context->currentTime += deltaTime;
    }

    /**
     * Shutdown and cleanup
     */
    void shutdown() {
        pluginManager->shutdownAll();
    }

    /**
     * Get a plugin by name
     */
    std::shared_ptr<IPlugin> getPlugin(const std::string& name) const {
        return pluginManager->getPlugin(name);
    }

    /**
     * Get typed plugin
     */
    template<typename T>
    std::shared_ptr<T> getPlugin(const std::string& name) const {
        return pluginManager->getPlugin<T>(name);
    }

    /**
     * Get simulation context
     */
    SimulationContext& getContext() { return *context; }
    const SimulationContext& getContext() const { return *context; }

    /**
     * Get vehicle name
     */
    const std::string& getName() const { return vehicleName; }

    /**
     * Get vehicle type
     */
    VehicleType getType() const { return vehicleType; }

private:
    VehicleType vehicleType;
    std::string vehicleName;
    std::shared_ptr<SimulationContext> context;
    std::shared_ptr<PluginManager> pluginManager;

    static std::string vehicleTypeToString(VehicleType type) {
        switch (type) {
            case VehicleType::Spacecraft: return "Spacecraft";
            case VehicleType::Drone: return "Drone";
            case VehicleType::Rover: return "Rover";
            case VehicleType::Custom: return "Custom";
            default: return "Unknown";
        }
    }
};

} // namespace RE::Core
