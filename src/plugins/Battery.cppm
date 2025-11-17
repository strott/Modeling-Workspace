module;

#include <string>
#include <memory>
#include <nlohmann/json.hpp>

export module RE.Plugins.Battery;

import RE.Core.IPlugin;
import RE.Core.SimulationContext;

export namespace RE::Plugins {

using json = nlohmann::json;

/**
 * Battery/Power source plugin
 *
 * Models electrical energy storage and power delivery.
 * Used by both drone and spacecraft electric propulsion systems.
 *
 * Configuration:
 * {
 *   "capacity": 5000.0,          // Watt-hours (Wh)
 *   "voltage": 48.0,              // Volts
 *   "maxDischargeRate": 100.0,    // Amps (C-rating * capacity)
 *   "maxChargeRate": 20.0,        // Amps
 *   "initialCharge": 5000.0,      // Wh (defaults to full)
 *   "efficiency": 0.95            // Discharge efficiency
 * }
 */
class Battery : public RE::Core::IPlugin {
public:
    Battery()
        : capacity(0.0)
        , voltage(0.0)
        , maxDischargeRate(0.0)
        , maxChargeRate(0.0)
        , currentCharge(0.0)
        , efficiency(0.95)
        , currentDraw(0.0)
        , totalEnergyConsumed(0.0) {}

    ~Battery() override = default;

    RE::Core::PluginMetadata getMetadata() const override {
        return {
            .name = "Battery",
            .version = "1.0.0",
            .description = "Battery/power source for electric systems",
            .dependencies = {}
        };
    }

    void initialize(const json& config, RE::Core::SimulationContext& context) override {
        (void)context;

        if (config.contains("capacity")) {
            capacity = config["capacity"].get<double>();
            currentCharge = capacity; // Default to full
        }

        if (config.contains("voltage")) {
            voltage = config["voltage"].get<double>();
        }

        if (config.contains("maxDischargeRate")) {
            maxDischargeRate = config["maxDischargeRate"].get<double>();
        }

        if (config.contains("maxChargeRate")) {
            maxChargeRate = config["maxChargeRate"].get<double>();
        }

        if (config.contains("initialCharge")) {
            currentCharge = config["initialCharge"].get<double>();
            if (currentCharge > capacity) {
                currentCharge = capacity;
            }
        }

        if (config.contains("efficiency")) {
            efficiency = config["efficiency"].get<double>();
            if (efficiency < 0.0) efficiency = 0.0;
            if (efficiency > 1.0) efficiency = 1.0;
        }
    }

    void update(double deltaTime, RE::Core::SimulationContext& context) override {
        // Power consumption is set by other plugins via requestPower()
        // Discharge energy based on current draw
        if (currentDraw > 0.0 && currentCharge > 0.0) {
            // Energy = Power * Time (Wh = W * h)
            double energyRequested = currentDraw * (deltaTime / 3600.0); // Convert seconds to hours

            // Apply efficiency loss
            double energyConsumed = energyRequested / efficiency;

            if (energyConsumed > currentCharge) {
                energyConsumed = currentCharge;
                currentCharge = 0.0;
            } else {
                currentCharge -= energyConsumed;
            }

            totalEnergyConsumed += energyConsumed;
        }

        // Store battery state in context for other plugins
        context.setProperty("Battery.charge", currentCharge);
        context.setProperty("Battery.stateOfCharge", getStateOfCharge());
        context.setProperty("Battery.powerAvailable", isPowerAvailable(currentDraw));

        // Reset current draw for next timestep
        currentDraw = 0.0;
    }

    json getState() const override {
        json state;
        state["capacity"] = capacity;
        state["voltage"] = voltage;
        state["maxDischargeRate"] = maxDischargeRate;
        state["maxChargeRate"] = maxChargeRate;
        state["currentCharge"] = currentCharge;
        state["efficiency"] = efficiency;
        state["totalEnergyConsumed"] = totalEnergyConsumed;
        state["stateOfCharge"] = getStateOfCharge();
        return state;
    }

    void setState(const json& state) override {
        if (state.contains("capacity")) {
            capacity = state["capacity"].get<double>();
        }
        if (state.contains("voltage")) {
            voltage = state["voltage"].get<double>();
        }
        if (state.contains("maxDischargeRate")) {
            maxDischargeRate = state["maxDischargeRate"].get<double>();
        }
        if (state.contains("maxChargeRate")) {
            maxChargeRate = state["maxChargeRate"].get<double>();
        }
        if (state.contains("currentCharge")) {
            currentCharge = state["currentCharge"].get<double>();
        }
        if (state.contains("efficiency")) {
            efficiency = state["efficiency"].get<double>();
        }
        if (state.contains("totalEnergyConsumed")) {
            totalEnergyConsumed = state["totalEnergyConsumed"].get<double>();
        }
    }

    void shutdown() override {
        // Nothing to clean up
    }

    // Power management interface

    /**
     * Request power draw (Watts)
     * Returns actual power available (may be less if battery depleted/limited)
     */
    double requestPower(double powerWatts) {
        // Check max discharge rate: P = V * I, so I = P / V
        double maxPower = maxDischargeRate * voltage;

        if (powerWatts > maxPower) {
            powerWatts = maxPower;
        }

        // Check if enough charge available
        if (currentCharge <= 0.0) {
            return 0.0;
        }

        currentDraw += powerWatts;
        return powerWatts;
    }

    /**
     * Check if requested power is available
     */
    bool isPowerAvailable(double powerWatts) const {
        if (currentCharge <= 0.0) return false;
        double maxPower = maxDischargeRate * voltage;
        return powerWatts <= maxPower;
    }

    /**
     * Get state of charge (0.0 to 1.0)
     */
    double getStateOfCharge() const {
        return capacity > 0.0 ? currentCharge / capacity : 0.0;
    }

    /**
     * Get remaining charge in Wh
     */
    double getRemainingCharge() const {
        return currentCharge;
    }

    /**
     * Get battery voltage
     */
    double getVoltage() const {
        return voltage;
    }

    /**
     * Get capacity in Wh
     */
    double getCapacity() const {
        return capacity;
    }

    /**
     * Check if battery is depleted
     */
    bool isDepleted() const {
        return currentCharge <= 0.0;
    }

private:
    double capacity;            // Wh
    double voltage;             // V
    double maxDischargeRate;    // A
    double maxChargeRate;       // A
    double currentCharge;       // Wh
    double efficiency;          // 0.0 to 1.0
    double currentDraw;         // W (accumulated during timestep)
    double totalEnergyConsumed; // Wh
};

} // namespace RE::Plugins
