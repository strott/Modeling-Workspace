# Core library with C++20 modules
add_library(reign_core)

target_sources(reign_core
    PUBLIC
        FILE_SET CXX_MODULES FILES
            core/IPlugin.cppm
            core/PluginManager.cppm
            core/Vehicle.cppm
            core/SimulationContext.cppm
            compute/CudaContext.cppm
            compute/DynamicsKernel.cppm
)

target_link_libraries(reign_core
    PUBLIC
        CUDA::cudart
        nlohmann_json::nlohmann_json
)

target_include_directories(reign_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Plugin library (shared/dynamic)
add_library(reign_plugins SHARED)

target_sources(reign_plugins
    PUBLIC
        FILE_SET CXX_MODULES FILES
            plugins/MassProperties.cppm
            plugins/ReactionWheel.cppm
            plugins/RocketPropulsion.cppm
            plugins/Battery.cppm
            plugins/DroneElectricPropulsion.cppm
            plugins/SpacecraftElectricPropulsion.cppm
)

target_link_libraries(reign_plugins
    PUBLIC
        reign_core
)

# Main executable
add_executable(reign_sim main.cpp)

target_link_libraries(reign_sim
    PRIVATE
        reign_core
        reign_plugins
)

# Drone example
add_executable(example_drone example_drone.cpp)

target_link_libraries(example_drone
    PRIVATE
        reign_core
        reign_plugins
)

# Spacecraft electric propulsion example
add_executable(example_spacecraft_electric example_spacecraft_electric.cpp)

target_link_libraries(example_spacecraft_electric
    PRIVATE
        reign_core
        reign_plugins
)
