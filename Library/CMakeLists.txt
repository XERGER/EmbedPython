# CMakeList.txt : CMake project for test, include source and define
# project specific logic here.
#
project(Library)

set(CMAKE_CXX_STANDARD 20 PARENT_SCOPE)
set(CMAKE_CXX_STANDARD_REQUIRED ON PARENT_SCOPE)


set(CMAKE_AUTOMOC ON PARENT_SCOPE)
set(CMAKE_AUTORCC ON PARENT_SCOPE)
set(CMAKE_AUTOUIC ON PARENT_SCOPE)
    
    find_package(Qt6 COMPONENTS Core Gui Sql REQUIRED)

add_library(${PROJECT_NAME} SHARED 
    global.h
    Python.cpp
    Python.h
    PythonForWin.cpp
    PythonForWin.h   
    PythonForLinux.cpp
    PythonForLinux.h
)

if (WIN32)
    target_compile_options(
        ${PROJECT_NAME}
        PRIVATE
        /bigobj
    )
endif()

target_link_libraries(
    ${PROJECT_NAME} PUBLIC
    Qt6::Core 
    Qt6::Gui
)

target_include_directories(${PROJECT_NAME} PRIVATE ${includeDirOut})

target_include_directories(
    ${PROJECT_NAME}
    PUBLIC
        ../
		
)

install(DIRECTORY ./
        DESTINATION include/${PROJECT_NAME}
        FILES_MATCHING
        PATTERN "*.h"
)

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
