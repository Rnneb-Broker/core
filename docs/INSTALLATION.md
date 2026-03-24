# Installation Guide - RabbitBroker

Complete instructions for building and installing RabbitBroker from source.

## Table of Contents

- [System Requirements](#system-requirements)
- [Quick Install](#quick-install)
- [Detailed Installation](#detailed-installation)
- [Dependency Installation](#dependency-installation)
- [Build Options](#build-options)
- [Troubleshooting](#troubleshooting)
- [Verification](#verification)
- [Docker Installation](#docker-installation)

## System Requirements

### Minimum Requirements

- **CPU**: 2 cores
- **RAM**: 256 MB
- **Disk**: 500 MB (for build artifacts + test data)
- **Network**: TCP/IP stack

### Compiler Requirements

Choose one of the following C++17 compatible compilers:

| Compiler | Version | Status |
|----------|---------|--------|
| GCC | 7.0+ |  Tested |
| Clang | 5.0+ |  Tested |
| MSVC | 2017+ |  Supported |

### Build Tools

```bash
# Required
cmake >= 3.15          # CMake build system
git                    # Version control
conan >= 2.0           # Package manager

# Optional
make or ninja          # Build generator
clang-format           # Code formatting
clang-tidy             # Static analysis
```

### Supported Operating Systems

- **Linux**: Ubuntu 18.04+, Debian 10+, CentOS 7+, Fedora 30+
- **macOS**: 10.13+
- **Windows**: Windows 10/11 with MSVC 2017+

## Quick Install

For the impatient, here's the 5-minute setup:

```bash
# 1. Clone repository
git clone https://github.com/yourusername/rabbit-broker.git
cd rabbit-broker

# 2. Create build directory
mkdir build && cd build

# 3. Install dependencies and build
conan install .. --build=missing
cmake .. && make -j$(nproc)

# 4. Run tests
ctest

# 5. Start broker
./broker

# In another terminal:
./sensor_simulator 127.0.0.1 1883 10
```

That's it! See [QUICKSTART.md](QUICKSTART.md) for next steps.

## Detailed Installation

### Step 1: Install Prerequisites

#### Ubuntu/Debian

```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    git \
    python3 \
    python3-pip

# Install Conan package manager
pip3 install conan
```

#### macOS (using Homebrew)

```bash
# Install Xcode command line tools
xcode-select --install

# Install build tools
brew install cmake git python3

# Install Conan
pip3 install conan
```

#### CentOS/RHEL

```bash
sudo yum groupinstall -y "Development Tools"
sudo yum install -y \
    cmake3 \
    git \
    python3 \
    python3-pip

# Create symlink for cmake3
sudo ln -s /usr/bin/cmake3 /usr/bin/cmake

# Install Conan
pip3 install conan
```

#### Windows

1. Install [Visual Studio Community](https://visualstudio.microsoft.com/community/)
   - Select "Desktop development with C++"
   - Select CMake tools
   - Select Windows 10 SDK

2. Install Git: https://git-scm.com/download/win

3. Install Python 3: https://www.python.org/downloads/

4. Install CMake: https://cmake.org/download/

5. Install Conan:
   ```cmd
   pip install conan
   ```

### Step 2: Clone Repository

```bash
git clone https://github.com/yourusername/rabbit-broker.git
cd rabbit-broker
git checkout main  # or your preferred branch
```

### Step 3: Install Conan Dependencies

```bash
cd rabbit-broker
mkdir build
cd build

# Install dependencies from conanfile.txt
conan install .. --build=missing

# On first run, Conan will:
# 1. Download dependency recipes
# 2. Determine compatible versions
# 3. Download pre-built binaries (or build if needed)
# 4. Generate CMake integration files
```

**Expected output:**
```
conan profile detected: /home/user/.conan2/profiles/default
Downloading conanfile.py: boost/1.84.0
Downloading conanfile.py: gtest/1.14.0
...
conanfile.txt: Calling generate()
conanfile.txt: Aggregating env generators
conanfile.txt: Generators folder: /path/to/build
```

### Step 4: Configure with CMake

```bash
# From build directory
cmake ..

# Or with options:
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_COMPILER=g++ \
      ..
```

**Generated files:**
```
build/
├── CMakeCache.txt          # Configuration cache
├── conanbuild.cmake        # Conan integration
├── conaninfo.cmake         # Dependency info
├── Makefile                # Generated build file
└── compile_commands.json   # IDE integration
```

### Step 5: Build

```bash
# With make (default)
make -j$(nproc)    # Use all CPU cores
make -j4           # Use 4 cores
make               # Use single core

# With ninja (faster)
ninja -j$(nproc)

# CMake build command (platform independent)
cmake --build . -j 4
```

**Build artifacts:**
```
build/
├── broker                  # Broker executable
├── sensor_simulator        # Example: sensor publisher
├── traffic_monitor         # Example: consumer
├── storage_manager_test    # Unit tests
└── lib/                    # Static libraries
    ├── librabbit_core.a
    └── librabbit_client.a
```

### Step 6: Verify Installation

```bash
# Run unit tests
ctest --verbose

# Test individual executable
./broker --help   # If help is implemented, or:
./broker          # Start broker in one terminal

# In another terminal, test with sensor simulator
./sensor_simulator 127.0.0.1 1883 5
```

## Dependency Installation

### Manual Boost Installation (Alternative to Conan)

If you prefer to manage Boost manually:

```bash
# Ubuntu/Debian
sudo apt-get install libboost-system-dev libboost-dev

# macOS
brew install boost

# CentOS/RHEL
sudo yum install boost-devel

# Build from source (if needed)
cd ~
wget https://boostorg.jfrog.io/artifactory/main/release/1.84.0/source/boost_1_84_0.tar.gz
tar -xzf boost_1_84_0.tar.gz
cd boost_1_84_0
./bootstrap.sh --prefix=$HOME/boost_install
./b2 --prefix=$HOME/boost_install link=shared variant=release install

# Then pass to CMake:
cmake .. -DBOOST_ROOT=$HOME/boost_install
```

### Manual GTest Installation (Alternative to Conan)

```bash
# Ubuntu/Debian
sudo apt-get install libgtest-dev

# Build and install
cd /usr/src/gtest
sudo cmake .
sudo make
sudo cp lib/libgtest* /usr/lib/

# Or from source
git clone https://github.com/google/googletest.git
cd googletest
mkdir build && cd build
cmake ..
make
sudo make install
```

## Build Options

### CMake Configuration Variables

```bash
# Build type
-DCMAKE_BUILD_TYPE=Release    # Optimized (default)
-DCMAKE_BUILD_TYPE=Debug      # With symbols, no optimization

# Compiler
-DCMAKE_CXX_COMPILER=clang++  # Use Clang
-DCMAKE_CXX_COMPILER=g++      # Use GCC
-DCMAKE_CXX_FLAGS="-march=native -O3"  # Custom flags

# Installation
-DCMAKE_INSTALL_PREFIX=/opt/rabbit  # Install location

# Code analysis
-DENABLE_CLANG_TIDY=ON        # Run clang-tidy checks
-DENABLE_CPPCHECK=ON          # Run cppcheck
```

### Common Build Configurations

**Release (Optimized)**:
```bash
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ ..
make -j$(nproc)
```

**Debug (Development)**:
```bash
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_CLANG_TIDY=ON ..
make -j4
```

**Static Linking**:
```bash
cmake -DBUILD_SHARED_LIBS=OFF ..
make
```

**With GCC and Custom Optimization**:
```bash
cmake -DCMAKE_CXX_COMPILER=g++ \
      -DCMAKE_CXX_FLAGS="-march=native -O3 -flto" \
      -DCMAKE_BUILD_TYPE=Release ..
make
```

## Troubleshooting

### Build Issues

#### Error: "cmake not found"

```bash
# Install CMake
# Ubuntu/Debian
sudo apt-get install cmake

# macOS
brew install cmake

# Windows: Download from https://cmake.org/download/
```

#### Error: "conan: command not found"

```bash
# Reinstall Conan
pip3 install --upgrade conan

# Verify installation
conan --version
```

#### Error: "Boost not found"

```bash
# Option 1: Install via system package manager
# Ubuntu/Debian
sudo apt-get install libboost-all-dev

# Option 2: Use Conan (automatic)
cd build
conan install .. --build=missing
```

#### Error: "Multiple definitions of __gtest_main"

```bash
# GTest linking error - remove from CMakeLists.txt or:
cmake -DENABLE_TESTING=OFF ..
```

#### Compilation errors with newer C++ standards

```bash
# Ensure C++17 is used
cmake -DCMAKE_CXX_STANDARD=17 -DCMAKE_CXX_STANDARD_REQUIRED=ON ..
```

### Runtime Issues

#### Port 1883 already in use

```bash
# Find process using port
lsof -i :1883          # macOS/Linux
netstat -ano | grep 1883  # Windows

# Use different port (if broker supports it)
./broker --port 1884

# Or kill existing process
kill -9 <PID>
```

#### "Permission denied" when running broker

```bash
# Broker needs to bind to port < 1024
sudo ./broker

# Or use port >= 1024
./broker --port 8883
```

#### Connection refused errors

```bash
# Check if broker is running
ps aux | grep broker

# Check if listening on correct port
netstat -tlnp | grep 1883

# Firewall might be blocking - check:
sudo ufw status          # Ubuntu
sudo firewall-cmd --list-all  # CentOS
```

### Performance Issues

#### Build takes too long

```bash
# Use more cores (if available)
make -j$(nproc)  # Use all cores
make -j8         # Use 8 cores

# Or use Ninja (usually faster)
cmake -GNinja ..
ninja -j$(nproc)

# Or use ccache for incremental builds
cmake -DCMAKE_CXX_COMPILER_LAUNCHER=ccache ..
```

#### Broker runs slowly

```bash
# Ensure building Release, not Debug
cmake -DCMAKE_BUILD_TYPE=Release ..
make

# Check CPU usage
top
htop  # If installed

# Check for I/O bottlenecks
iostat -x 1
```

### Platform-Specific Issues

#### macOS: "Undefined symbol" errors

```bash
# Update Xcode
xcode-select --install

# Reinstall dependencies
rm -rf build
mkdir build && cd build
conan install .. --build=missing
cmake ..
make -j$(nproc)
```

#### Windows: CMake can't find compiler

```cmd
# Run from Visual Studio Developer Command Prompt:
# "x64 Native Tools Command Prompt for VS 20XX"
# Then run cmake commands

# Or specify compiler explicitly:
cmake -G "Visual Studio 16 2019" ..
cmake --build . --config Release
```

#### Linux: Symbol not found for architecture

```bash
# Ensure compatible architecture
file ./broker  # Check if 64-bit or 32-bit

# Match with available libraries
apt search boost | grep amd64  # Check available
```

## Verification

### Verify Installation

```bash
# Check broker runs
./broker &
sleep 1
ps aux | grep broker

# Check broker responds to connections
echo "QUIT" | nc localhost 1883

# Run unit tests
cd build
ctest --verbose

# Expected output:
# Test project /path/to/build
# Start 1: StorageManagerTests
# 1/1 Test #1: StorageManagerTests ....... Passed    1.23 sec
# 100% tests passed
```

### Verify Examples

```bash
# Terminal 1: Start broker
./broker

# Terminal 2: Start sensor (generates data)
./sensor_simulator 127.0.0.1 1883 5

# Terminal 3: Start monitor (consumes data)
./traffic_monitor 127.0.0.1 1883

# Expected output in Terminal 3:
# Connected to broker
# [MONITOR] Speed statistics...
# [MONITOR] Slow traffic detected!
```

## Docker Installation

### Build Docker Image

```dockerfile
FROM ubuntu:20.04

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    python3-pip

RUN pip3 install conan

WORKDIR /rabbit
COPY . .

RUN mkdir build && cd build && \
    conan install .. --build=missing && \
    cmake .. && \
    make -j$(nproc)

EXPOSE 1883
CMD ["./build/broker"]
```

### Run with Docker

```bash
# Build image
docker build -t rabbit-broker .

# Run container
docker run -d \
    --name broker \
    -p 1883:1883 \
    rabbit-broker

# Check logs
docker logs broker

# Stop container
docker stop broker
```

## Next Steps

1. Read [QUICKSTART.md](QUICKSTART.md) for first run instructions
2. Check [USAGE_EXAMPLES.md](USAGE_EXAMPLES.md) for code samples
3. Review [ARCHITECTURE.md](docs/ARCHITECTURE.md) for system design
4. See [CONTRIBUTING.md](CONTRIBUTING.md) to contribute

## Getting Help

- **Installation help**: Open issue with tag `installation`
- **Build errors**: Include full error output and environment info
- **Questions**: Post in GitHub Discussions

---

**Happy building! **
