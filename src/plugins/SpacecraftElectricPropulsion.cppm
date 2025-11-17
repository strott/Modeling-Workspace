module;

#include <string>
#include <memory>
#include <cmath>
#include <array>
#include <nlohmann/json.hpp>

export module RE.Plugins.SpacecraftElectricPropulsion;

import RE.Core.IPlugin;
import RE.Core.SimulationContext;
import RE.Plugins.MassProperties;
import RE.Plugins.Battery;

export namespace RE::Plugins {

using json = nlohmann::json;

/**
 * Thruster types for electric propulsion
 */
enum class ThrusterType {
    IonThruster,        // Gridded ion thruster (high ISP, low thrust)
    HallEffect,         // Hall effect thruster (medium ISP, medium thrust)
    Electrospray,       // Electrospray thruster (very low thrust, very high ISP)
    VASIMR             // Variable specific impulse (adjustable ISP/thrust)
};

/**
 * Spacecraft electric propulsion plugin
 *
 * Models electric thrusters for spacecraft (ion, Hall effect, etc.).
 * These have very high specific impulse but require significant electrical power.
 * Depends on Battery/PowerSource and MassProperties.
 *
 * Configuration:
 * {
 *   "thrusterType": "HallEffect",
 *   "maxPower": 5000.0,           // Watts
 *   "specificImpulse": 2000.0,    // seconds (much higher than chemical)
 *   "maxThrust": 0.1,             // Newtons (much lower than chemical)
 *   "efficiency": 0.65,           // Electrical to kinetic efficiency
 *   "propellantMass": 20.0,       // kg (Xenon, etc.)
 *   "thrustDirection": [0, 0, 1]
 * }
 */
class SpacecraftElectricPropulsion : public RE::Core::IPlugin {
public:
    SpacecraftElectricPropulsion()
        : thrusterType(ThrusterType::HallEffect)
        , maxPower(0.0)
        , specificImpulse(0.0)
        , maxThrust(0.0)
        , efficiency(0.65)
        , propellantMass(0.0)
        , initialPropellantMass(0.0)
        , powerLevel(0.0)
        , currentThrust(0.0)
        , currentPower(0.0)
        , thrustDirection{0.0, 0.0, 1.0}
        , massPropsPlugin(nullptr)
        , batteryPlugin(nullptr) {}

    ~SpacecraftElectricPropulsion() override = default;

    RE::Core::PluginMetadata getMetadata() const override {
        return {
            .name = "SpacecraftElectricPropulsion",
            .version = "1.0.0",
            .description = "Electric propulsion for spacecraft (ion, Hall effect thrusters)",
            .dependencies = {"MassProperties", "Battery"}
        };
    }

    void initialize(const json& config, RE::Core::SimulationContext& context) override {
        (void)context;

        // Parse thruster type
        if (config.contains("thrusterType")) {
            std::string typeStr = config["thrusterType"].get<std::string>();
            if (typeStr == "IonThruster") {
                thrusterType = ThrusterType::IonThruster;
            } else if (typeStr == "HallEffect") {
                thrusterType = ThrusterType::HallEffect;
            } else if (typeStr == "Electrospray") {
                thrusterType = ThrusterType::Electrospray;
            } else if (typeStr == "VASIMR") {
                thrusterType = ThrusterType::VASIMR;
            }
        }

        if (config.contains("maxPower")) {
            maxPower = config["maxPower"].get<double>();
        }

        if (config.contains("specificImpulse")) {
            specificImpulse = config["specificImpulse"].get<double>();
        }

        if (config.contains("maxThrust")) {
            maxThrust = config["maxThrust"].get<double>();
        }

        if (config.contains("efficiency")) {
            efficiency = config["efficiency"].get<double>();
        }

        if (config.contains("propellantMass")) {
            propellantMass = config["propellantMass"].get<double>();
            initialPropellantMass = propellantMass;
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
        // Clamp power level to [0, 1]
        if (powerLevel < 0.0) powerLevel = 0.0;
        if (powerLevel > 1.0) powerLevel = 1.0;

        // Request power from battery
        double requestedPower = maxPower * powerLevel;
        double availablePower = 0.0;

        if (batteryPlugin && !batteryPlugin->isDepleted()) {
            availablePower = batteryPlugin->requestPower(requestedPower);
        }

        // Calculate actual power ratio
        double actualPowerRatio = 0.0;
        if (requestedPower > 1e-6) {
            actualPowerRatio = availablePower / requestedPower;
        }

        // Calculate thrust based on available power
        // For electric thrusters: T = 2 * eta * P / (Isp * g0)
        // where eta is efficiency, P is power, Isp is specific impulse
        const double g0 = 9.80665; // Standard gravity

        currentPower = availablePower;
        double theoreticalThrust = maxThrust * powerLevel * actualPowerRatio;

        // Calculate actual thrust from power and efficiency
        if (specificImpulse > 1e-6 && propellantMass > 1e-6) {
            double exhaustVelocity = specificImpulse * g0;

            // Thrust-power relationship for electric thrusters
            // P = T * Ve / (2 * eta)
            // Therefore: T = 2 * eta * P / Ve
            double thrustFromPower = 2.0 * efficiency * currentPower / exhaustVelocity;

            // Use the minimum of theoretical max and power-limited thrust
            currentThrust = std::min(theoreticalThrust, thrustFromPower);

            // Calculate propellant consumption
            // mdot = T / (Isp * g0)
            double massFlowRate = currentThrust / (specificImpulse * g0);
            double propellantConsumed = massFlowRate * deltaTime;

            if (propellantConsumed > propellantMass) {
                propellantConsumed = propellantMass;
                currentThrust = propellantMass * specificImpulse * g0 / deltaTime;
            }

            propellantMass -= propellantConsumed;

            // Update vehicle mass
            if (massPropsPlugin) {
                double currentMass = massPropsPlugin->getMass();
                massPropsPlugin->setMass(currentMass - propellantConsumed);
            }
        } else {
            currentThrust = 0.0;
        }

        // Store state in context
        context.setProperty("SpacecraftElectricPropulsion.thrust", currentThrust);
        context.setProperty("SpacecraftElectricPropulsion.power", currentPower);
        context.setProperty("SpacecraftElectricPropulsion.propellantMass", propellantMass);
        context.setProperty("SpacecraftElectricPropulsion.specificImpulse", specificImpulse);

        // Store thrust vector
        std::array<double, 3> thrustVector = {
            currentThrust * thrustDirection[0],
            currentThrust * thrustDirection[1],
            currentThrust * thrustDirection[2]
        };
        context.setProperty("SpacecraftElectricPropulsion.thrustVector", thrustVector);
    }

    json getState() const override {
        json state;
        state["thrusterType"] = thrusterTypeToString(thrusterType);
        state["maxPower"] = maxPower;
        state["specificImpulse"] = specificImpulse;
        state["maxThrust"] = maxThrust;
        state["efficiency"] = efficiency;
        state["propellantMass"] = propellantMass;
        state["initialPropellantMass"] = initialPropellantMass;
        state["powerLevel"] = powerLevel;
        state["currentThrust"] = currentThrust;
        state["currentPower"] = currentPower;
        state["thrustDirection"] = {thrustDirection[0], thrustDirection[1], thrustDirection[2]};
        state["propellantFraction"] = getPropellantFraction();
        return state;
    }

    void setState(const json& state) override {
        if (state.contains("maxPower")) {
            maxPower = state["maxPower"].get<double>();
        }
        if (state.contains("specificImpulse")) {
            specificImpulse = state["specificImpulse"].get<double>();
        }
        if (state.contains("maxThrust")) {
            maxThrust = state["maxThrust"].get<double>();
        }
        if (state.contains("efficiency")) {
            efficiency = state["efficiency"].get<double>();
        }
        if (state.contains("propellantMass")) {
            propellantMass = state["propellantMass"].get<double>();
        }
        if (state.contains("initialPropellantMass")) {
            initialPropellantMass = state["initialPropellantMass"].get<double>();
        }
        if (state.contains("powerLevel")) {
            powerLevel = state["powerLevel"].get<double>();
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
        batteryPlugin = nullptr;
    }

    void injectDependency(const std::string& name, std::shared_ptr<RE::Core::IPlugin> plugin) override {
        if (name == "MassProperties") {
            massPropsPlugin = std::dynamic_pointer_cast<MassProperties>(plugin);
        } else if (name == "Battery") {
            batteryPlugin = std::dynamic_pointer_cast<Battery>(plugin);
        }
    }

    // Control interface

    /**
     * Set power level (0.0 to 1.0)
     */
    void setPowerLevel(double level) {
        powerLevel = level;
    }

    /**
     * Get current power level
     */
    double getPowerLevel() const {
        return powerLevel;
    }

    /**
     * Get current thrust (Newtons)
     */
    double getCurrentThrust() const {
        return currentThrust;
    }

    /**
     * Get current power consumption (Watts)
     */
    double getCurrentPower() const {
        return currentPower;
    }

    /**
     * Get remaining propellant mass (kg)
     */
    double getPropellantMass() const {
        return propellantMass;
    }

    /**
     * Get propellant fraction (0.0 to 1.0)
     */
    double getPropellantFraction() const {
        return initialPropellantMass > 1e-6 ? propellantMass / initialPropellantMass : 0.0;
    }

    /**
     * Get specific impulse (seconds)
     */
    double getSpecificImpulse() const {
        return specificImpulse;
    }

    /**
     * Get thruster type
     */
    ThrusterType getThrusterType() const {
        return thrusterType;
    }

private:
    ThrusterType thrusterType;
    double maxPower;            // W
    double specificImpulse;     // seconds (typically 1500-3000+ for electric)
    double maxThrust;           // N (typically 0.01-1.0 for electric)
    double efficiency;          // Electrical to kinetic efficiency
    double propellantMass;      // kg (Xenon, Argon, etc.)
    double initialPropellantMass;
    double powerLevel;          // 0.0 to 1.0
    double currentThrust;       // N
    double currentPower;        // W
    std::array<double, 3> thrustDirection;

    std::shared_ptr<MassProperties> massPropsPlugin;
    std::shared_ptr<Battery> batteryPlugin;

    static std::string thrusterTypeToString(ThrusterType type) {
        switch (type) {
            case ThrusterType::IonThruster: return "IonThruster";
            case ThrusterType::HallEffect: return "HallEffect";
            case ThrusterType::Electrospray: return "Electrospray";
            case ThrusterType::VASIMR: return "VASIMR";
            default: return "Unknown";
        }
    }
};

} // namespace RE::Plugins
