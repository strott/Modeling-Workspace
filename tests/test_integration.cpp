#include <gtest/gtest.h>
#include <memory>

import RE.Core.Vehicle;
import RE.Plugins.MassProperties;
import RE.Plugins.ReactionWheel;
import RE.Plugins.RocketPropulsion;

using namespace RE::Core;
using namespace RE::Plugins;

class IntegrationTest : public ::testing::Test {
protected:
    // Integration test setup
};

TEST_F(IntegrationTest, CompleteSpacecraftSimulation) {
    // Create a complete spacecraft with all plugins
    Vehicle spacecraft(VehicleType::Spacecraft, "IntegrationTestSat");

    // Add mass properties
    auto massProps = std::make_shared<MassProperties>();
    nlohmann::json massConfig = {
        {"mass", 1500.0},
        {"centerOfGravity", {0.0, 0.0, 0.0}},
        {"inertiaTensor", {
            {100.0, 0.0, 0.0},
            {0.0, 150.0, 0.0},
            {0.0, 0.0, 120.0}
        }}
    };
    spacecraft.addPlugin(massProps, massConfig);

    // Add reaction wheel
    auto wheel = std::make_shared<ReactionWheel>();
    nlohmann::json wheelConfig = {
        {"wheelInertia", 0.05},
        {"maxTorque", 0.1},
        {"maxSpeed", 6000.0}
    };
    spacecraft.addPlugin(wheel, wheelConfig);

    // Add propulsion
    auto propulsion = std::make_shared<RocketPropulsion>();
    nlohmann::json propulsionConfig = {
        {"maxThrust", 10000.0},
        {"specificImpulse", 300.0},
        {"fuelMass", 500.0}
    };
    spacecraft.addPlugin(propulsion, propulsionConfig);

    // Initialize
    EXPECT_NO_THROW(spacecraft.initialize());

    // Set initial conditions
    propulsion->setThrustLevel(0.5);
    wheel->setCommandedTorque(0.05);

    // Run simulation
    double dt = 0.1;
    int numSteps = 100;

    double initialMass = massProps->getMass();
    double initialFuel = propulsion->getFuelMass();

    for (int i = 0; i < numSteps; ++i) {
        spacecraft.update(dt);
    }

    // Verify results
    EXPECT_LT(massProps->getMass(), initialMass); // Mass decreased
    EXPECT_LT(propulsion->getFuelMass(), initialFuel); // Fuel consumed
    EXPECT_GT(wheel->getCurrentSpeed(), 0.0); // Wheel spinning

    // Clean shutdown
    EXPECT_NO_THROW(spacecraft.shutdown());
}

TEST_F(IntegrationTest, DependencyInjectionWorks) {
    Vehicle vehicle;

    // Register in order that requires dependency resolution
    auto wheel = std::make_shared<ReactionWheel>();
    auto propulsion = std::make_shared<RocketPropulsion>();
    auto massProps = std::make_shared<MassProperties>();

    // Add plugins in wrong order - manager should handle it
    vehicle.addPlugin(wheel);
    vehicle.addPlugin(propulsion);
    vehicle.addPlugin(massProps); // Added last, but required by others

    EXPECT_NO_THROW(vehicle.initialize());
}

TEST_F(IntegrationTest, PropulsionAffectsMass) {
    Vehicle vehicle;

    auto massProps = std::make_shared<MassProperties>();
    auto propulsion = std::make_shared<RocketPropulsion>();

    nlohmann::json massConfig = {{"mass", 2000.0}};
    nlohmann::json propConfig = {
        {"maxThrust", 10000.0},
        {"specificImpulse", 300.0},
        {"fuelMass", 500.0}
    };

    vehicle.addPlugin(massProps, massConfig);
    vehicle.addPlugin(propulsion, propConfig);
    vehicle.initialize();

    double initialMass = massProps->getMass();
    double initialFuel = propulsion->getFuelMass();

    // Burn fuel
    propulsion->setThrustLevel(1.0);

    for (int i = 0; i < 10; ++i) {
        vehicle.update(1.0);
    }

    double finalMass = massProps->getMass();
    double finalFuel = propulsion->getFuelMass();
    double fuelConsumed = initialFuel - finalFuel;
    double massLost = initialMass - finalMass;

    // Mass loss should equal fuel consumed
    EXPECT_NEAR(massLost, fuelConsumed, 1e-3);
}

TEST_F(IntegrationTest, ContextSharing) {
    Vehicle vehicle;

    auto wheel = std::make_shared<ReactionWheel>();
    auto propulsion = std::make_shared<RocketPropulsion>();

    vehicle.addPlugin(wheel);
    vehicle.addPlugin(propulsion);
    vehicle.initialize();

    // Set thrust and torque
    propulsion->setThrustLevel(0.5);
    wheel->setCommandedTorque(0.05);

    vehicle.update(0.1);

    // Both plugins should have written to context
    auto& context = vehicle.getContext();
    EXPECT_TRUE(context.hasProperty("RocketPropulsion.thrust"));
    EXPECT_TRUE(context.hasProperty("ReactionWheel.speed"));
}

TEST_F(IntegrationTest, StatePersistence) {
    // Create vehicle and simulate
    auto spacecraft = std::make_unique<Vehicle>(VehicleType::Spacecraft, "TestSat");

    auto massProps = std::make_shared<MassProperties>();
    nlohmann::json config = {{"mass", 1000.0}};
    spacecraft->addPlugin(massProps, config);
    spacecraft->initialize();

    // Run some updates
    for (int i = 0; i < 10; ++i) {
        spacecraft->update(0.1);
    }

    // Get state
    auto state = massProps->getState();
    double mass = state["mass"].get<double>();

    // Create new plugin and restore state
    auto newMassProps = std::make_shared<MassProperties>();
    newMassProps->setState(state);

    EXPECT_DOUBLE_EQ(newMassProps->getMass(), mass);
}
