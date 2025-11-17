# Test suite for Reign

# Test for SimulationContext
add_executable(test_simulation_context test_simulation_context.cpp)
target_link_libraries(test_simulation_context
    PRIVATE
        reign_core
        GTest::gtest_main
)
gtest_discover_tests(test_simulation_context)

# Test for PluginManager
add_executable(test_plugin_manager test_plugin_manager.cpp)
target_link_libraries(test_plugin_manager
    PRIVATE
        reign_core
        reign_plugins
        GTest::gtest_main
)
gtest_discover_tests(test_plugin_manager)

# Test for MassProperties plugin
add_executable(test_mass_properties test_mass_properties.cpp)
target_link_libraries(test_mass_properties
    PRIVATE
        reign_core
        reign_plugins
        GTest::gtest_main
)
gtest_discover_tests(test_mass_properties)

# Test for ReactionWheel plugin
add_executable(test_reaction_wheel test_reaction_wheel.cpp)
target_link_libraries(test_reaction_wheel
    PRIVATE
        reign_core
        reign_plugins
        GTest::gtest_main
)
gtest_discover_tests(test_reaction_wheel)

# Test for RocketPropulsion plugin
add_executable(test_rocket_propulsion test_rocket_propulsion.cpp)
target_link_libraries(test_rocket_propulsion
    PRIVATE
        reign_core
        reign_plugins
        GTest::gtest_main
)
gtest_discover_tests(test_rocket_propulsion)

# Test for Vehicle
add_executable(test_vehicle test_vehicle.cpp)
target_link_libraries(test_vehicle
    PRIVATE
        reign_core
        reign_plugins
        GTest::gtest_main
)
gtest_discover_tests(test_vehicle)

# Integration tests
add_executable(test_integration test_integration.cpp)
target_link_libraries(test_integration
    PRIVATE
        reign_core
        reign_plugins
        GTest::gtest_main
)
gtest_discover_tests(test_integration)
