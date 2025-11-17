#include <gtest/gtest.h>
#include <memory>

import RE.Core.SimulationContext;
import RE.Plugins.Battery;

using namespace RE::Core;
using namespace RE::Plugins;

class BatteryTest : public ::testing::Test {
protected:
    std::shared_ptr<Battery> battery;
    SimulationContext context;

    void SetUp() override {
        battery = std::make_shared<Battery>();
    }
};

TEST_F(BatteryTest, Metadata) {
    auto metadata = battery->getMetadata();

    EXPECT_EQ(metadata.name, "Battery");
    EXPECT_TRUE(metadata.dependencies.empty());
}

TEST_F(BatteryTest, Initialization) {
    nlohmann::json config = {
        {"capacity", 5000.0},
        {"voltage", 48.0},
        {"maxDischargeRate", 100.0},
        {"maxChargeRate", 20.0},
        {"efficiency", 0.95}
    };

    battery->initialize(config, context);

    EXPECT_DOUBLE_EQ(battery->getCapacity(), 5000.0);
    EXPECT_DOUBLE_EQ(battery->getVoltage(), 48.0);
    EXPECT_DOUBLE_EQ(battery->getStateOfCharge(), 1.0); // Full
    EXPECT_FALSE(battery->isDepleted());
}

TEST_F(BatteryTest, InitialChargePartial) {
    nlohmann::json config = {
        {"capacity", 5000.0},
        {"initialCharge", 2500.0}
    };

    battery->initialize(config, context);

    EXPECT_DOUBLE_EQ(battery->getStateOfCharge(), 0.5);
}

TEST_F(BatteryTest, PowerRequest) {
    nlohmann::json config = {
        {"capacity", 5000.0},
        {"voltage", 48.0},
        {"maxDischargeRate", 100.0}
    };

    battery->initialize(config, context);

    // Request reasonable power
    double availablePower = battery->requestPower(1000.0);
    EXPECT_DOUBLE_EQ(availablePower, 1000.0);
}

TEST_F(BatteryTest, PowerRequestExceedsMax) {
    nlohmann::json config = {
        {"capacity", 5000.0},
        {"voltage", 48.0},
        {"maxDischargeRate", 100.0} // Max = 100A * 48V = 4800W
    };

    battery->initialize(config, context);

    // Request more than max discharge
    double availablePower = battery->requestPower(10000.0);
    EXPECT_DOUBLE_EQ(availablePower, 4800.0); // Clamped to max
}

TEST_F(BatteryTest, EnergyConsumption) {
    nlohmann::json config = {
        {"capacity", 100.0},  // 100 Wh
        {"voltage", 48.0},
        {"maxDischargeRate", 100.0},
        {"efficiency", 1.0}  // 100% for easy calculation
    };

    battery->initialize(config, context);

    double initialCharge = battery->getRemainingCharge();

    // Request 100W for 1 second = 100/3600 Wh
    battery->requestPower(100.0);
    battery->update(1.0, context);

    double expectedConsumed = 100.0 / 3600.0; // Wh
    EXPECT_NEAR(battery->getRemainingCharge(), initialCharge - expectedConsumed, 1e-6);
}

TEST_F(BatteryTest, Depletion) {
    nlohmann::json config = {
        {"capacity", 10.0},  // Small capacity
        {"voltage", 48.0},
        {"maxDischargeRate", 100.0},
        {"efficiency", 1.0}
    };

    battery->initialize(config, context);

    // Drain battery
    for (int i = 0; i < 1000; ++i) {
        battery->requestPower(1000.0);
        battery->update(1.0, context);
    }

    EXPECT_TRUE(battery->isDepleted());
    EXPECT_NEAR(battery->getStateOfCharge(), 0.0, 1e-6);

    // Should not provide power when depleted
    double availablePower = battery->requestPower(100.0);
    EXPECT_DOUBLE_EQ(availablePower, 0.0);
}

TEST_F(BatteryTest, EfficiencyLoss) {
    nlohmann::json config = {
        {"capacity", 100.0},
        {"voltage", 48.0},
        {"maxDischargeRate", 100.0},
        {"efficiency", 0.8}  // 80% efficient
    };

    battery->initialize(config, context);

    double initialCharge = battery->getRemainingCharge();

    // Request 80W for 1 hour
    battery->requestPower(80.0);
    battery->update(3600.0, context); // 1 hour

    // With 80% efficiency, 80Wh requested = 100Wh consumed
    EXPECT_NEAR(battery->getRemainingCharge(), 0.0, 1e-3);
}

TEST_F(BatteryTest, ContextPropertyStorage) {
    nlohmann::json config = {
        {"capacity", 100.0},
        {"voltage", 48.0}
    };

    battery->initialize(config, context);
    battery->update(0.1, context);

    EXPECT_TRUE(context.hasProperty("Battery.charge"));
    EXPECT_TRUE(context.hasProperty("Battery.stateOfCharge"));
    EXPECT_TRUE(context.hasProperty("Battery.powerAvailable"));
}

TEST_F(BatteryTest, GetState) {
    nlohmann::json config = {
        {"capacity", 5000.0},
        {"voltage", 48.0}
    };

    battery->initialize(config, context);

    auto state = battery->getState();

    EXPECT_DOUBLE_EQ(state["capacity"].get<double>(), 5000.0);
    EXPECT_DOUBLE_EQ(state["voltage"].get<double>(), 48.0);
    EXPECT_DOUBLE_EQ(state["stateOfCharge"].get<double>(), 1.0);
}

TEST_F(BatteryTest, SetState) {
    battery->initialize(nlohmann::json::object(), context);

    nlohmann::json state = {
        {"capacity", 3000.0},
        {"voltage", 24.0},
        {"currentCharge", 1500.0}
    };

    battery->setState(state);

    EXPECT_DOUBLE_EQ(battery->getCapacity(), 3000.0);
    EXPECT_DOUBLE_EQ(battery->getVoltage(), 24.0);
}
