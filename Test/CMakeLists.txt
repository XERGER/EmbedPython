# CMakeList.txt : CMake project for test, include source and define
# project specific logic here.
#
project(Test)

set(CMAKE_CXX_STANDARD 20 PARENT_SCOPE)
set(CMAKE_CXX_STANDARD_REQUIRED ON PARENT_SCOPE)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(GTest CONFIG REQUIRED)
find_package(Qt6 COMPONENTS Core Network Test REQUIRED)

# Add include directories and link the library
add_executable(${PROJECT_NAME}
    test.cpp
  # Test/Python.cpp
  # Test/PythonEdgeCases.cpp
   Test/ClientTest.cpp
   Test/PythonPackages.cpp
)

if (WIN32)
    target_compile_options(
        ${PROJECT_NAME}
        PRIVATE
        /bigobj
    )
endif()

target_link_libraries(
    ${PROJECT_NAME} 
    PUBLIC
    Library
    Qt6::Core
    Qt6::Network
    Qt6::Test
    GTest::gtest
    GTest::gtest_main

    PRIVATE
    PythonClient

 )

target_include_directories(
    ${PROJECT_NAME}
    PUBLIC
        include
)

include(GoogleTest)

# Install targets using generator expressions
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION $<IF:$<CONFIG:Debug>,Debug/bin,Release/bin>
    LIBRARY DESTINATION $<IF:$<CONFIG:Debug>,Debug/lib,Release/lib>
    ARCHIVE DESTINATION $<IF:$<CONFIG:Debug>,Debug/lib,Release/lib>
    CONFIGURATIONS Debug Release
)