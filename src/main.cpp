#include <iostream>
#include <memory>
#include <iomanip>

import RE.Core.Vehicle;
import RE.Core.SimulationContext;
import RE.Plugins.MassProperties;
import RE.Plugins.ReactionWheel;
import RE.Plugins.RocketPropulsion;

using namespace RE::Core;
using namespace RE::Plugins;

int main() {
    std::cout << "=== Reign Spacecraft Plugin System Demo ===\n" << std::endl;

    try {
        // Create a spacecraft vehicle
        Vehicle spacecraft(VehicleType::Spacecraft, "TestSpacecraft");

        // Configure mass properties
        auto massProps = std::make_shared<MassProperties>();
        nlohmann::json massConfig = {
            {"mass", 1500.0},  // 1500 kg dry mass
            {"centerOfGravity", {0.0, 0.0, 0.0}},
            {"inertiaTensor", {
                {100.0, 0.0, 0.0},
                {0.0, 150.0, 0.0},
                {0.0, 0.0, 120.0}
            }}
        };
        spacecraft.addPlugin(massProps, massConfig);

        // Configure reaction wheel
        auto reactionWheel = std::make_shared<ReactionWheel>();
        nlohmann::json wheelConfig = {
            {"wheelInertia", 0.05},
            {"maxTorque", 0.1},
            {"maxSpeed", 6000.0},
            {"spinAxis", {0, 0, 1}}
        };
        spacecraft.addPlugin(reactionWheel, wheelConfig);

        // Configure rocket propulsion
        auto propulsion = std::make_shared<RocketPropulsion>();
        nlohmann::json propulsionConfig = {
            {"maxThrust", 10000.0},     // 10 kN
            {"specificImpulse", 300.0}, // 300 seconds
            {"fuelMass", 500.0},        // 500 kg fuel
            {"thrustDirection", {0, 0, 1}}
        };
        spacecraft.addPlugin(propulsion, propulsionConfig);

        std::cout << "Initializing spacecraft plugins..." << std::endl;
        spacecraft.initialize();
        std::cout << "Initialization complete!\n" << std::endl;

        // Simulation parameters
        const double timeStep = 0.1;  // 100ms timesteps
        const double totalTime = 10.0; // 10 seconds
        const int numSteps = static_cast<int>(totalTime / timeStep);

        std::cout << "Starting simulation..." << std::endl;
        std::cout << "Time step: " << timeStep << "s" << std::endl;
        std::cout << "Total time: " << totalTime << "s" << std::endl;
        std::cout << "Number of steps: " << numSteps << "\n" << std::endl;

        // Command 50% thrust for first 5 seconds
        propulsion->setThrustLevel(0.5);

        // Command small torque to reaction wheel
        reactionWheel->setCommandedTorque(0.05);

        std::cout << std::fixed << std::setprecision(2);
        std::cout << "Time(s) | Mass(kg) | Fuel(kg) | Thrust(%) | Wheel Speed(rad/s)" << std::endl;
        std::cout << "--------|----------|----------|-----------|-------------------" << std::endl;

        // Run simulation
        for (int step = 0; step < numSteps; ++step) {
            double currentTime = step * timeStep;

            // Change thrust at 5 seconds
            if (step == numSteps / 2) {
                propulsion->setThrustLevel(0.0);
                std::cout << "\n>>> Thrust cut-off at t=" << currentTime << "s <<<\n" << std::endl;
            }

            // Update simulation
            spacecraft.update(timeStep);

            // Print status every 1 second
            if (step % 10 == 0) {
                auto& ctx = spacecraft.getContext();
                double mass = massProps->getMass();
                double fuel = propulsion->getFuelMass();
                double thrustLevel = propulsion->getThrustLevel();
                double wheelSpeed = reactionWheel->getCurrentSpeed();

                std::cout << std::setw(7) << currentTime << " | "
                         << std::setw(8) << mass << " | "
                         << std::setw(8) << fuel << " | "
                         << std::setw(9) << (thrustLevel * 100.0) << " | "
                         << std::setw(17) << wheelSpeed << std::endl;
            }
        }

        std::cout << "\nSimulation complete!" << std::endl;

        // Print final state
        std::cout << "\n=== Final State ===" << std::endl;
        std::cout << "Total mass: " << massProps->getMass() << " kg" << std::endl;
        std::cout << "Fuel remaining: " << propulsion->getFuelMass() << " kg" << std::endl;
        std::cout << "Fuel used: " << (500.0 - propulsion->getFuelMass()) << " kg" << std::endl;
        std::cout << "Reaction wheel speed: " << reactionWheel->getCurrentSpeed() << " rad/s" << std::endl;

        // Save configuration
        std::cout << "\nSaving configuration to spacecraft_config.json..." << std::endl;
        spacecraft.saveToFile("spacecraft_config.json");

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
