module;

#include <array>
#include <string>
#include <memory>
#include <nlohmann/json.hpp>

export module RE.Plugins.MassProperties;

import RE.Core.IPlugin;
import RE.Core.SimulationContext;

export namespace RE::Plugins {

using json = nlohmann::json;

/**
 * Mass properties plugin
 *
 * Tracks the mass, center of gravity, and inertia tensor of a vehicle.
 * This is a fundamental plugin that many other plugins depend on.
 *
 * Configuration:
 * {
 *   "mass": 1000.0,              // kg
 *   "centerOfGravity": [0, 0, 0], // meters
 *   "inertiaTensor": [            // kg*m^2
 *     [100, 0, 0],
 *     [0, 150, 0],
 *     [0, 0, 120]
 *   ]
 * }
 */
class MassProperties : public RE::Core::IPlugin {
public:
    MassProperties()
        : mass(0.0)
        , centerOfGravity{0.0, 0.0, 0.0}
        , inertiaTensor{{
            {0.0, 0.0, 0.0},
            {0.0, 0.0, 0.0},
            {0.0, 0.0, 0.0}
        }} {}

    ~MassProperties() override = default;

    RE::Core::PluginMetadata getMetadata() const override {
        return {
            .name = "MassProperties",
            .version = "1.0.0",
            .description = "Tracks vehicle mass, center of gravity, and inertia tensor",
            .dependencies = {}
        };
    }

    void initialize(const json& config, RE::Core::SimulationContext& context) override {
        (void)context; // Unused for now

        if (config.contains("mass")) {
            mass = config["mass"].get<double>();
        }

        if (config.contains("centerOfGravity")) {
            auto cog = config["centerOfGravity"];
            if (cog.is_array() && cog.size() == 3) {
                centerOfGravity[0] = cog[0].get<double>();
                centerOfGravity[1] = cog[1].get<double>();
                centerOfGravity[2] = cog[2].get<double>();
            }
        }

        if (config.contains("inertiaTensor")) {
            auto inertia = config["inertiaTensor"];
            if (inertia.is_array() && inertia.size() == 3) {
                for (size_t i = 0; i < 3; ++i) {
                    if (inertia[i].is_array() && inertia[i].size() == 3) {
                        for (size_t j = 0; j < 3; ++j) {
                            inertiaTensor[i][j] = inertia[i][j].get<double>();
                        }
                    }
                }
            }
        }
    }

    void update(double deltaTime, RE::Core::SimulationContext& context) override {
        (void)deltaTime;
        (void)context;
        // Mass properties typically don't change per timestep
        // unless fuel is being consumed (handled by propulsion plugin)
    }

    json getState() const override {
        json state;
        state["mass"] = mass;
        state["centerOfGravity"] = {centerOfGravity[0], centerOfGravity[1], centerOfGravity[2]};

        state["inertiaTensor"] = json::array();
        for (size_t i = 0; i < 3; ++i) {
            state["inertiaTensor"].push_back(
                json::array({inertiaTensor[i][0], inertiaTensor[i][1], inertiaTensor[i][2]})
            );
        }

        return state;
    }

    void setState(const json& state) override {
        if (state.contains("mass")) {
            mass = state["mass"].get<double>();
        }

        if (state.contains("centerOfGravity")) {
            auto cog = state["centerOfGravity"];
            if (cog.is_array() && cog.size() == 3) {
                centerOfGravity[0] = cog[0].get<double>();
                centerOfGravity[1] = cog[1].get<double>();
                centerOfGravity[2] = cog[2].get<double>();
            }
        }

        if (state.contains("inertiaTensor")) {
            auto inertia = state["inertiaTensor"];
            if (inertia.is_array() && inertia.size() == 3) {
                for (size_t i = 0; i < 3; ++i) {
                    if (inertia[i].is_array() && inertia[i].size() == 3) {
                        for (size_t j = 0; j < 3; ++j) {
                            inertiaTensor[i][j] = inertia[i][j].get<double>();
                        }
                    }
                }
            }
        }
    }

    void shutdown() override {
        // Nothing to clean up
    }

    // Accessors for other plugins
    double getMass() const { return mass; }
    void setMass(double m) { mass = m; }

    const std::array<double, 3>& getCenterOfGravity() const { return centerOfGravity; }
    void setCenterOfGravity(const std::array<double, 3>& cog) { centerOfGravity = cog; }

    const std::array<std::array<double, 3>, 3>& getInertiaTensor() const { return inertiaTensor; }
    void setInertiaTensor(const std::array<std::array<double, 3>, 3>& inertia) {
        inertiaTensor = inertia;
    }

private:
    double mass; // kg
    std::array<double, 3> centerOfGravity; // meters
    std::array<std::array<double, 3>, 3> inertiaTensor; // kg*m^2
};

} // namespace RE::Plugins
