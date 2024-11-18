# CMakeList.txt : CMake project for test, include source and define
# project specific logic here.
#
project(Test)

set(CMAKE_CXX_STANDARD 20 PARENT_SCOPE)
set(CMAKE_CXX_STANDARD_REQUIRED ON PARENT_SCOPE)

set(CMAKE_AUTOMOC ON PARENT_SCOPE)
set(CMAKE_AUTORCC ON PARENT_SCOPE)
set(CMAKE_AUTOUIC ON PARENT_SCOPE)

find_package(GTest CONFIG REQUIRED)
find_package(Qt6 COMPONENTS Core Test REQUIRED)

include_directories(${CMAKE_INSTALL_PREFIX}/include)

add_executable(${PROJECT_NAME}
    test.cpp
    #Test/Python.cpp
    #Test/PythonEdgeCases.cpp
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
    Qt6::Test
    GTest::gtest
    GTest::gtest_main
 )

target_include_directories(
    ${PROJECT_NAME}
    PUBLIC
        include
)

include(GoogleTest)

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION Debug/bin
    LIBRARY DESTINATION Debug/lib
    ARCHIVE DESTINATION Debug/lib
    CONFIGURATIONS Debug
)

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION Release/bin
    LIBRARY DESTINATION Release/lib
    ARCHIVE DESTINATION Release/lib
    CONFIGURATIONS Release
)
