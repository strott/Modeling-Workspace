module;

#include <string>
#include <memory>
#include <vector>
#include <cmath>
#include <algorithm>
#include <nlohmann/json.hpp>

export module RE.Plugins.DroneElectricPropulsion;

import RE.Core.IPlugin;
import RE.Core.SimulationContext;
import RE.Plugins.MassProperties;
import RE.Plugins.Battery;

export namespace RE::Plugins {

using json = nlohmann::json;

/**
 * Motor configuration for a single motor/propeller
 */
struct MotorConfig {
    double maxRpm;              // Maximum RPM
    double maxThrust;           // Maximum thrust at maxRpm (Newtons)
    double motorKv;             // Motor Kv rating (RPM/V)
    double propellerDiameter;   // Propeller diameter (meters)
    double motorEfficiency;     // Motor efficiency (0.0 to 1.0)
    double timeConstant;        // Motor response time constant (seconds)
    std::array<double, 3> position; // Position relative to CoG (meters)
    std::array<double, 3> thrustDirection; // Unit vector for thrust direction
};

/**
 * Motor state for a single motor/propeller
 */
struct MotorState {
    double currentRpm;          // Current RPM
    double commandedThrottle;   // Commanded throttle (0.0 to 1.0)
    double currentThrust;       // Current thrust (Newtons)
    double powerConsumption;    // Current power draw (Watts)
};

/**
 * Drone electric propulsion plugin
 *
 * Models electric motors and propellers for multirotor/fixed-wing drones.
 * Supports multiple motors with individual control.
 * Depends on Battery for power and MassProperties for mass tracking.
 *
 * Configuration:
 * {
 *   "numMotors": 4,
 *   "motorConfig": {
 *     "maxRpm": 8000.0,
 *     "maxThrust": 12.0,
 *     "motorKv": 920,
 *     "propellerDiameter": 0.254,
 *     "motorEfficiency": 0.85,
 *     "timeConstant": 0.1
 *   },
 *   "motorPositions": [
 *     [0.15, 0.15, 0.0],   // Front right
 *     [-0.15, 0.15, 0.0],  // Front left
 *     [-0.15, -0.15, 0.0], // Rear left
 *     [0.15, -0.15, 0.0]   // Rear right
 *   ]
 * }
 */
class DroneElectricPropulsion : public RE::Core::IPlugin {
public:
    DroneElectricPropulsion()
        : massPropsPlugin(nullptr)
        , batteryPlugin(nullptr)
        , numMotors(0) {}

    ~DroneElectricPropulsion() override = default;

    RE::Core::PluginMetadata getMetadata() const override {
        return {
            .name = "DroneElectricPropulsion",
            .version = "1.0.0",
            .description = "Electric motor/propeller propulsion for drones",
            .dependencies = {"MassProperties", "Battery"}
        };
    }

    void initialize(const json& config, RE::Core::SimulationContext& context) override {
        (void)context;

        if (!config.contains("numMotors")) {
            throw std::runtime_error("DroneElectricPropulsion: numMotors not specified");
        }

        numMotors = config["numMotors"].get<int>();

        if (numMotors <= 0) {
            throw std::runtime_error("DroneElectricPropulsion: numMotors must be > 0");
        }

        // Default motor configuration
        MotorConfig defaultConfig = {
            .maxRpm = 8000.0,
            .maxThrust = 12.0,
            .motorKv = 920.0,
            .propellerDiameter = 0.254,
            .motorEfficiency = 0.85,
            .timeConstant = 0.1,
            .position = {0.0, 0.0, 0.0},
            .thrustDirection = {0.0, 0.0, 1.0}
        };

        // Parse motor configuration
        if (config.contains("motorConfig")) {
            auto mc = config["motorConfig"];
            if (mc.contains("maxRpm")) {
                defaultConfig.maxRpm = mc["maxRpm"].get<double>();
            }
            if (mc.contains("maxThrust")) {
                defaultConfig.maxThrust = mc["maxThrust"].get<double>();
            }
            if (mc.contains("motorKv")) {
                defaultConfig.motorKv = mc["motorKv"].get<double>();
            }
            if (mc.contains("propellerDiameter")) {
                defaultConfig.propellerDiameter = mc["propellerDiameter"].get<double>();
            }
            if (mc.contains("motorEfficiency")) {
                defaultConfig.motorEfficiency = mc["motorEfficiency"].get<double>();
            }
            if (mc.contains("timeConstant")) {
                defaultConfig.timeConstant = mc["timeConstant"].get<double>();
            }
        }

        // Initialize motors
        motorConfigs.resize(numMotors, defaultConfig);
        motorStates.resize(numMotors, {0.0, 0.0, 0.0, 0.0});

        // Parse motor positions if provided
        if (config.contains("motorPositions")) {
            auto positions = config["motorPositions"];
            if (positions.is_array()) {
                for (size_t i = 0; i < positions.size() && i < motorConfigs.size(); ++i) {
                    if (positions[i].is_array() && positions[i].size() == 3) {
                        motorConfigs[i].position[0] = positions[i][0].get<double>();
                        motorConfigs[i].position[1] = positions[i][1].get<double>();
                        motorConfigs[i].position[2] = positions[i][2].get<double>();
                    }
                }
            }
        }

        // Parse thrust directions if provided
        if (config.contains("thrustDirections")) {
            auto directions = config["thrustDirections"];
            if (directions.is_array()) {
                for (size_t i = 0; i < directions.size() && i < motorConfigs.size(); ++i) {
                    if (directions[i].is_array() && directions[i].size() == 3) {
                        motorConfigs[i].thrustDirection[0] = directions[i][0].get<double>();
                        motorConfigs[i].thrustDirection[1] = directions[i][1].get<double>();
                        motorConfigs[i].thrustDirection[2] = directions[i][2].get<double>();

                        // Normalize
                        double norm = std::sqrt(
                            motorConfigs[i].thrustDirection[0] * motorConfigs[i].thrustDirection[0] +
                            motorConfigs[i].thrustDirection[1] * motorConfigs[i].thrustDirection[1] +
                            motorConfigs[i].thrustDirection[2] * motorConfigs[i].thrustDirection[2]
                        );
                        if (norm > 1e-6) {
                            motorConfigs[i].thrustDirection[0] /= norm;
                            motorConfigs[i].thrustDirection[1] /= norm;
                            motorConfigs[i].thrustDirection[2] /= norm;
                        }
                    }
                }
            }
        }
    }

    void update(double deltaTime, RE::Core::SimulationContext& context) override {
        double totalThrust = 0.0;
        double totalPower = 0.0;
        std::array<double, 3> totalThrustVector = {0.0, 0.0, 0.0};
        std::array<double, 3> totalTorque = {0.0, 0.0, 0.0};

        // Update each motor
        for (size_t i = 0; i < motorStates.size(); ++i) {
            auto& state = motorStates[i];
            const auto& config = motorConfigs[i];

            // Clamp throttle
            double throttle = std::clamp(state.commandedThrottle, 0.0, 1.0);

            // Check battery power availability
            double availablePower = 0.0;
            if (batteryPlugin && !batteryPlugin->isDepleted()) {
                // Target RPM based on throttle and battery voltage
                double voltage = batteryPlugin->getVoltage();
                double targetRpm = throttle * config.motorKv * voltage;
                targetRpm = std::min(targetRpm, config.maxRpm);

                // Motor dynamics - first order response
                // dRPM/dt = (target - current) / tau
                double rpmError = targetRpm - state.currentRpm;
                double rpmDelta = rpmError * (deltaTime / config.timeConstant);
                state.currentRpm = std::max(0.0, state.currentRpm + rpmDelta);

                // Thrust curve - quadratic with RPM (simplified)
                double rpmRatio = state.currentRpm / config.maxRpm;
                state.currentThrust = config.maxThrust * rpmRatio * rpmRatio;

                // Power consumption - simplified model
                // P = k * thrust * rpm
                double powerFactor = throttle * throttle * throttle; // Cubic with throttle
                double requestedPower = powerFactor * 200.0; // Scale factor (Watts)

                // Request power from battery
                availablePower = batteryPlugin->requestPower(requestedPower);
                state.powerConsumption = availablePower;

                // If insufficient power, reduce thrust proportionally
                if (requestedPower > 1e-6) {
                    double powerRatio = availablePower / requestedPower;
                    state.currentThrust *= powerRatio;
                    state.currentRpm *= std::sqrt(powerRatio); // RPM scales with sqrt(power)
                }
            } else {
                // No battery power - motors coast down
                state.currentRpm *= std::exp(-deltaTime / (2.0 * config.timeConstant));
                state.currentThrust = 0.0;
                state.powerConsumption = 0.0;
            }

            // Accumulate thrust vector
            totalThrustVector[0] += state.currentThrust * config.thrustDirection[0];
            totalThrustVector[1] += state.currentThrust * config.thrustDirection[1];
            totalThrustVector[2] += state.currentThrust * config.thrustDirection[2];

            // Calculate torque (thrust x position)
            totalTorque[0] += config.position[1] * state.currentThrust * config.thrustDirection[2] -
                             config.position[2] * state.currentThrust * config.thrustDirection[1];
            totalTorque[1] += config.position[2] * state.currentThrust * config.thrustDirection[0] -
                             config.position[0] * state.currentThrust * config.thrustDirection[2];
            totalTorque[2] += config.position[0] * state.currentThrust * config.thrustDirection[1] -
                             config.position[1] * state.currentThrust * config.thrustDirection[0];

            totalThrust += state.currentThrust;
            totalPower += state.powerConsumption;
        }

        // Store results in context
        context.setProperty("DroneElectricPropulsion.totalThrust", totalThrust);
        context.setProperty("DroneElectricPropulsion.totalPower", totalPower);
        context.setProperty("DroneElectricPropulsion.thrustVector", totalThrustVector);
        context.setProperty("DroneElectricPropulsion.torque", totalTorque);
    }

    json getState() const override {
        json state;
        state["numMotors"] = numMotors;

        json motorsJson = json::array();
        for (size_t i = 0; i < motorStates.size(); ++i) {
            json motorJson;
            motorJson["rpm"] = motorStates[i].currentRpm;
            motorJson["throttle"] = motorStates[i].commandedThrottle;
            motorJson["thrust"] = motorStates[i].currentThrust;
            motorJson["power"] = motorStates[i].powerConsumption;
            motorsJson.push_back(motorJson);
        }
        state["motors"] = motorsJson;

        return state;
    }

    void setState(const json& state) override {
        if (state.contains("motors") && state["motors"].is_array()) {
            auto motorsJson = state["motors"];
            for (size_t i = 0; i < motorsJson.size() && i < motorStates.size(); ++i) {
                if (motorsJson[i].contains("rpm")) {
                    motorStates[i].currentRpm = motorsJson[i]["rpm"].get<double>();
                }
                if (motorsJson[i].contains("throttle")) {
                    motorStates[i].commandedThrottle = motorsJson[i]["throttle"].get<double>();
                }
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
     * Set throttle for a specific motor (0.0 to 1.0)
     */
    void setMotorThrottle(int motorIndex, double throttle) {
        if (motorIndex >= 0 && motorIndex < static_cast<int>(motorStates.size())) {
            motorStates[motorIndex].commandedThrottle = throttle;
        }
    }

    /**
     * Set throttle for all motors (0.0 to 1.0)
     */
    void setAllMotorThrottle(double throttle) {
        for (auto& state : motorStates) {
            state.commandedThrottle = throttle;
        }
    }

    /**
     * Get current RPM for a motor
     */
    double getMotorRpm(int motorIndex) const {
        if (motorIndex >= 0 && motorIndex < static_cast<int>(motorStates.size())) {
            return motorStates[motorIndex].currentRpm;
        }
        return 0.0;
    }

    /**
     * Get current thrust for a motor
     */
    double getMotorThrust(int motorIndex) const {
        if (motorIndex >= 0 && motorIndex < static_cast<int>(motorStates.size())) {
            return motorStates[motorIndex].currentThrust;
        }
        return 0.0;
    }

    /**
     * Get total thrust from all motors
     */
    double getTotalThrust() const {
        double total = 0.0;
        for (const auto& state : motorStates) {
            total += state.currentThrust;
        }
        return total;
    }

    /**
     * Get number of motors
     */
    int getNumMotors() const {
        return numMotors;
    }

private:
    std::shared_ptr<MassProperties> massPropsPlugin;
    std::shared_ptr<Battery> batteryPlugin;

    int numMotors;
    std::vector<MotorConfig> motorConfigs;
    std::vector<MotorState> motorStates;
};

} // namespace RE::Plugins
