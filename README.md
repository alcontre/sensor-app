# Sensor Tree Viewer

A C++ wxWidgets application for displaying hierarchical sensor data.

## Features
- Hierarchical tree view of sensors
- Real-time data display (voltage, temperature, current)
- Multi-threaded data generation
- Cross-platform GUI

## Building

### Prerequisites
- CMake 3.16 or higher
- wxWidgets development libraries
- C++17 compatible compiler

### On macOS
```bash
# Install wxWidgets using Homebrew
brew install wxwidgets

# Fetch vendored dependencies
git submodule update --init --recursive

# Build the project
./build.sh
```

### On Ubuntu/Debian
```bash
# Install dependencies
sudo apt-get install libwxgtk3.0-gtk3-dev cmake build-essential

# Fetch vendored dependencies
git submodule update --init --recursive

# Build the project
./build.sh
```

If you cloned the repository without submodules, run `git submodule update --init --recursive`
before building.

## Running
After building, run the executable:
```bash
./build/bin/SensorTreeApp
```

## Tests
The repository includes a lightweight non-GUI maintenance test target for serialization,
path utilities, and tree-model visibility behavior.

Build and run it with:
```bash
cmake --build build --target SensorTreeMaintenanceTests
ctest --test-dir build -R SensorTreeMaintenanceTests --output-on-failure
```

## Recorded Data
Generated sensor recordings use a canonical JSON schema with required
`elapsed_seconds`, `local_time`, `path`, and `value` fields.
The `status` field is optional: recordings omit it for the common `ok` case and
emit it for `warn` or `failed` samples.
The loader expects files written by this version of the app and defaults missing
`status` to `ok`.

## Project Structure
```
├── CMakeLists.txt      # CMake configuration
├── build.sh           # Build script
├── include/           # Header files
│   ├── App.h
│   └── MainFrame.h
└── src/              # Source files
    ├── main.cpp
    ├── App.cpp
    └── MainFrame.cpp
```