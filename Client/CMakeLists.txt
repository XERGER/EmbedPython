
project(PythonClient LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable Qt's automatic tools
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)


# Find Qt6 packages
find_package(Qt6 COMPONENTS Core Network REQUIRED)

find_package(cryptopp CONFIG REQUIRED)

include_directories(${CMAKE_INSTALL_PREFIX}/include)

# Find the Crypto++ include directory
find_path(CRYPTOPP_CPP_INCLUDE_DIRS "cryptopp/")

if(NOT CRYPTOPP_CPP_INCLUDE_DIRS)
    message(FATAL_ERROR "Crypto++ include directory not found")
else()
    message(STATUS "Crypto++ include directory: ${CRYPTOPP_CPP_INCLUDE_DIRS}")
endif()


add_library(${PROJECT_NAME} SHARED
    global.h
    PythonClient.cpp
    PythonClient.h  
    PythonResult.cpp
    PythonResult.h
    ServerController.cpp
    ServerController.h
    encryption.cpp
    encryption.h
)

# Include directories
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(${PROJECT_NAME} PRIVATE "${CRYPTOPP_CPP_INCLUDE_DIRS}/")

target_link_libraries(${PROJECT_NAME}  
PRIVATE    
   Qt6::Core
    Qt6::Network
    cryptopp::cryptopp
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