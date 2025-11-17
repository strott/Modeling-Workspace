#include <gtest/gtest.h>
#include <memory>

import RE.Core.Vehicle;
import RE.Plugins.MassProperties;
import RE.Plugins.ReactionWheel;

using namespace RE::Core;
using namespace RE::Plugins;

class VehicleTest : public ::testing::Test {
protected:
    // Test setup if needed
};

TEST_F(VehicleTest, DefaultConstruction) {
    Vehicle vehicle;

    EXPECT_EQ(vehicle.getName(), "Vehicle");
    EXPECT_EQ(vehicle.getType(), VehicleType::Custom);
}

TEST_F(VehicleTest, ConstructionWithParameters) {
    Vehicle spacecraft(VehicleType::Spacecraft, "TestSat");

    EXPECT_EQ(spacecraft.getName(), "TestSat");
    EXPECT_EQ(spacecraft.getType(), VehicleType::Spacecraft);
}

TEST_F(VehicleTest, AddPlugin) {
    Vehicle vehicle;

    auto massProps = std::make_shared<MassProperties>();
    nlohmann::json config = {{"mass", 1000.0}};

    EXPECT_NO_THROW(vehicle.addPlugin(massProps, config));
}

TEST_F(VehicleTest, GetPlugin) {
    Vehicle vehicle;

    auto massProps = std::make_shared<MassProperties>();
    vehicle.addPlugin(massProps);
    vehicle.initialize();

    auto retrieved = vehicle.getPlugin("MassProperties");
    EXPECT_NE(retrieved, nullptr);
}

TEST_F(VehicleTest, GetTypedPlugin) {
    Vehicle vehicle;

    auto massProps = std::make_shared<MassProperties>();
    vehicle.addPlugin(massProps);
    vehicle.initialize();

    auto retrieved = vehicle.getPlugin<MassProperties>("MassProperties");
    EXPECT_NE(retrieved, nullptr);
}

TEST_F(VehicleTest, Initialize) {
    Vehicle vehicle;

    auto massProps = std::make_shared<MassProperties>();
    nlohmann::json config = {{"mass", 1500.0}};
    vehicle.addPlugin(massProps, config);

    EXPECT_NO_THROW(vehicle.initialize());

    // Verify plugin was initialized
    EXPECT_DOUBLE_EQ(massProps->getMass(), 1500.0);
}

TEST_F(VehicleTest, Update) {
    Vehicle vehicle;

    auto massProps = std::make_shared<MassProperties>();
    vehicle.addPlugin(massProps);
    vehicle.initialize();

    double dt = 0.1;
    EXPECT_NO_THROW(vehicle.update(dt));

    // Context time should be updated
    EXPECT_DOUBLE_EQ(vehicle.getContext().currentTime, dt);
    EXPECT_DOUBLE_EQ(vehicle.getContext().deltaTime, dt);
}

TEST_F(VehicleTest, MultipleUpdates) {
    Vehicle vehicle;

    auto massProps = std::make_shared<MassProperties>();
    vehicle.addPlugin(massProps);
    vehicle.initialize();

    double dt = 0.1;
    int numSteps = 10;

    for (int i = 0; i < numSteps; ++i) {
        vehicle.update(dt);
    }

    EXPECT_NEAR(vehicle.getContext().currentTime, dt * numSteps, 1e-6);
}

TEST_F(VehicleTest, Shutdown) {
    Vehicle vehicle;

    auto massProps = std::make_shared<MassProperties>();
    vehicle.addPlugin(massProps);
    vehicle.initialize();

    EXPECT_NO_THROW(vehicle.shutdown());
}

TEST_F(VehicleTest, MultiplePlugins) {
    Vehicle vehicle;

    auto massProps = std::make_shared<MassProperties>();
    auto wheel = std::make_shared<ReactionWheel>();

    nlohmann::json massConfig = {{"mass", 1000.0}};
    nlohmann::json wheelConfig = {{"wheelInertia", 0.05}};

    vehicle.addPlugin(massProps, massConfig);
    vehicle.addPlugin(wheel, wheelConfig);

    EXPECT_NO_THROW(vehicle.initialize());

    // Both plugins should be accessible
    EXPECT_NE(vehicle.getPlugin("MassProperties"), nullptr);
    EXPECT_NE(vehicle.getPlugin("ReactionWheel"), nullptr);
}

TEST_F(VehicleTest, SaveToFile) {
    Vehicle vehicle(VehicleType::Spacecraft, "TestSpacecraft");

    auto massProps = std::make_shared<MassProperties>();
    nlohmann::json config = {{"mass", 1000.0}};
    vehicle.addPlugin(massProps, config);
    vehicle.initialize();

    std::string filename = "/tmp/test_vehicle_config.json";
    EXPECT_NO_THROW(vehicle.saveToFile(filename));

    // File should exist (we could check this, but requires filesystem)
}

TEST_F(VehicleTest, ContextAccess) {
    Vehicle vehicle;

    auto& context = vehicle.getContext();
    context.useCuda = false;
    context.cudaDeviceId = 1;

    EXPECT_FALSE(vehicle.getContext().useCuda);
    EXPECT_EQ(vehicle.getContext().cudaDeviceId, 1);
}
