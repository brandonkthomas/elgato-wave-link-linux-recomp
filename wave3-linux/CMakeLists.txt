cmake_minimum_required(VERSION 3.16)
project(wave3-linux VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSB REQUIRED IMPORTED_TARGET libusb-1.0)

# The protocol tables and codec have no USB dependency so they can be unit-tested anywhere.
add_library(wave3proto STATIC src/schema.cpp src/codec.cpp)
target_include_directories(wave3proto PUBLIC include)
target_compile_options(wave3proto PRIVATE -Wall -Wextra)

add_library(wave3 STATIC src/transport_libusb.cpp src/device.cpp)
target_link_libraries(wave3 PUBLIC wave3proto PkgConfig::LIBUSB)
target_compile_options(wave3 PRIVATE -Wall -Wextra)

# The CLI, tests and install rules only apply when this is the top-level project, not when it
# is embedded (e.g. by wavelink-linux).
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
add_executable(wave3ctl tools/wave3ctl.cpp)
target_link_libraries(wave3ctl PRIVATE wave3)
target_compile_options(wave3ctl PRIVATE -Wall -Wextra)

include(CTest)
if(BUILD_TESTING)
  add_executable(test_protocol tests/test_protocol.cpp)
  target_link_libraries(test_protocol PRIVATE wave3proto)
  add_test(NAME protocol COMMAND test_protocol)
endif()

include(GNUInstallDirs)
# udev does not read rules under /usr/local, so the rule goes to /etc unless a package overrides it.
set(WAVE3_UDEV_RULES_DIR "/etc/udev/rules.d" CACHE PATH "Where to install the udev rule")
install(TARGETS wave3ctl RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES packaging/70-elgato-wave3.rules DESTINATION ${WAVE3_UDEV_RULES_DIR})
endif()
