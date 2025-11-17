#include <gtest/gtest.h>
#include <memory>

import RE.Core.PluginManager;
import RE.Core.SimulationContext;
import RE.Plugins.MassProperties;
import RE.Plugins.ReactionWheel;

using namespace RE::Core;
using namespace RE::Plugins;

class PluginManagerTest : public ::testing::Test {
protected:
    PluginManager manager;
    SimulationContext context;
};

TEST_F(PluginManagerTest, RegisterPlugin) {
    auto plugin = std::make_shared<MassProperties>();
    EXPECT_NO_THROW(manager.registerPlugin(plugin));

    EXPECT_TRUE(manager.hasPlugin("MassProperties"));
    EXPECT_EQ(manager.getPluginNames().size(), 1);
}

TEST_F(PluginManagerTest, RegisterDuplicatePlugin) {
    auto plugin1 = std::make_shared<MassProperties>();
    auto plugin2 = std::make_shared<MassProperties>();

    manager.registerPlugin(plugin1);

    EXPECT_THROW(manager.registerPlugin(plugin2), std::runtime_error);
}

TEST_F(PluginManagerTest, GetPlugin) {
    auto plugin = std::make_shared<MassProperties>();
    manager.registerPlugin(plugin);

    auto retrieved = manager.getPlugin("MassProperties");
    EXPECT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved, plugin);
}

TEST_F(PluginManagerTest, GetTypedPlugin) {
    auto plugin = std::make_shared<MassProperties>();
    manager.registerPlugin(plugin);

    auto retrieved = manager.getPlugin<MassProperties>("MassProperties");
    EXPECT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved, plugin);
}

TEST_F(PluginManagerTest, GetNonExistentPlugin) {
    auto retrieved = manager.getPlugin("NonExistent");
    EXPECT_EQ(retrieved, nullptr);
}

TEST_F(PluginManagerTest, InitializeAll) {
    auto massProps = std::make_shared<MassProperties>();
    nlohmann::json config = {
        {"mass", 1000.0}
    };
    manager.registerPlugin(massProps, config);

    EXPECT_NO_THROW(manager.initializeAll(context));

    // Verify plugin was initialized
    EXPECT_DOUBLE_EQ(massProps->getMass(), 1000.0);
}

TEST_F(PluginManagerTest, DependencyInjection) {
    // Register plugins with dependency
    auto massProps = std::make_shared<MassProperties>();
    auto wheel = std::make_shared<ReactionWheel>();

    nlohmann::json massConfig = {{"mass", 500.0}};
    nlohmann::json wheelConfig = {{"wheelInertia", 0.05}};

    manager.registerPlugin(massProps, massConfig);
    manager.registerPlugin(wheel, wheelConfig);

    EXPECT_NO_THROW(manager.initializeAll(context));

    // ReactionWheel should have MassProperties injected
    // This is verified by the fact that initialization succeeds
}

TEST_F(PluginManagerTest, MissingDependency) {
    // Register wheel without mass properties
    auto wheel = std::make_shared<ReactionWheel>();
    manager.registerPlugin(wheel);

    EXPECT_THROW(manager.initializeAll(context), std::runtime_error);
}

TEST_F(PluginManagerTest, UpdateAll) {
    auto massProps = std::make_shared<MassProperties>();
    manager.registerPlugin(massProps);
    manager.initializeAll(context);

    EXPECT_NO_THROW(manager.updateAll(0.1, context));
}

TEST_F(PluginManagerTest, ShutdownAll) {
    auto massProps = std::make_shared<MassProperties>();
    manager.registerPlugin(massProps);
    manager.initializeAll(context);

    EXPECT_NO_THROW(manager.shutdownAll());
}
