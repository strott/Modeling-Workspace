#!/bin/bash

# Run script for Reign Spacecraft Plugin System

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== Reign Simulation Runner ===${NC}"
echo ""

# Default values
BUILD_TYPE="Release"
BUILD_DIR="build"
EXECUTABLE="reign_sim"

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -d|--debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        -h|--help)
            echo "Usage: ./run.sh [options] [-- simulation_args]"
            echo ""
            echo "Options:"
            echo "  -d, --debug    Run Debug build (default: Release)"
            echo "  -h, --help     Show this help message"
            echo ""
            echo "Examples:"
            echo "  ./run.sh                   # Run release build"
            echo "  ./run.sh -d                # Run debug build"
            echo "  ./run.sh -- --config file.json  # Pass args to simulation"
            exit 0
            ;;
        --)
            shift
            break
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            echo "Use -h or --help for usage information"
            exit 1
            ;;
    esac
done

# Check if build directory exists
if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${RED}ERROR: Build directory not found.${NC}"
    echo "Please run ./build.sh first to build the project."
    exit 1
fi

# Find executable
EXEC_PATH=""

# Check common locations
POSSIBLE_PATHS=(
    "$BUILD_DIR/bin/$EXECUTABLE"
    "$BUILD_DIR/bin/$BUILD_TYPE/$EXECUTABLE"
    "$BUILD_DIR/src/$EXECUTABLE"
)

for path in "${POSSIBLE_PATHS[@]}"; do
    if [ -f "$path" ]; then
        EXEC_PATH="$path"
        break
    fi
done

if [ -z "$EXEC_PATH" ]; then
    echo -e "${RED}ERROR: Executable not found.${NC}"
    echo "Searched in:"
    for path in "${POSSIBLE_PATHS[@]}"; do
        echo "  - $path"
    done
    echo ""
    echo "Please build the project first with ./build.sh"
    exit 1
fi

echo -e "Executable: ${YELLOW}${EXEC_PATH}${NC}"
echo -e "Build Type: ${YELLOW}${BUILD_TYPE}${NC}"
echo ""

# Set LD_LIBRARY_PATH for shared libraries
export LD_LIBRARY_PATH="$BUILD_DIR/lib:$LD_LIBRARY_PATH"

# Run the simulation
echo -e "${GREEN}Starting simulation...${NC}"
echo "==============================================="
echo ""

"$EXEC_PATH" "$@"
EXIT_CODE=$?

echo ""
echo "==============================================="

if [ $EXIT_CODE -eq 0 ]; then
    echo -e "${GREEN}Simulation completed successfully!${NC}"
else
    echo -e "${RED}Simulation exited with code: $EXIT_CODE${NC}"
fi

exit $EXIT_CODE
