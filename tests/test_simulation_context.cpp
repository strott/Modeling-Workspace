#include <gtest/gtest.h>

import RE.Core.SimulationContext;

using namespace RE::Core;

class SimulationContextTest : public ::testing::Test {
protected:
    SimulationContext context;
};

TEST_F(SimulationContextTest, DefaultInitialization) {
    EXPECT_DOUBLE_EQ(context.currentTime, 0.0);
    EXPECT_DOUBLE_EQ(context.deltaTime, 0.0);
    EXPECT_TRUE(context.useCuda);
    EXPECT_EQ(context.cudaDeviceId, 0);
}

TEST_F(SimulationContextTest, TimeUpdates) {
    context.currentTime = 1.5;
    context.deltaTime = 0.1;

    EXPECT_DOUBLE_EQ(context.currentTime, 1.5);
    EXPECT_DOUBLE_EQ(context.deltaTime, 0.1);
}

TEST_F(SimulationContextTest, PropertyStorage) {
    // Set properties
    context.setProperty("testInt", 42);
    context.setProperty("testDouble", 3.14);
    context.setProperty("testString", std::string("hello"));

    // Get properties
    EXPECT_EQ(context.getProperty<int>("testInt"), 42);
    EXPECT_DOUBLE_EQ(context.getProperty<double>("testDouble"), 3.14);
    EXPECT_EQ(context.getProperty<std::string>("testString"), "hello");
}

TEST_F(SimulationContextTest, PropertyDefaults) {
    // Non-existent property should return default
    EXPECT_EQ(context.getProperty<int>("nonExistent", 100), 100);
    EXPECT_DOUBLE_EQ(context.getProperty<double>("nonExistent", 2.71), 2.71);
}

TEST_F(SimulationContextTest, PropertyOverwrite) {
    context.setProperty("value", 10);
    EXPECT_EQ(context.getProperty<int>("value"), 10);

    context.setProperty("value", 20);
    EXPECT_EQ(context.getProperty<int>("value"), 20);
}

TEST_F(SimulationContextTest, HasProperty) {
    EXPECT_FALSE(context.hasProperty("test"));

    context.setProperty("test", 42);
    EXPECT_TRUE(context.hasProperty("test"));
}
