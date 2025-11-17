#include <gtest/gtest.h>
#include <memory>

import RE.Core.SimulationContext;
import RE.Plugins.MassProperties;
import RE.Plugins.Battery;
import RE.Plugins.SpacecraftElectricPropulsion;

using namespace RE::Core;
using namespace RE::Plugins;

class SpacecraftElectricPropulsionTest : public ::testing::Test {
protected:
    std::shared_ptr<MassProperties> massProps;
    std::shared_ptr<Battery> battery;
    std::shared_ptr<SpacecraftElectricPropulsion> propulsion;
    SimulationContext context;

    void SetUp() override {
        massProps = std::make_shared<MassProperties>();
        battery = std::make_shared<Battery>();
        propulsion = std::make_shared<SpacecraftElectricPropulsion>();
    }
};

TEST_F(SpacecraftElectricPropulsionTest, Metadata) {
    auto metadata = propulsion->getMetadata();

    EXPECT_EQ(metadata.name, "SpacecraftElectricPropulsion");
    EXPECT_EQ(metadata.dependencies.size(), 2);
}

TEST_F(SpacecraftElectricPropulsionTest, Initialization) {
    nlohmann::json config = {
        {"thrusterType", "HallEffect"},
        {"maxPower", 5000.0},
        {"specificImpulse", 2000.0},
        {"maxThrust", 0.1},
        {"efficiency", 0.65},
        {"propellantMass", 20.0}
    };

    propulsion->initialize(config, context);

    EXPECT_DOUBLE_EQ(propulsion->getSpecificImpulse(), 2000.0);
    EXPECT_DOUBLE_EQ(propulsion->getPropellantMass(), 20.0);
    EXPECT_DOUBLE_EQ(propulsion->getPropellantFraction(), 1.0);
}

TEST_F(SpacecraftElectricPropulsionTest, ThrusterTypes) {
    nlohmann::json config1 = {{"thrusterType", "IonThruster"}, {"maxPower", 1000.0}};
    nlohmann::json config2 = {{"thrusterType", "HallEffect"}, {"maxPower", 1000.0}};
    nlohmann::json config3 = {{"thrusterType", "VASIMR"}, {"maxPower", 1000.0}};

    auto ion = std::make_shared<SpacecraftElectricPropulsion>();
    auto hall = std::make_shared<SpacecraftElectricPropulsion>();
    auto vasimr = std::make_shared<SpacecraftElectricPropulsion>();

    EXPECT_NO_THROW(ion->initialize(config1, context));
    EXPECT_NO_THROW(hall->initialize(config2, context));
    EXPECT_NO_THROW(vasimr->initialize(config3, context));
}

TEST_F(SpacecraftElectricPropulsionTest, DependencyInjection) {
    nlohmann::json config = {
        {"maxPower", 5000.0},
        {"specificImpulse", 2000.0},
        {"propellantMass", 20.0}
    };

    massProps->initialize(nlohmann::json{{"mass", 500.0}}, context);
    battery->initialize(nlohmann::json{{"capacity", 10000.0}, {"voltage", 100.0}}, context);
    propulsion->initialize(config, context);

    EXPECT_NO_THROW(propulsion->injectDependency("MassProperties", massProps));
    EXPECT_NO_THROW(propulsion->injectDependency("Battery", battery));
}

TEST_F(SpacecraftElectricPropulsionTest, PowerLevelControl) {
    nlohmann::json config = {
        {"maxPower", 5000.0},
        {"specificImpulse", 2000.0},
        {"maxThrust", 0.2},
        {"propellantMass", 20.0}
    };

    massProps->initialize(nlohmann::json{{"mass", 500.0}}, context);
    battery->initialize(nlohmann::json{{"capacity", 10000.0}, {"voltage", 100.0}}, context);
    propulsion->initialize(config, context);
    propulsion->injectDependency("MassProperties", massProps);
    propulsion->injectDependency("Battery", battery);

    propulsion->setPowerLevel(0.5);  // 50% power

    propulsion->update(1.0, context);
    battery->update(1.0, context);

    EXPECT_GT(propulsion->getCurrentThrust(), 0.0);
    EXPECT_GT(propulsion->getCurrentPower(), 0.0);
    EXPECT_LT(propulsion->getCurrentPower(), 5000.0);  // Less than max
}

TEST_F(SpacecraftElectricPropulsionTest, PropellantConsumption) {
    nlohmann::json config = {
        {"maxPower", 5000.0},
        {"specificImpulse", 2000.0},
        {"maxThrust", 0.2},
        {"efficiency", 0.65},
        {"propellantMass", 20.0}
    };

    massProps->initialize(nlohmann::json{{"mass", 500.0}}, context);
    battery->initialize(nlohmann::json{{"capacity", 10000.0}, {"voltage", 100.0}}, context);
    propulsion->initialize(config, context);
    propulsion->injectDependency("MassProperties", massProps);
    propulsion->injectDependency("Battery", battery);

    double initialPropellant = propulsion->getPropellantMass();
    double initialMass = massProps->getMass();

    propulsion->setPowerLevel(1.0);  // Full power

    // Run for some time
    for (int i = 0; i < 100; ++i) {
        propulsion->update(1.0, context);
        battery->update(1.0, context);
    }

    // Propellant should be consumed
    EXPECT_LT(propulsion->getPropellantMass(), initialPropellant);

    // Vehicle mass should decrease
    EXPECT_LT(massProps->getMass(), initialMass);

    // Mass decrease should equal propellant consumed
    double propellantConsumed = initialPropellant - propulsion->getPropellantMass();
    double massLost = initialMass - massProps->getMass();
    EXPECT_NEAR(propellantConsumed, massLost, 1e-6);
}

TEST_F(SpacecraftElectricPropulsionTest, HighSpecificImpulse) {
    // Electric propulsion should have very high ISP compared to chemical
    nlohmann::json config = {
        {"maxPower", 5000.0},
        {"specificImpulse", 3000.0},  // Very high
        {"maxThrust", 0.1},
        {"propellantMass", 10.0}
    };

    massProps->initialize(nlohmann::json{{"mass", 500.0}}, context);
    battery->initialize(nlohmann::json{{"capacity", 100000.0}, {"voltage", 100.0}}, context);
    propulsion->initialize(config, context);
    propulsion->injectDependency("MassProperties", massProps);
    propulsion->injectDependency("Battery", battery);

    double initialPropellant = propulsion->getPropellantMass();

    propulsion->setPowerLevel(1.0);

    // Run for a long time
    for (int i = 0; i < 1000; ++i) {
        propulsion->update(10.0, context);
        battery->update(10.0, context);
    }

    // High ISP means propellant lasts longer
    double propellantFraction = propulsion->getPropellantFraction();
    EXPECT_GT(propellantFraction, 0.0);  // Should still have propellant
}

TEST_F(SpacecraftElectricPropulsionTest, PropellantDepletion) {
    nlohmann::json config = {
        {"maxPower", 5000.0},
        {"specificImpulse", 2000.0},
        {"maxThrust", 0.2},
        {"propellantMass", 1.0}  // Small amount
    };

    massProps->initialize(nlohmann::json{{"mass", 500.0}}, context);
    battery->initialize(nlohmann::json{{"capacity", 100000.0}, {"voltage", 100.0}}, context);
    propulsion->initialize(config, context);
    propulsion->injectDependency("MassProperties", massProps);
    propulsion->injectDependency("Battery", battery);

    propulsion->setPowerLevel(1.0);

    // Run until propellant depleted
    for (int i = 0; i < 10000; ++i) {
        propulsion->update(1.0, context);
        battery->update(1.0, context);

        if (propulsion->getPropellantMass() <= 0.0) {
            break;
        }
    }

    EXPECT_NEAR(propulsion->getPropellantMass(), 0.0, 1e-3);
    EXPECT_NEAR(propulsion->getCurrentThrust(), 0.0, 1e-3);
}

TEST_F(SpacecraftElectricPropulsionTest, BatteryLimitedOperation) {
    nlohmann::json config = {
        {"maxPower", 5000.0},
        {"specificImpulse", 2000.0},
        {"propellantMass", 20.0}
    };

    massProps->initialize(nlohmann::json{{"mass", 500.0}}, context);

    // Small battery that will limit power
    battery->initialize(nlohmann::json{
        {"capacity", 100.0},
        {"voltage", 100.0},
        {"maxDischargeRate", 10.0}  // Max 1000W
    }, context);

    propulsion->initialize(config, context);
    propulsion->injectDependency("MassProperties", massProps);
    propulsion->injectDependency("Battery", battery);

    propulsion->setPowerLevel(1.0);  // Request full power (5000W)

    propulsion->update(1.0, context);
    battery->update(1.0, context);

    // Should be limited by battery
    EXPECT_LT(propulsion->getCurrentPower(), 5000.0);
    EXPECT_LE(propulsion->getCurrentPower(), 1000.0);
}

TEST_F(SpacecraftElectricPropulsionTest, ContextPropertyStorage) {
    nlohmann::json config = {
        {"maxPower", 5000.0},
        {"specificImpulse", 2000.0},
        {"propellantMass", 20.0}
    };

    massProps->initialize(nlohmann::json::object(), context);
    battery->initialize(nlohmann::json{{"capacity", 10000.0}, {"voltage", 100.0}}, context);
    propulsion->initialize(config, context);
    propulsion->injectDependency("MassProperties", massProps);
    propulsion->injectDependency("Battery", battery);

    propulsion->setPowerLevel(0.5);
    propulsion->update(0.1, context);

    EXPECT_TRUE(context.hasProperty("SpacecraftElectricPropulsion.thrust"));
    EXPECT_TRUE(context.hasProperty("SpacecraftElectricPropulsion.power"));
    EXPECT_TRUE(context.hasProperty("SpacecraftElectricPropulsion.propellantMass"));
    EXPECT_TRUE(context.hasProperty("SpacecraftElectricPropulsion.specificImpulse"));
}

TEST_F(SpacecraftElectricPropulsionTest, GetState) {
    nlohmann::json config = {
        {"thrusterType", "HallEffect"},
        {"maxPower", 5000.0},
        {"specificImpulse", 2000.0}
    };

    propulsion->initialize(config, context);

    auto state = propulsion->getState();

    EXPECT_EQ(state["thrusterType"].get<std::string>(), "HallEffect");
    EXPECT_DOUBLE_EQ(state["maxPower"].get<double>(), 5000.0);
    EXPECT_DOUBLE_EQ(state["specificImpulse"].get<double>(), 2000.0);
}
