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

# Build the project
./build.sh
```

### On Ubuntu/Debian
```bash
# Install dependencies
sudo apt-get install libwxgtk3.0-gtk3-dev cmake build-essential

# Build the project
./build.sh
```

## Running
After building, run the executable:
```bash
./build/bin/SensorTreeApp
```

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