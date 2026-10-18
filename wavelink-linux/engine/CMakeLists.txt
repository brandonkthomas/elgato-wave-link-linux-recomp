find_package(PkgConfig REQUIRED)
pkg_check_modules(PIPEWIRE REQUIRED IMPORTED_TARGET libpipewire-0.3)

add_library(wavelink_model STATIC src/Model.cpp src/MixerCore.cpp src/effects/ElgatoEq.cpp src/effects/LadspaEffect.cpp
            src/effects/Registry.cpp)
target_include_directories(wavelink_model PUBLIC src third_party/ladspa)
target_link_libraries(wavelink_model PUBLIC ${CMAKE_DL_LIBS})
target_link_libraries(wavelink_model PUBLIC Qt6::Core)

add_executable(wavelinkd
  src/main.cpp
  src/Engine.cpp
  src/PwGraph.cpp
  src/ApiServer.cpp
  src/Wave3Service.cpp
)
target_link_libraries(wavelinkd PRIVATE wavelink_model Qt6::Network Qt6::WebSockets PkgConfig::PIPEWIRE wave3)
# PipeWire event tables use designated initializers that leave optional callbacks unset.
target_compile_options(wavelinkd PRIVATE -Wall -Wextra -Wno-missing-field-initializers)


if(BUILD_TESTING)
  add_executable(test_model tests/test_model.cpp)
  target_link_libraries(test_model PRIVATE wavelink_model)
  add_test(NAME model COMMAND test_model)
  add_executable(test_eq tests/test_eq.cpp)
  target_link_libraries(test_eq PRIVATE wavelink_model)
  add_test(NAME eq COMMAND test_eq)
  add_executable(fx_probe tests/fx_probe.cpp)
  target_link_libraries(fx_probe PRIVATE wavelink_model)
endif()

install(TARGETS wavelinkd RUNTIME DESTINATION bin)
