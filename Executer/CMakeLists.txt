# Executable/CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

project(PythonEngine LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable Qt's automatic tools
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find Qt6 packages
find_package(Qt6 COMPONENTS Core Network REQUIRED)


# Add the executable
add_executable(${PROJECT_NAME}
    main.cpp
    server.h
    server.cpp
 
    # Add other source files as needed
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
    ${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Network
    Library
    PythonClient
)

# Conditional installation based on build type
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Configuring Debug installation")
    install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION Debug/bin
        LIBRARY DESTINATION Debug/lib
        ARCHIVE DESTINATION Debug/lib
    )
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Configuring Release installation")
    install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION Release/bin
        LIBRARY DESTINATION Release/lib
        ARCHIVE DESTINATION Release/lib
    )
else()
    message(WARNING "Unknown build type '${CMAKE_BUILD_TYPE}'. No installation performed for Debug or Release.")
endif()