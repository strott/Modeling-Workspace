#include <iostream>
#include <memory>
#include <iomanip>

import RE.Core.Vehicle;
import RE.Core.SimulationContext;
import RE.Plugins.MassProperties;
import RE.Plugins.Battery;
import RE.Plugins.SpacecraftElectricPropulsion;

using namespace RE::Core;
using namespace RE::Plugins;

int main() {
    std::cout << "=== Reign Spacecraft Electric Propulsion Demo ===\n" << std::endl;

    try {
        // Create a small satellite with electric propulsion
        Vehicle spacecraft(VehicleType::Spacecraft, "SmallSat-EP");

        // Configure mass properties
        auto massProps = std::make_shared<MassProperties>();
        nlohmann::json massConfig = {
            {"mass", 150.0},  // 150 kg wet mass
            {"centerOfGravity", {0.0, 0.0, 0.0}},
            {"inertiaTensor", {
                {20.0, 0.0, 0.0},
                {0.0, 25.0, 0.0},
                {0.0, 0.0, 22.0}
            }}
        };
        spacecraft.addPlugin(massProps, massConfig);

        // Configure battery/power source
        // Solar panels + battery for LEO operations
        auto battery = std::make_shared<Battery>();
        nlohmann::json batteryConfig = {
            {"capacity", 50000.0},      // 50 kWh battery
            {"voltage", 100.0},          // 100V bus
            {"maxDischargeRate", 100.0}, // 100A = 10kW max
            {"efficiency", 0.92}
        };
        spacecraft.addPlugin(battery, batteryConfig);

        // Configure Hall effect thruster
        auto propulsion = std::make_shared<SpacecraftElectricPropulsion>();
        nlohmann::json propulsionConfig = {
            {"thrusterType", "HallEffect"},
            {"maxPower", 5000.0},        // 5 kW thruster
            {"specificImpulse", 2000.0}, // 2000s (much higher than chemical ~300s)
            {"maxThrust", 0.15},         // 150 mN (low thrust but efficient)
            {"efficiency", 0.65},        // 65% electrical to kinetic
            {"propellantMass", 20.0},    // 20 kg Xenon
            {"thrustDirection", {0, 0, 1}}
        };
        spacecraft.addPlugin(propulsion, propulsionConfig);

        std::cout << "Initializing spacecraft systems..." << std::endl;
        spacecraft.initialize();
        std::cout << "Initialization complete!\n" << std::endl;

        // Simulation parameters
        const double timeStep = 1.0;      // 1 second timesteps
        const double missionTime = 3600.0 * 10; // 10 hours burn
        const int numSteps = static_cast<int>(missionTime / timeStep);

        std::cout << "Starting orbit raising maneuver simulation..." << std::endl;
        std::cout << "Time step: " << timeStep << "s" << std::endl;
        std::cout << "Mission duration: " << (missionTime / 3600.0) << " hours\n" << std::endl;

        std::cout << std::fixed << std::setprecision(3);
        std::cout << "Time(h) | Mass(kg) | Xenon(kg) | Power(%) | Thrust(mN) | Power(kW) | Delta-V(m/s)" << std::endl;
        std::cout << "--------|----------|-----------|----------|------------|-----------|-------------" << std::endl;

        // Track delta-V
        double deltaV = 0.0;
        double initialMass = massProps->getMass();

        // Burn phases
        bool burningPhase = true;

        // Run simulation
        for (int step = 0; step < numSteps; ++step) {
            double currentTime = step * timeStep;
            double currentHours = currentTime / 3600.0;

            // Control strategy: burn until xenon depleted or battery low
            if (burningPhase) {
                if (propulsion->getPropellantMass() > 0.1 && battery->getStateOfCharge() > 0.1) {
                    propulsion->setPowerLevel(1.0); // Full power
                } else {
                    propulsion->setPowerLevel(0.0);
                    burningPhase = false;
                    std::cout << "\n>>> Burn complete at t=" << currentHours << "h <<<\n" << std::endl;
                }
            }

            // Update simulation
            spacecraft.update(timeStep);

            // Calculate delta-V using Tsiolkovsky equation incrementally
            // dV = Ve * ln(m0/m1) where Ve = Isp * g0
            double currentMass = massProps->getMass();
            if (currentMass < initialMass - 0.001) {
                const double g0 = 9.80665;
                double exhaustVelocity = propulsion->getSpecificImpulse() * g0;
                double massRatio = initialMass / currentMass;
                deltaV = exhaustVelocity * std::log(massRatio);
            }

            // Print status every 30 minutes
            if (step % 1800 == 0) {
                auto& ctx = spacecraft.getContext();
                double mass = massProps->getMass();
                double xenon = propulsion->getPropellantMass();
                double powerLevel = propulsion->getPowerLevel() * 100.0;
                double thrust = propulsion->getCurrentThrust() * 1000.0; // Convert to mN
                double power = propulsion->getCurrentPower() / 1000.0; // Convert to kW

                std::cout << std::setw(7) << currentHours << " | "
                         << std::setw(8) << mass << " | "
                         << std::setw(9) << xenon << " | "
                         << std::setw(8) << powerLevel << " | "
                         << std::setw(10) << thrust << " | "
                         << std::setw(9) << power << " | "
                         << std::setw(11) << deltaV << std::endl;
            }
        }

        std::cout << "\nMission complete!" << std::endl;

        // Print final statistics
        std::cout << "\n=== Mission Statistics ===" << std::endl;
        std::cout << "Initial mass: " << initialMass << " kg" << std::endl;
        std::cout << "Final mass: " << massProps->getMass() << " kg" << std::endl;
        std::cout << "Propellant used: " << (20.0 - propulsion->getPropellantMass()) << " kg" << std::endl;
        std::cout << "Total delta-V: " << deltaV << " m/s" << std::endl;
        std::cout << "Specific impulse: " << propulsion->getSpecificImpulse() << " seconds" << std::endl;

        // Compare to chemical propulsion
        const double chemicalIsp = 300.0; // Typical chemical ISP
        const double g0 = 9.80665;
        double massRatio = initialMass / massProps->getMass();
        double chemicalDeltaV = chemicalIsp * g0 * std::log(massRatio);

        std::cout << "\nComparison to chemical propulsion:" << std::endl;
        std::cout << "Chemical ISP: " << chemicalIsp << "s" << std::endl;
        std::cout << "Chemical delta-V (same propellant): " << chemicalDeltaV << " m/s" << std::endl;
        std::cout << "Electric advantage: " << ((deltaV / chemicalDeltaV) * 100.0 - 100.0) << "% more delta-V" << std::endl;

        std::cout << "\nBattery state of charge: " << (battery->getStateOfCharge() * 100.0) << "%" << std::endl;
        std::cout << "Energy consumed: " << (battery->getCapacity() - battery->getRemainingCharge()) << " Wh" << std::endl;

        // Save configuration
        std::cout << "\nSaving configuration to spacecraft_electric_config.json..." << std::endl;
        spacecraft.saveToFile("spacecraft_electric_config.json");

        // Shutdown
        std::cout << "Shutting down..." << std::endl;
        spacecraft.shutdown();

        std::cout << "\nDemo completed successfully!" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
