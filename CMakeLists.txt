cmake_minimum_required(VERSION 3.27)
project(EmbedPython)

include("${PROJECT_SOURCE_DIR}/../CMake/project.cmake")

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(MODULE "MODULE" CACHE STRING "All")

if( MODULE )
    if(NOT "${MODULE}" STREQUAL "All")
		add_subdirectory(${MODULE})
    else()
		add_subdirectory(Library)
		add_subdirectory(Executer)
		add_subdirectory(Client)
		add_subdirectory(Test)
		set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Test)
    endif()
endif()




