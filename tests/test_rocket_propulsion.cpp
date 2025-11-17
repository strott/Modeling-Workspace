#include <gtest/gtest.h>
#include <memory>

import RE.Core.SimulationContext;
import RE.Plugins.MassProperties;
import RE.Plugins.RocketPropulsion;

using namespace RE::Core;
using namespace RE::Plugins;

class RocketPropulsionTest : public ::testing::Test {
protected:
    std::shared_ptr<MassProperties> massProps;
    std::shared_ptr<RocketPropulsion> propulsion;
    SimulationContext context;

    void SetUp() override {
        massProps = std::make_shared<MassProperties>();
        propulsion = std::make_shared<RocketPropulsion>();
    }
};

TEST_F(RocketPropulsionTest, Metadata) {
    auto metadata = propulsion->getMetadata();

    EXPECT_EQ(metadata.name, "RocketPropulsion");
    EXPECT_EQ(metadata.dependencies.size(), 1);
    EXPECT_EQ(metadata.dependencies[0], "MassProperties");
}

TEST_F(RocketPropulsionTest, Initialization) {
    nlohmann::json config = {
        {"maxThrust", 10000.0},
        {"specificImpulse", 300.0},
        {"fuelMass", 500.0},
        {"thrustDirection", {0, 0, 1}}
    };

    propulsion->initialize(config, context);

    EXPECT_DOUBLE_EQ(propulsion->getFuelMass(), 500.0);
    EXPECT_DOUBLE_EQ(propulsion->getThrustLevel(), 0.0);
    EXPECT_DOUBLE_EQ(propulsion->getFuelFraction(), 1.0);
}

TEST_F(RocketPropulsionTest, DependencyInjection) {
    nlohmann::json massConfig = {{"mass", 1000.0}};
    massProps->initialize(massConfig, context);
    propulsion->initialize(nlohmann::json::object(), context);

    EXPECT_NO_THROW(propulsion->injectDependency("MassProperties", massProps));
}

TEST_F(RocketPropulsionTest, NoThrustNoFuelConsumption) {
    nlohmann::json config = {
        {"maxThrust", 10000.0},
        {"specificImpulse", 300.0},
        {"fuelMass", 500.0}
    };
    propulsion->initialize(config, context);
    massProps->initialize(nlohmann::json{{"mass", 1000.0}}, context);
    propulsion->injectDependency("MassProperties", massProps);

    propulsion->setThrustLevel(0.0);
    double initialFuel = propulsion->getFuelMass();
    double initialMass = massProps->getMass();

    propulsion->update(1.0, context);

    EXPECT_DOUBLE_EQ(propulsion->getFuelMass(), initialFuel);
    EXPECT_DOUBLE_EQ(massProps->getMass(), initialMass);
}

TEST_F(RocketPropulsionTest, FuelConsumption) {
    nlohmann::json config = {
        {"maxThrust", 10000.0},
        {"specificImpulse", 300.0},
        {"fuelMass", 500.0}
    };
    propulsion->initialize(config, context);

    nlohmann::json massConfig = {{"mass", 1500.0}};
    massProps->initialize(massConfig, context);
    propulsion->injectDependency("MassProperties", massProps);

    propulsion->setThrustLevel(0.5); // 50% thrust
    double initialFuel = propulsion->getFuelMass();
    double initialMass = massProps->getMass();

    propulsion->update(1.0, context); // 1 second

    // Fuel should be consumed
    EXPECT_LT(propulsion->getFuelMass(), initialFuel);

    // Vehicle mass should decrease
    EXPECT_LT(massProps->getMass(), initialMass);

    // Mass change should equal fuel consumed
    double fuelConsumed = initialFuel - propulsion->getFuelMass();
    double massChange = initialMass - massProps->getMass();
    EXPECT_NEAR(fuelConsumed, massChange, 1e-6);
}

TEST_F(RocketPropulsionTest, FuelDepletion) {
    nlohmann::json config = {
        {"maxThrust", 10000.0},
        {"specificImpulse", 300.0},
        {"fuelMass", 10.0} // Small fuel amount
    };
    propulsion->initialize(config, context);

    nlohmann::json massConfig = {{"mass", 1000.0}};
    massProps->initialize(massConfig, context);
    propulsion->injectDependency("MassProperties", massProps);

    propulsion->setThrustLevel(1.0); // Full thrust

    // Run until fuel depleted
    for (int i = 0; i < 100; ++i) {
        propulsion->update(1.0, context);
    }

    // Fuel should be depleted
    EXPECT_NEAR(propulsion->getFuelMass(), 0.0, 1e-3);
    EXPECT_LT(propulsion->getFuelFraction(), 0.01);
}

TEST_F(RocketPropulsionTest, ThrustLevelClamping) {
    nlohmann::json config = {
        {"maxThrust", 10000.0},
        {"specificImpulse", 300.0},
        {"fuelMass", 500.0}
    };
    propulsion->initialize(config, context);

    // Test negative clamping
    propulsion->setThrustLevel(-0.5);
    propulsion->update(0.1, context);
    EXPECT_DOUBLE_EQ(propulsion->getFuelMass(), 500.0); // No fuel consumed

    // Test over 1.0 clamping
    propulsion->setThrustLevel(2.0);
    double fuel1 = propulsion->getFuelMass();

    propulsion->setThrustLevel(1.0);
    double fuel2 = propulsion->getFuelMass();

    // Should behave the same
    EXPECT_DOUBLE_EQ(fuel1, fuel2);
}

TEST_F(RocketPropulsionTest, ContextPropertyStorage) {
    nlohmann::json config = {
        {"maxThrust", 10000.0},
        {"specificImpulse", 300.0},
        {"fuelMass", 500.0}
    };
    propulsion->initialize(config, context);

    propulsion->setThrustLevel(0.5);
    propulsion->update(0.1, context);

    // Check properties stored in context
    EXPECT_TRUE(context.hasProperty("RocketPropulsion.thrust"));
    EXPECT_TRUE(context.hasProperty("RocketPropulsion.fuelMass"));
    EXPECT_TRUE(context.hasProperty("RocketPropulsion.fuelConsumptionRate"));
}
