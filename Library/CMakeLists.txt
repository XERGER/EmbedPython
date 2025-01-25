cmake_minimum_required(VERSION 3.16)
# Set policies for modern CMake
if(POLICY CMP0003)
    cmake_policy(SET CMP0003 NEW)
endif()

if(POLICY CMP0011)
    cmake_policy(SET CMP0011 NEW)
endif()

if(POLICY CMP0148)
    cmake_policy(SET CMP0148 NEW)
endif()

project(Library LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable Qt's automatic tools
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Optional: Enable verbose automoc for debugging
# set(CMAKE_AUTOMOC_VERBOSE ON)

# Find Qt6 packages
find_package(Qt6 COMPONENTS Core Concurrent REQUIRED)

# Find other Qt6 modules as needed
# Example:
# find_package(Qt6 COMPONENTS Widgets REQUIRED)

# Find Python3 packages
#find_package(Python3 COMPONENTS Interpreter Development REQUIRED)

# Include directories for Python3
#include_directories(${Python3_INCLUDE_DIRS})

#qt_add_resources(${PROJECT_NAME}_RESOURCES resources.qrc)

# Add the library
add_library(${PROJECT_NAME} SHARED
    global.h
    PythonEnvironment.cpp
    PythonEnvironment.h
    PythonResult.cpp
    PythonResult.h
    PythonRunner.cpp
    PythonRunner.h   
    PythonSyntaxCheck.h   
    PythonSyntaxCheck.cpp   
    #WorkerPool.cpp
    #WorkerPool.h
    #DataConverter.cpp
    #DataConverter.h
#    resources.qrc
)


# Add compile options for Windows
if (WIN32)
    target_compile_options(
        ${PROJECT_NAME}
        PRIVATE
        /bigobj
    )
endif()

# Link libraries
target_link_libraries(
    ${PROJECT_NAME} PUBLIC
    Qt6::Core
    Qt6::Concurrent
    PRIVATE
    #${Python3_LIBRARIES}
)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE ${includeDirOut})

target_include_directories(
    ${PROJECT_NAME}
    PUBLIC
        ../
        ${Python3_INCLUDE_DIRS}
)

# Install headers
install(DIRECTORY ./ 
    DESTINATION include/${PROJECT_NAME}
    FILES_MATCHING
    PATTERN "*.h"
)

# Install targets using generator expressions
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION $<IF:$<CONFIG:Debug>,Debug/bin,Release/bin>
    LIBRARY DESTINATION $<IF:$<CONFIG:Debug>,Debug/lib,Release/lib>
    ARCHIVE DESTINATION $<IF:$<CONFIG:Debug>,Debug/lib,Release/lib>
    CONFIGURATIONS Debug Release
)