cmake_minimum_required(VERSION 3.16)
project(SensorTreeApp VERSION 1.0.0)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find wxWidgets
find_package(wxWidgets REQUIRED COMPONENTS core base)
include(${wxWidgets_USE_FILE})

# Add executable
add_executable(${PROJECT_NAME}
    src/MainFrame.cpp
    src/App.cpp
)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    include
)

# Link libraries
target_link_libraries(${PROJECT_NAME} ${wxWidgets_LIBRARIES})

# Set output directory
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)