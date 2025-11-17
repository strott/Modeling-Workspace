module;

#include <string>
#include <memory>
#include <unordered_map>

export module RE.Core.SimulationContext;

export namespace RE::Core {

/**
 * Shared simulation context containing global state
 */
class SimulationContext {
public:
    SimulationContext() = default;
    ~SimulationContext() = default;

    // Non-copyable
    SimulationContext(const SimulationContext&) = delete;
    SimulationContext& operator=(const SimulationContext&) = delete;

    // Movable
    SimulationContext(SimulationContext&&) = default;
    SimulationContext& operator=(SimulationContext&&) = default;

    // Simulation time
    double currentTime = 0.0;
    double deltaTime = 0.0;

    // Simulation parameters
    bool useCuda = true;
    int cudaDeviceId = 0;

    // Generic property storage for inter-plugin communication
    template<typename T>
    void setProperty(const std::string& key, const T& value) {
        properties[key] = std::make_shared<PropertyHolder<T>>(value);
    }

    template<typename T>
    T getProperty(const std::string& key, const T& defaultValue = T{}) const {
        auto it = properties.find(key);
        if (it != properties.end()) {
            auto holder = std::dynamic_pointer_cast<PropertyHolder<T>>(it->second);
            if (holder) {
                return holder->value;
            }
        }
        return defaultValue;
    }

    bool hasProperty(const std::string& key) const {
        return properties.find(key) != properties.end();
    }

private:
    struct IPropertyHolder {
        virtual ~IPropertyHolder() = default;
    };

    template<typename T>
    struct PropertyHolder : IPropertyHolder {
        explicit PropertyHolder(const T& val) : value(val) {}
        T value;
    };

    std::unordered_map<std::string, std::shared_ptr<IPropertyHolder>> properties;
};

} // namespace RE::Core
