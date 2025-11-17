#!/bin/bash

# Build script for Reign Spacecraft Plugin System
# Supports Linux, macOS, and Windows (via MSYS2/Git Bash)

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== Reign Build System ===${NC}"
echo ""

# Detect OS
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    OS="Linux"
elif [[ "$OSTYPE" == "darwin"* ]]; then
    OS="macOS"
elif [[ "$OSTYPE" == "msys" ]] || [[ "$OSTYPE" == "win32" ]]; then
    OS="Windows"
else
    OS="Unknown"
fi

echo -e "Operating System: ${YELLOW}${OS}${NC}"

# Parse command line arguments
BUILD_TYPE="Release"
CLEAN=false
VERBOSE=false
RUN_TESTS=false

while [[ $# -gt 0 ]]; do
    case $1 in
        -d|--debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        -c|--clean)
            CLEAN=true
            shift
            ;;
        -v|--verbose)
            VERBOSE=true
            shift
            ;;
        -t|--test)
            RUN_TESTS=true
            shift
            ;;
        -h|--help)
            echo "Usage: ./build.sh [options]"
            echo ""
            echo "Options:"
            echo "  -d, --debug    Build in Debug mode (default: Release)"
            echo "  -c, --clean    Clean build directory before building"
            echo "  -v, --verbose  Verbose build output"
            echo "  -t, --test     Run tests after building"
            echo "  -h, --help     Show this help message"
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            echo "Use -h or --help for usage information"
            exit 1
            ;;
    esac
done

echo -e "Build Type: ${YELLOW}${BUILD_TYPE}${NC}"
echo ""

# Check for CMake
if ! command -v cmake &> /dev/null; then
    echo -e "${RED}ERROR: CMake not found. Please install CMake 3.28 or later.${NC}"
    exit 1
fi

CMAKE_VERSION=$(cmake --version | head -n1 | cut -d' ' -f3)
echo "CMake Version: $CMAKE_VERSION"

# Check for CUDA (optional)
if command -v nvcc &> /dev/null; then
    NVCC_VERSION=$(nvcc --version | grep "release" | sed -n 's/.*release \([0-9.]*\).*/\1/p')
    echo -e "CUDA Version: ${GREEN}${NVCC_VERSION}${NC}"
    HAS_CUDA=true
else
    echo -e "${YELLOW}WARNING: CUDA not found. Building without GPU support.${NC}"
    HAS_CUDA=false
fi

echo ""

# Create build directory
BUILD_DIR="build"

if [ "$CLEAN" = true ]; then
    echo -e "${YELLOW}Cleaning build directory...${NC}"
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure with CMake
echo -e "${GREEN}Configuring with CMake...${NC}"
CMAKE_ARGS="-DCMAKE_BUILD_TYPE=$BUILD_TYPE"

if [ "$VERBOSE" = true ]; then
    CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_VERBOSE_MAKEFILE=ON"
fi

# Handle different platforms
if [[ "$OS" == "Windows" ]]; then
    # Use Ninja or NMake on Windows
    if command -v ninja &> /dev/null; then
        CMAKE_ARGS="$CMAKE_ARGS -G Ninja"
    fi
fi

cmake .. $CMAKE_ARGS

if [ $? -ne 0 ]; then
    echo -e "${RED}ERROR: CMake configuration failed${NC}"
    exit 1
fi

echo ""

# Build
echo -e "${GREEN}Building...${NC}"
CPU_COUNT=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
echo "Using $CPU_COUNT parallel jobs"
echo ""

if [ "$VERBOSE" = true ]; then
    cmake --build . --config $BUILD_TYPE -- -j$CPU_COUNT
else
    cmake --build . --config $BUILD_TYPE -- -j$CPU_COUNT 2>&1 | grep -E "error|warning|Built target"
fi

if [ $? -ne 0 ]; then
    echo -e "${RED}ERROR: Build failed${NC}"
    exit 1
fi

echo ""
echo -e "${GREEN}Build completed successfully!${NC}"
echo ""

# Run tests if requested
if [ "$RUN_TESTS" = true ]; then
    echo -e "${GREEN}Running tests...${NC}"
    ctest --output-on-failure -C $BUILD_TYPE
    if [ $? -ne 0 ]; then
        echo -e "${RED}ERROR: Some tests failed${NC}"
        exit 1
    fi
    echo ""
fi

# Show output location
echo -e "Executables location: ${YELLOW}$(pwd)/bin${NC}"
echo -e "Libraries location: ${YELLOW}$(pwd)/lib${NC}"
echo ""
echo -e "${GREEN}To run the simulation, use: ./run.sh${NC}"
