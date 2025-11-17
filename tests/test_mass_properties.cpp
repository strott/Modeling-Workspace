#include <gtest/gtest.h>
#include <memory>

import RE.Core.SimulationContext;
import RE.Plugins.MassProperties;

using namespace RE::Core;
using namespace RE::Plugins;

class MassPropertiesTest : public ::testing::Test {
protected:
    std::shared_ptr<MassProperties> plugin;
    SimulationContext context;

    void SetUp() override {
        plugin = std::make_shared<MassProperties>();
    }
};

TEST_F(MassPropertiesTest, Metadata) {
    auto metadata = plugin->getMetadata();

    EXPECT_EQ(metadata.name, "MassProperties");
    EXPECT_EQ(metadata.version, "1.0.0");
    EXPECT_TRUE(metadata.dependencies.empty());
}

TEST_F(MassPropertiesTest, DefaultInitialization) {
    nlohmann::json config = nlohmann::json::object();
    plugin->initialize(config, context);

    EXPECT_DOUBLE_EQ(plugin->getMass(), 0.0);

    auto cog = plugin->getCenterOfGravity();
    EXPECT_DOUBLE_EQ(cog[0], 0.0);
    EXPECT_DOUBLE_EQ(cog[1], 0.0);
    EXPECT_DOUBLE_EQ(cog[2], 0.0);
}

TEST_F(MassPropertiesTest, ConfiguredInitialization) {
    nlohmann::json config = {
        {"mass", 1500.0},
        {"centerOfGravity", {1.0, 2.0, 3.0}},
        {"inertiaTensor", {
            {100.0, 0.0, 0.0},
            {0.0, 150.0, 0.0},
            {0.0, 0.0, 120.0}
        }}
    };

    plugin->initialize(config, context);

    EXPECT_DOUBLE_EQ(plugin->getMass(), 1500.0);

    auto cog = plugin->getCenterOfGravity();
    EXPECT_DOUBLE_EQ(cog[0], 1.0);
    EXPECT_DOUBLE_EQ(cog[1], 2.0);
    EXPECT_DOUBLE_EQ(cog[2], 3.0);

    auto inertia = plugin->getInertiaTensor();
    EXPECT_DOUBLE_EQ(inertia[0][0], 100.0);
    EXPECT_DOUBLE_EQ(inertia[1][1], 150.0);
    EXPECT_DOUBLE_EQ(inertia[2][2], 120.0);
}

TEST_F(MassPropertiesTest, SetMass) {
    plugin->initialize(nlohmann::json::object(), context);

    plugin->setMass(2000.0);
    EXPECT_DOUBLE_EQ(plugin->getMass(), 2000.0);
}

TEST_F(MassPropertiesTest, SetCenterOfGravity) {
    plugin->initialize(nlohmann::json::object(), context);

    std::array<double, 3> newCog = {5.0, 6.0, 7.0};
    plugin->setCenterOfGravity(newCog);

    auto cog = plugin->getCenterOfGravity();
    EXPECT_DOUBLE_EQ(cog[0], 5.0);
    EXPECT_DOUBLE_EQ(cog[1], 6.0);
    EXPECT_DOUBLE_EQ(cog[2], 7.0);
}

TEST_F(MassPropertiesTest, GetState) {
    nlohmann::json config = {
        {"mass", 1000.0},
        {"centerOfGravity", {1.0, 2.0, 3.0}}
    };
    plugin->initialize(config, context);

    auto state = plugin->getState();

    EXPECT_DOUBLE_EQ(state["mass"].get<double>(), 1000.0);
    EXPECT_DOUBLE_EQ(state["centerOfGravity"][0].get<double>(), 1.0);
    EXPECT_DOUBLE_EQ(state["centerOfGravity"][1].get<double>(), 2.0);
    EXPECT_DOUBLE_EQ(state["centerOfGravity"][2].get<double>(), 3.0);
}

TEST_F(MassPropertiesTest, SetState) {
    plugin->initialize(nlohmann::json::object(), context);

    nlohmann::json state = {
        {"mass", 500.0},
        {"centerOfGravity", {10.0, 20.0, 30.0}}
    };
    plugin->setState(state);

    EXPECT_DOUBLE_EQ(plugin->getMass(), 500.0);

    auto cog = plugin->getCenterOfGravity();
    EXPECT_DOUBLE_EQ(cog[0], 10.0);
    EXPECT_DOUBLE_EQ(cog[1], 20.0);
    EXPECT_DOUBLE_EQ(cog[2], 30.0);
}

TEST_F(MassPropertiesTest, Update) {
    plugin->initialize(nlohmann::json::object(), context);

    double initialMass = plugin->getMass();
    plugin->update(0.1, context);

    // Mass should not change during update (unless modified by other plugins)
    EXPECT_DOUBLE_EQ(plugin->getMass(), initialMass);
}
