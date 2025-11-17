#include <iostream>
#include <memory>
#include <iomanip>

import RE.Core.Vehicle;
import RE.Core.SimulationContext;
import RE.Plugins.MassProperties;
import RE.Plugins.Battery;
import RE.Plugins.DroneElectricPropulsion;

using namespace RE::Core;
using namespace RE::Plugins;

int main() {
    std::cout << "=== Reign Drone Electric Propulsion Demo ===\n" << std::endl;

    try {
        // Create a quadcopter drone
        Vehicle drone(VehicleType::Drone, "Quadcopter");

        // Configure mass properties
        auto massProps = std::make_shared<MassProperties>();
        nlohmann::json massConfig = {
            {"mass", 2.5},  // 2.5 kg total (including battery)
            {"centerOfGravity", {0.0, 0.0, 0.0}},
            {"inertiaTensor", {
                {0.01, 0.0, 0.0},
                {0.0, 0.01, 0.0},
                {0.0, 0.0, 0.015}
            }}
        };
        drone.addPlugin(massProps, massConfig);

        // Configure battery - 6S LiPo, 5000mAh
        auto battery = std::make_shared<Battery>();
        nlohmann::json batteryConfig = {
            {"capacity", 111.0},        // 5000mAh * 22.2V = 111 Wh
            {"voltage", 22.2},          // 6S LiPo nominal voltage
            {"maxDischargeRate", 100.0}, // 100A continuous
            {"efficiency", 0.95}
        };
        drone.addPlugin(battery, batteryConfig);

        // Configure 4 motors (quadcopter configuration)
        auto propulsion = std::make_shared<DroneElectricPropulsion>();
        nlohmann::json propulsionConfig = {
            {"numMotors", 4},
            {"motorConfig", {
                {"maxRpm", 8000.0},
                {"maxThrust", 12.0},         // 12N per motor @ max
                {"motorKv", 920},
                {"propellerDiameter", 0.254}, // 10 inch props
                {"motorEfficiency", 0.85},
                {"timeConstant", 0.15}
            }},
            {"motorPositions", {
                {0.15, 0.15, 0.0},     // Front right
                {-0.15, 0.15, 0.0},    // Front left
                {-0.15, -0.15, 0.0},   // Rear left
                {0.15, -0.15, 0.0}     // Rear right
            }}
        };
        drone.addPlugin(propulsion, propulsionConfig);

        std::cout << "Initializing drone systems..." << std::endl;
        drone.initialize();
        std::cout << "Initialization complete!\n" << std::endl;

        // Simulation parameters
        const double timeStep = 0.01;  // 10ms timesteps (100Hz control loop)
        const double flightTime = 180.0; // 3 minutes flight
        const int numSteps = static_cast<int>(flightTime / timeStep);

        std::cout << "Starting flight simulation..." << std::endl;
        std::cout << "Time step: " << timeStep << "s (100Hz)" << std::endl;
        std::cout << "Flight time: " << flightTime << "s\n" << std::endl;

        // Flight phases
        double hoverThrottle = 0.55;  // ~55% throttle to hover

        std::cout << std::fixed << std::setprecision(2);
        std::cout << "Time(s) | Battery(%) | Throttle | Total Thrust(N) | Power(W) | Motor RPM" << std::endl;
        std::cout << "--------|------------|----------|-----------------|----------|----------" << std::endl;

        // Run simulation
        for (int step = 0; step < numSteps; ++step) {
            double currentTime = step * timeStep;

            // Simple flight profile
            if (currentTime < 10.0) {
                // Takeoff - higher throttle
                propulsion->setAllMotorThrottle(0.7);
            } else if (currentTime < 160.0) {
                // Hover
                propulsion->setAllMotorThrottle(hoverThrottle);
            } else if (currentTime < 170.0) {
                // Descent
                propulsion->setAllMotorThrottle(0.45);
            } else {
                // Landing
                propulsion->setAllMotorThrottle(0.3);
            }

            // Update simulation
            drone.update(timeStep);

            // Print status every second
            if (step % 100 == 0) {
                auto& ctx = drone.getContext();
                double soc = battery->getStateOfCharge() * 100.0;
                double throttle = hoverThrottle;
                double thrust = propulsion->getTotalThrust();
                double power = ctx.getProperty<double>("DroneElectricPropulsion.totalPower", 0.0);
                double rpm = propulsion->getMotorRpm(0);

                std::cout << std::setw(7) << currentTime << " | "
                         << std::setw(10) << soc << " | "
                         << std::setw(8) << throttle << " | "
                         << std::setw(15) << thrust << " | "
                         << std::setw(8) << power << " | "
                         << std::setw(9) << rpm << std::endl;
            }

            // Check for battery depletion
            if (battery->isDepleted()) {
                std::cout << "\n>>> WARNING: Battery depleted at t=" << currentTime << "s <<<" << std::endl;
                break;
            }
        }

        std::cout << "\nFlight complete!" << std::endl;

        // Print final statistics
        std::cout << "\n=== Flight Statistics ===" << std::endl;
        std::cout << "Final battery charge: " << (battery->getStateOfCharge() * 100.0) << "%" << std::endl;
        std::cout << "Energy consumed: " << (battery->getCapacity() - battery->getRemainingCharge()) << " Wh" << std::endl;

        double flightTimeActual = drone.getContext().currentTime;
        std::cout << "Flight time: " << flightTimeActual << " seconds (" << (flightTimeActual / 60.0) << " minutes)" << std::endl;

        // Average power calculation
        double energyUsed = battery->getCapacity() - battery->getRemainingCharge();
        double avgPower = (energyUsed / (flightTimeActual / 3600.0));
        std::cout << "Average power: " << avgPower << " W" << std::endl;

        // Save configuration
        std::cout << "\nSaving configuration to drone_config.json..." << std::endl;
        drone.saveToFile("drone_config.json");

        // Shutdown
        std::cout << "Shutting down..." << std::endl;
        drone.shutdown();

        std::cout << "\nDemo completed successfully!" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
