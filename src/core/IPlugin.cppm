module;

#include <string>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>

export module RE.Core.IPlugin;

import RE.Core.SimulationContext;

export namespace RE::Core {

using json = nlohmann::json;

/**
 * Plugin metadata
 */
struct PluginMetadata {
    std::string name;
    std::string version;
    std::string description;
    std::vector<std::string> dependencies; // Names of plugins this depends on
};

/**
 * Base interface for all Reign plugins
 *
 * Plugins represent modular components of a vehicle simulation (e.g., mass properties,
 * reaction wheels, propulsion systems). They follow a lifecycle:
 * 1. Construction
 * 2. initialize() - Set up initial state, acquire resources
 * 3. update() - Called each simulation step to update state
 * 4. shutdown() - Clean up resources
 */
class IPlugin {
public:
    virtual ~IPlugin() = default;

    /**
     * Get plugin metadata (name, version, dependencies)
     */
    virtual PluginMetadata getMetadata() const = 0;

    /**
     * Initialize the plugin with configuration
     * Called once before simulation starts
     * Dependencies are guaranteed to be initialized first
     */
    virtual void initialize(const json& config, SimulationContext& context) = 0;

    /**
     * Update plugin state for a single simulation step
     * @param deltaTime Time step in seconds
     * @param context Shared simulation context
     */
    virtual void update(double deltaTime, SimulationContext& context) = 0;

    /**
     * Get current plugin state as JSON
     */
    virtual json getState() const = 0;

    /**
     * Set plugin state from JSON
     */
    virtual void setState(const json& state) = 0;

    /**
     * Shutdown and cleanup
     * Called once when simulation ends
     */
    virtual void shutdown() = 0;

    /**
     * Inject a dependency plugin
     * Called by PluginManager during initialization to satisfy dependencies
     */
    virtual void injectDependency(const std::string& name, std::shared_ptr<IPlugin> plugin) {
        (void)name;
        (void)plugin;
        // Default implementation does nothing; override if needed
    }
};

/**
 * Factory function type for creating plugins
 */
using PluginFactory = std::shared_ptr<IPlugin>(*)();

} // namespace RE::Core
