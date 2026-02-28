# Installation Guide

This project uses **CMake** for building and **Conan** for dependency management.

---

## Requirements

You must have the following installed:

-   **CMake ≥ 3.15**
-   **Conan ≥ 2.x**
-   **C++ compiler** with C++17 support (GCC or Clang)
-   **SDL2 development package**

### Install system dependencies

#### Fedora

```bash
sudo dnf install cmake gcc-c++ SDL2-devel
```

#### Ubuntu / Debian

```bash
sudo apt install cmake g++ libsdl2-dev
```

---

## Build Instructions

From the project root directory:

### 1. Install dependencies with Conan

```bash
conan install . \
  --output-folder=build \
  --build=missing \
  -s build_type=Release
```

This will download dependencies (Boost) and generate CMake configuration files.

---

### 2. Configure the project with CMake

```bash
cmake -S . -B build \
  -DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake \
  -DCMAKE_BUILD_TYPE=Release
```

---

### 3. Build the project

```bash
cmake --build build -j
```

---

## Run

```bash
./build/relay
```

---

## Notes

-   SDL2 is provided by the system (not Conan).
-   If Boost is not found, ensure Conan was run before CMake.
-   If you change dependencies, delete the `build/` folder and repeat the steps.

---

## Clean Build (optional)

```bash
rm -rf build
```

Then repeat the build steps above.
