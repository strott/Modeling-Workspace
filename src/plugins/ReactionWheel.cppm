module;

#include <string>
#include <memory>
#include <cmath>
#include <nlohmann/json.hpp>

export module RE.Plugins.ReactionWheel;

import RE.Core.IPlugin;
import RE.Core.SimulationContext;
import RE.Plugins.MassProperties;

export namespace RE::Plugins {

using json = nlohmann::json;

/**
 * Reaction wheel plugin
 *
 * Models a reaction wheel for spacecraft attitude control.
 * Depends on MassProperties to apply torques correctly.
 *
 * Configuration:
 * {
 *   "wheelInertia": 0.05,        // kg*m^2
 *   "maxTorque": 0.1,            // N*m
 *   "maxSpeed": 6000.0,          // RPM
 *   "spinAxis": [0, 0, 1]        // unit vector
 * }
 */
class ReactionWheel : public RE::Core::IPlugin {
public:
    ReactionWheel()
        : wheelInertia(0.0)
        , maxTorque(0.0)
        , maxSpeed(0.0)
        , currentSpeed(0.0)
        , commandedTorque(0.0)
        , spinAxis{0.0, 0.0, 1.0}
        , massPropsPlugin(nullptr) {}

    ~ReactionWheel() override = default;

    RE::Core::PluginMetadata getMetadata() const override {
        return {
            .name = "ReactionWheel",
            .version = "1.0.0",
            .description = "Reaction wheel for spacecraft attitude control",
            .dependencies = {"MassProperties"}
        };
    }

    void initialize(const json& config, RE::Core::SimulationContext& context) override {
        (void)context;

        if (config.contains("wheelInertia")) {
            wheelInertia = config["wheelInertia"].get<double>();
        }

        if (config.contains("maxTorque")) {
            maxTorque = config["maxTorque"].get<double>();
        }

        if (config.contains("maxSpeed")) {
            maxSpeed = config["maxSpeed"].get<double>();
        }

        if (config.contains("spinAxis")) {
            auto axis = config["spinAxis"];
            if (axis.is_array() && axis.size() == 3) {
                spinAxis[0] = axis[0].get<double>();
                spinAxis[1] = axis[1].get<double>();
                spinAxis[2] = axis[2].get<double>();

                // Normalize
                double norm = std::sqrt(spinAxis[0]*spinAxis[0] +
                                      spinAxis[1]*spinAxis[1] +
                                      spinAxis[2]*spinAxis[2]);
                if (norm > 1e-6) {
                    spinAxis[0] /= norm;
                    spinAxis[1] /= norm;
                    spinAxis[2] /= norm;
                }
            }
        }
    }

    void update(double deltaTime, RE::Core::SimulationContext& context) override {
        (void)context;

        // Clamp commanded torque
        double actualTorque = commandedTorque;
        if (actualTorque > maxTorque) actualTorque = maxTorque;
        if (actualTorque < -maxTorque) actualTorque = -maxTorque;

        // Update wheel speed: omega_dot = torque / inertia
        if (wheelInertia > 1e-6) {
            double acceleration = actualTorque / wheelInertia;
            currentSpeed += acceleration * deltaTime;

            // Clamp speed
            if (currentSpeed > maxSpeed) currentSpeed = maxSpeed;
            if (currentSpeed < -maxSpeed) currentSpeed = -maxSpeed;
        }

        // Reaction torque on spacecraft (Newton's 3rd law)
        // This would be applied to the vehicle's angular momentum
        // For now, we just track it
        double reactionTorque = -actualTorque;

        // Store in context for other plugins to access
        context.setProperty("ReactionWheel.torque", reactionTorque);
        context.setProperty("ReactionWheel.speed", currentSpeed);
    }

    json getState() const override {
        json state;
        state["wheelInertia"] = wheelInertia;
        state["maxTorque"] = maxTorque;
        state["maxSpeed"] = maxSpeed;
        state["currentSpeed"] = currentSpeed;
        state["commandedTorque"] = commandedTorque;
        state["spinAxis"] = {spinAxis[0], spinAxis[1], spinAxis[2]};
        return state;
    }

    void setState(const json& state) override {
        if (state.contains("wheelInertia")) {
            wheelInertia = state["wheelInertia"].get<double>();
        }
        if (state.contains("maxTorque")) {
            maxTorque = state["maxTorque"].get<double>();
        }
        if (state.contains("maxSpeed")) {
            maxSpeed = state["maxSpeed"].get<double>();
        }
        if (state.contains("currentSpeed")) {
            currentSpeed = state["currentSpeed"].get<double>();
        }
        if (state.contains("commandedTorque")) {
            commandedTorque = state["commandedTorque"].get<double>();
        }
        if (state.contains("spinAxis")) {
            auto axis = state["spinAxis"];
            if (axis.is_array() && axis.size() == 3) {
                spinAxis[0] = axis[0].get<double>();
                spinAxis[1] = axis[1].get<double>();
                spinAxis[2] = axis[2].get<double>();
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
    void setCommandedTorque(double torque) { commandedTorque = torque; }
    double getCurrentSpeed() const { return currentSpeed; }
    double getWheelInertia() const { return wheelInertia; }

private:
    double wheelInertia;    // kg*m^2
    double maxTorque;       // N*m
    double maxSpeed;        // rad/s
    double currentSpeed;    // rad/s
    double commandedTorque; // N*m
    std::array<double, 3> spinAxis;

    std::shared_ptr<MassProperties> massPropsPlugin;
};

} // namespace RE::Plugins
