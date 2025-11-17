module;

#include <string>
#include <memory>
#include <cmath>
#include <array>
#include <nlohmann/json.hpp>

export module RE.Plugins.RocketPropulsion;

import RE.Core.IPlugin;
import RE.Core.SimulationContext;
import RE.Plugins.MassProperties;

export namespace RE::Plugins {

using json = nlohmann::json;

/**
 * Rocket propulsion plugin
 *
 * Models a rocket engine with thrust and fuel consumption.
 * Depends on MassProperties to update vehicle mass as fuel is consumed.
 *
 * Configuration:
 * {
 *   "maxThrust": 100000.0,         // N
 *   "specificImpulse": 300.0,      // seconds
 *   "fuelMass": 500.0,             // kg
 *   "thrustDirection": [0, 0, 1]   // unit vector
 * }
 */
class RocketPropulsion : public RE::Core::IPlugin {
public:
    RocketPropulsion()
        : maxThrust(0.0)
        , specificImpulse(0.0)
        , fuelMass(0.0)
        , thrustLevel(0.0)
        , thrustDirection{0.0, 0.0, 1.0}
        , massPropsPlugin(nullptr) {}

    ~RocketPropulsion() override = default;

    RE::Core::PluginMetadata getMetadata() const override {
        return {
            .name = "RocketPropulsion",
            .version = "1.0.0",
            .description = "Rocket engine with thrust and fuel consumption",
            .dependencies = {"MassProperties"}
        };
    }

    void initialize(const json& config, RE::Core::SimulationContext& context) override {
        (void)context;

        if (config.contains("maxThrust")) {
            maxThrust = config["maxThrust"].get<double>();
        }

        if (config.contains("specificImpulse")) {
            specificImpulse = config["specificImpulse"].get<double>();
        }

        if (config.contains("fuelMass")) {
            fuelMass = config["fuelMass"].get<double>();
            initialFuelMass = fuelMass;
        }

        if (config.contains("thrustDirection")) {
            auto dir = config["thrustDirection"];
            if (dir.is_array() && dir.size() == 3) {
                thrustDirection[0] = dir[0].get<double>();
                thrustDirection[1] = dir[1].get<double>();
                thrustDirection[2] = dir[2].get<double>();

                // Normalize
                double norm = std::sqrt(thrustDirection[0]*thrustDirection[0] +
                                      thrustDirection[1]*thrustDirection[1] +
                                      thrustDirection[2]*thrustDirection[2]);
                if (norm > 1e-6) {
                    thrustDirection[0] /= norm;
                    thrustDirection[1] /= norm;
                    thrustDirection[2] /= norm;
                }
            }
        }
    }

    void update(double deltaTime, RE::Core::SimulationContext& context) override {
        (void)context;

        // Clamp thrust level to [0, 1]
        if (thrustLevel < 0.0) thrustLevel = 0.0;
        if (thrustLevel > 1.0) thrustLevel = 1.0;

        // Calculate actual thrust
        double actualThrust = maxThrust * thrustLevel;

        // Calculate fuel consumption
        // dm/dt = thrust / (Isp * g0)
        // where g0 = 9.80665 m/s^2 (standard gravity)
        const double g0 = 9.80665;
        double fuelConsumptionRate = 0.0;

        if (specificImpulse > 1e-6 && fuelMass > 1e-6) {
            fuelConsumptionRate = actualThrust / (specificImpulse * g0);
            double fuelConsumed = fuelConsumptionRate * deltaTime;

            if (fuelConsumed > fuelMass) {
                fuelConsumed = fuelMass;
                actualThrust = fuelMass * specificImpulse * g0 / deltaTime;
            }

            fuelMass -= fuelConsumed;

            // Update vehicle mass
            if (massPropsPlugin) {
                double currentMass = massPropsPlugin->getMass();
                massPropsPlugin->setMass(currentMass - fuelConsumed);
            }
        } else {
            actualThrust = 0.0;
        }

        // Store thrust vector in context
        std::array<double, 3> thrustVector = {
            actualThrust * thrustDirection[0],
            actualThrust * thrustDirection[1],
            actualThrust * thrustDirection[2]
        };

        context.setProperty("RocketPropulsion.thrust", actualThrust);
        context.setProperty("RocketPropulsion.fuelMass", fuelMass);
        context.setProperty("RocketPropulsion.fuelConsumptionRate", fuelConsumptionRate);
    }

    json getState() const override {
        json state;
        state["maxThrust"] = maxThrust;
        state["specificImpulse"] = specificImpulse;
        state["fuelMass"] = fuelMass;
        state["initialFuelMass"] = initialFuelMass;
        state["thrustLevel"] = thrustLevel;
        state["thrustDirection"] = {thrustDirection[0], thrustDirection[1], thrustDirection[2]};
        return state;
    }

    void setState(const json& state) override {
        if (state.contains("maxThrust")) {
            maxThrust = state["maxThrust"].get<double>();
        }
        if (state.contains("specificImpulse")) {
            specificImpulse = state["specificImpulse"].get<double>();
        }
        if (state.contains("fuelMass")) {
            fuelMass = state["fuelMass"].get<double>();
        }
        if (state.contains("initialFuelMass")) {
            initialFuelMass = state["initialFuelMass"].get<double>();
        }
        if (state.contains("thrustLevel")) {
            thrustLevel = state["thrustLevel"].get<double>();
        }
        if (state.contains("thrustDirection")) {
            auto dir = state["thrustDirection"];
            if (dir.is_array() && dir.size() == 3) {
                thrustDirection[0] = dir[0].get<double>();
                thrustDirection[1] = dir[1].get<double>();
                thrustDirection[2] = dir[2].get<double>();
            }
        }
    }

    void shutdown() override {
        massPropsPlugin = nullptr;
    }

    void injectDependency(const std::string& name, std::shared_ptr<RE::Core::IPlugin> plugin) override {
        if (name == "MassProperties") {
            massPropsPlugin = std::dynamic_pointer_cast<MassProperties>(plugin);
        }
    }

    // Control interface
    void setThrustLevel(double level) { thrustLevel = level; }
    double getThrustLevel() const { return thrustLevel; }
    double getFuelMass() const { return fuelMass; }
    double getFuelFraction() const {
        return initialFuelMass > 1e-6 ? fuelMass / initialFuelMass : 0.0;
    }

private:
    double maxThrust;        // N
    double specificImpulse;  // seconds
    double fuelMass;         // kg
    double initialFuelMass;  // kg
    double thrustLevel;      // 0.0 to 1.0
    std::array<double, 3> thrustDirection;

    std::shared_ptr<MassProperties> massPropsPlugin;
};

} // namespace RE::Plugins
