#include <gtest/gtest.h>
#include <memory>

import RE.Core.SimulationContext;
import RE.Plugins.MassProperties;
import RE.Plugins.Battery;
import RE.Plugins.DroneElectricPropulsion;

using namespace RE::Core;
using namespace RE::Plugins;

class DroneElectricPropulsionTest : public ::testing::Test {
protected:
    std::shared_ptr<MassProperties> massProps;
    std::shared_ptr<Battery> battery;
    std::shared_ptr<DroneElectricPropulsion> propulsion;
    SimulationContext context;

    void SetUp() override {
        massProps = std::make_shared<MassProperties>();
        battery = std::make_shared<Battery>();
        propulsion = std::make_shared<DroneElectricPropulsion>();
    }
};

TEST_F(DroneElectricPropulsionTest, Metadata) {
    auto metadata = propulsion->getMetadata();

    EXPECT_EQ(metadata.name, "DroneElectricPropulsion");
    EXPECT_EQ(metadata.dependencies.size(), 2);
}

TEST_F(DroneElectricPropulsionTest, Initialization) {
    nlohmann::json config = {
        {"numMotors", 4},
        {"motorConfig", {
            {"maxRpm", 8000.0},
            {"maxThrust", 12.0}
        }}
    };

    propulsion->initialize(config, context);

    EXPECT_EQ(propulsion->getNumMotors(), 4);
}

TEST_F(DroneElectricPropulsionTest, InitializationWithPositions) {
    nlohmann::json config = {
        {"numMotors", 4},
        {"motorPositions", {
            {0.15, 0.15, 0.0},
            {-0.15, 0.15, 0.0},
            {-0.15, -0.15, 0.0},
            {0.15, -0.15, 0.0}
        }}
    };

    EXPECT_NO_THROW(propulsion->initialize(config, context));
}

TEST_F(DroneElectricPropulsionTest, DependencyInjection) {
    nlohmann::json config = {{"numMotors", 4}};

    massProps->initialize(nlohmann::json::object(), context);
    battery->initialize(nlohmann::json{{"capacity", 5000.0}, {"voltage", 48.0}}, context);
    propulsion->initialize(config, context);

    EXPECT_NO_THROW(propulsion->injectDependency("MassProperties", massProps));
    EXPECT_NO_THROW(propulsion->injectDependency("Battery", battery));
}

TEST_F(DroneElectricPropulsionTest, ThrottleCommand) {
    nlohmann::json config = {
        {"numMotors", 4},
        {"motorConfig", {{"maxRpm", 8000.0}}}
    };

    massProps->initialize(nlohmann::json::object(), context);
    battery->initialize(nlohmann::json{{"capacity", 5000.0}, {"voltage", 48.0}}, context);
    propulsion->initialize(config, context);
    propulsion->injectDependency("MassProperties", massProps);
    propulsion->injectDependency("Battery", battery);

    propulsion->setAllMotorThrottle(0.5);

    // Update a few times to spin up motors
    for (int i = 0; i < 10; ++i) {
        propulsion->update(0.1, context);
        battery->update(0.1, context);
    }

    // Motors should be spinning
    EXPECT_GT(propulsion->getMotorRpm(0), 0.0);
    EXPECT_GT(propulsion->getTotalThrust(), 0.0);
}

TEST_F(DroneElectricPropulsionTest, IndividualMotorControl) {
    nlohmann::json config = {{"numMotors", 4}};

    massProps->initialize(nlohmann::json::object(), context);
    battery->initialize(nlohmann::json{{"capacity", 5000.0}, {"voltage", 48.0}}, context);
    propulsion->initialize(config, context);
    propulsion->injectDependency("MassProperties", massProps);
    propulsion->injectDependency("Battery", battery);

    propulsion->setMotorThrottle(0, 1.0);  // Full throttle motor 0
    propulsion->setMotorThrottle(1, 0.5);  // Half throttle motor 1
    propulsion->setMotorThrottle(2, 0.0);  // Off
    propulsion->setMotorThrottle(3, 0.0);  // Off

    for (int i = 0; i < 20; ++i) {
        propulsion->update(0.1, context);
        battery->update(0.1, context);
    }

    EXPECT_GT(propulsion->getMotorRpm(0), propulsion->getMotorRpm(1));
    EXPECT_GT(propulsion->getMotorRpm(1), propulsion->getMotorRpm(2));
    EXPECT_NEAR(propulsion->getMotorRpm(2), 0.0, 100.0);
}

TEST_F(DroneElectricPropulsionTest, BatteryDepletion) {
    nlohmann::json config = {{"numMotors", 4}};

    massProps->initialize(nlohmann::json::object(), context);
    battery->initialize(nlohmann::json{{"capacity", 10.0}, {"voltage", 48.0}}, context);
    propulsion->initialize(config, context);
    propulsion->injectDependency("MassProperties", massProps);
    propulsion->injectDependency("Battery", battery);

    propulsion->setAllMotorThrottle(1.0);  // Full throttle

    // Run until battery depleted
    for (int i = 0; i < 1000; ++i) {
        propulsion->update(1.0, context);
        battery->update(1.0, context);

        if (battery->isDepleted()) {
            break;
        }
    }

    EXPECT_TRUE(battery->isDepleted());

    // Continue running - motors should coast down
    double rpmBefore = propulsion->getMotorRpm(0);
    for (int i = 0; i < 5; ++i) {
        propulsion->update(0.1, context);
        battery->update(0.1, context);
    }
    double rpmAfter = propulsion->getMotorRpm(0);

    EXPECT_LT(rpmAfter, rpmBefore);  // Should be coasting down
}

TEST_F(DroneElectricPropulsionTest, ContextPropertyStorage) {
    nlohmann::json config = {{"numMotors", 4}};

    massProps->initialize(nlohmann::json::object(), context);
    battery->initialize(nlohmann::json{{"capacity", 5000.0}, {"voltage", 48.0}}, context);
    propulsion->initialize(config, context);
    propulsion->injectDependency("MassProperties", massProps);
    propulsion->injectDependency("Battery", battery);

    propulsion->setAllMotorThrottle(0.5);
    propulsion->update(0.1, context);

    EXPECT_TRUE(context.hasProperty("DroneElectricPropulsion.totalThrust"));
    EXPECT_TRUE(context.hasProperty("DroneElectricPropulsion.totalPower"));
    EXPECT_TRUE(context.hasProperty("DroneElectricPropulsion.thrustVector"));
}

TEST_F(DroneElectricPropulsionTest, GetState) {
    nlohmann::json config = {{"numMotors", 2}};

    propulsion->initialize(config, context);

    auto state = propulsion->getState();

    EXPECT_EQ(state["numMotors"].get<int>(), 2);
    EXPECT_TRUE(state.contains("motors"));
    EXPECT_TRUE(state["motors"].is_array());
    EXPECT_EQ(state["motors"].size(), 2);
}
