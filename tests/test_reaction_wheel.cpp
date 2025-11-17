#include <gtest/gtest.h>
#include <memory>
#include <cmath>

import RE.Core.SimulationContext;
import RE.Plugins.MassProperties;
import RE.Plugins.ReactionWheel;

using namespace RE::Core;
using namespace RE::Plugins;

class ReactionWheelTest : public ::testing::Test {
protected:
    std::shared_ptr<MassProperties> massProps;
    std::shared_ptr<ReactionWheel> wheel;
    SimulationContext context;

    void SetUp() override {
        massProps = std::make_shared<MassProperties>();
        wheel = std::make_shared<ReactionWheel>();
    }
};

TEST_F(ReactionWheelTest, Metadata) {
    auto metadata = wheel->getMetadata();

    EXPECT_EQ(metadata.name, "ReactionWheel");
    EXPECT_EQ(metadata.dependencies.size(), 1);
    EXPECT_EQ(metadata.dependencies[0], "MassProperties");
}

TEST_F(ReactionWheelTest, Initialization) {
    nlohmann::json config = {
        {"wheelInertia", 0.05},
        {"maxTorque", 0.1},
        {"maxSpeed", 6000.0},
        {"spinAxis", {0, 0, 1}}
    };

    wheel->initialize(config, context);

    EXPECT_DOUBLE_EQ(wheel->getWheelInertia(), 0.05);
    EXPECT_DOUBLE_EQ(wheel->getCurrentSpeed(), 0.0);
}

TEST_F(ReactionWheelTest, DependencyInjection) {
    massProps->initialize(nlohmann::json::object(), context);
    wheel->initialize(nlohmann::json::object(), context);

    EXPECT_NO_THROW(wheel->injectDependency("MassProperties", massProps));
}

TEST_F(ReactionWheelTest, TorqueCommand) {
    nlohmann::json config = {
        {"wheelInertia", 0.05},
        {"maxTorque", 0.1},
        {"maxSpeed", 6000.0}
    };
    wheel->initialize(config, context);

    wheel->setCommandedTorque(0.05);
    EXPECT_DOUBLE_EQ(wheel->getCurrentSpeed(), 0.0); // Not yet updated

    wheel->update(0.1, context);

    // Speed should increase: omega = torque * dt / inertia = 0.05 * 0.1 / 0.05 = 0.1 rad/s
    EXPECT_NEAR(wheel->getCurrentSpeed(), 0.1, 1e-6);
}

TEST_F(ReactionWheelTest, TorqueClipping) {
    nlohmann::json config = {
        {"wheelInertia", 0.05},
        {"maxTorque", 0.1},
        {"maxSpeed", 6000.0}
    };
    wheel->initialize(config, context);

    // Command torque beyond limit
    wheel->setCommandedTorque(0.5); // Max is 0.1
    wheel->update(0.1, context);

    // Should be clamped to max torque
    // omega = 0.1 * 0.1 / 0.05 = 0.2 rad/s
    EXPECT_NEAR(wheel->getCurrentSpeed(), 0.2, 1e-6);
}

TEST_F(ReactionWheelTest, SpeedClipping) {
    nlohmann::json config = {
        {"wheelInertia", 0.05},
        {"maxTorque", 1.0},
        {"maxSpeed", 1.0} // Low max speed
    };
    wheel->initialize(config, context);

    wheel->setCommandedTorque(1.0);

    // Run multiple updates to exceed max speed
    for (int i = 0; i < 100; ++i) {
        wheel->update(0.1, context);
    }

    // Speed should be clamped
    EXPECT_LE(wheel->getCurrentSpeed(), 1.0);
    EXPECT_NEAR(wheel->getCurrentSpeed(), 1.0, 1e-6);
}

TEST_F(ReactionWheelTest, ContextPropertyStorage) {
    nlohmann::json config = {
        {"wheelInertia", 0.05},
        {"maxTorque", 0.1}
    };
    wheel->initialize(config, context);

    wheel->setCommandedTorque(0.05);
    wheel->update(0.1, context);

    // Check that properties are stored in context
    EXPECT_TRUE(context.hasProperty("ReactionWheel.speed"));
    EXPECT_TRUE(context.hasProperty("ReactionWheel.torque"));
}
