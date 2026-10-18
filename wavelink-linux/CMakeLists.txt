cmake_minimum_required(VERSION 3.21)
project(wavelink-linux VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(_engine_default ON)
else()
  set(_engine_default OFF)
endif()
option(WAVELINK_BUILD_ENGINE "Build the PipeWire engine (Linux only)" ${_engine_default})
option(WAVELINK_BUILD_APP "Build the Qt Quick app" ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Network WebSockets)
qt_standard_project_setup()
include(CTest)

# Wave:3 USB control library (built without its CLI install rules).
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../wave3-linux ${CMAKE_CURRENT_BINARY_DIR}/wave3 EXCLUDE_FROM_ALL)

if(WAVELINK_BUILD_ENGINE)
  add_subdirectory(engine)
endif()
if(WAVELINK_BUILD_APP)
  add_subdirectory(app)
endif()
