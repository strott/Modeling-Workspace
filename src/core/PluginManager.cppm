module;

#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include <stdexcept>
#include <algorithm>
#include <nlohmann/json.hpp>

export module RE.Core.PluginManager;

import RE.Core.IPlugin;
import RE.Core.SimulationContext;

export namespace RE::Core {

using json = nlohmann::json;

/**
 * Manages plugin lifecycle and dependency injection
 *
 * The PluginManager handles:
 * - Plugin registration
 * - Dependency resolution and injection
 * - Initialization order based on dependencies
 * - Update order execution
 * - Plugin lookup and access
 */
class PluginManager {
public:
    PluginManager() = default;
    ~PluginManager() = default;

    // Non-copyable
    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;

    /**
     * Register a plugin with optional configuration
     * Plugins are not initialized until initializeAll() is called
     */
    void registerPlugin(std::shared_ptr<IPlugin> plugin, const json& config = json::object()) {
        auto metadata = plugin->getMetadata();

        if (plugins.find(metadata.name) != plugins.end()) {
            throw std::runtime_error("Plugin already registered: " + metadata.name);
        }

        PluginEntry entry;
        entry.plugin = plugin;
        entry.metadata = metadata;
        entry.config = config;
        entry.initialized = false;

        plugins[metadata.name] = entry;
    }

    /**
     * Initialize all plugins in dependency order
     * This performs topological sort based on dependencies and calls initialize()
     */
    void initializeAll(SimulationContext& context) {
        // Build initialization order via topological sort
        std::vector<std::string> initOrder = getInitializationOrder();

        // Initialize plugins in order, injecting dependencies
        for (const auto& name : initOrder) {
            auto& entry = plugins.at(name);

            // Inject dependencies
            for (const auto& depName : entry.metadata.dependencies) {
                if (plugins.find(depName) == plugins.end()) {
                    throw std::runtime_error("Missing dependency: " + depName +
                                           " required by " + name);
                }
                entry.plugin->injectDependency(depName, plugins.at(depName).plugin);
            }

            // Initialize
            entry.plugin->initialize(entry.config, context);
            entry.initialized = true;
        }
    }

    /**
     * Update all plugins in the order they were registered
     */
    void updateAll(double deltaTime, SimulationContext& context) {
        for (auto& [name, entry] : plugins) {
            if (entry.initialized) {
                entry.plugin->update(deltaTime, context);
            }
        }
    }

    /**
     * Shutdown all plugins in reverse initialization order
     */
    void shutdownAll() {
        std::vector<std::string> initOrder = getInitializationOrder();
        std::reverse(initOrder.begin(), initOrder.end());

        for (const auto& name : initOrder) {
            auto& entry = plugins.at(name);
            if (entry.initialized) {
                entry.plugin->shutdown();
                entry.initialized = false;
            }
        }
    }

    /**
     * Get a plugin by name
     */
    std::shared_ptr<IPlugin> getPlugin(const std::string& name) const {
        auto it = plugins.find(name);
        if (it != plugins.end()) {
            return it->second.plugin;
        }
        return nullptr;
    }

    /**
     * Get a typed plugin by name
     */
    template<typename T>
    std::shared_ptr<T> getPlugin(const std::string& name) const {
        return std::dynamic_pointer_cast<T>(getPlugin(name));
    }

    /**
     * Check if a plugin is registered
     */
    bool hasPlugin(const std::string& name) const {
        return plugins.find(name) != plugins.end();
    }

    /**
     * Get list of all registered plugin names
     */
    std::vector<std::string> getPluginNames() const {
        std::vector<std::string> names;
        names.reserve(plugins.size());
        for (const auto& [name, _] : plugins) {
            names.push_back(name);
        }
        return names;
    }

private:
    struct PluginEntry {
        std::shared_ptr<IPlugin> plugin;
        PluginMetadata metadata;
        json config;
        bool initialized;
    };

    std::unordered_map<std::string, PluginEntry> plugins;

    /**
     * Topological sort to determine initialization order
     * Throws if circular dependencies detected
     */
    std::vector<std::string> getInitializationOrder() const {
        std::vector<std::string> order;
        std::unordered_map<std::string, bool> visited;
        std::unordered_map<std::string, bool> recursionStack;

        // DFS-based topological sort
        for (const auto& [name, _] : plugins) {
            if (!visited[name]) {
                if (!topologicalSortUtil(name, visited, recursionStack, order)) {
                    throw std::runtime_error("Circular dependency detected involving: " + name);
                }
            }
        }

        return order;
    }

    bool topologicalSortUtil(const std::string& name,
                           std::unordered_map<std::string, bool>& visited,
                           std::unordered_map<std::string, bool>& recursionStack,
                           std::vector<std::string>& order) const {
        visited[name] = true;
        recursionStack[name] = true;

        // Visit all dependencies
        const auto& entry = plugins.at(name);
        for (const auto& depName : entry.metadata.dependencies) {
            if (!visited[depName]) {
                if (!topologicalSortUtil(depName, visited, recursionStack, order)) {
                    return false; // Circular dependency
                }
            } else if (recursionStack[depName]) {
                return false; // Circular dependency
            }
        }

        recursionStack[name] = false;
        order.push_back(name);
        return true;
    }
};

} // namespace RE::Core
