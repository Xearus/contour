cmake_minimum_required(VERSION 3.21)
project(ContourEngineBaseline VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
option(CONTOUR_ENABLE_SANITIZERS "Instrument baseline components with ASan and UBSan" OFF)
option(CONTOUR_ENABLE_COVERAGE "Instrument baseline components for gcov" OFF)

if(CONTOUR_ENABLE_SANITIZERS AND CONTOUR_ENABLE_COVERAGE)
    message(FATAL_ERROR "Use separate build directories for sanitizers and coverage.")
endif()

add_library(contour_instrumentation INTERFACE)
if(CONTOUR_ENABLE_SANITIZERS)
    if(MSVC OR NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(FATAL_ERROR "This sanitizer configuration requires GCC or Clang.")
    endif()
    target_compile_options(contour_instrumentation INTERFACE
        -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(contour_instrumentation INTERFACE -fsanitize=address,undefined)
elseif(CONTOUR_ENABLE_COVERAGE)
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        message(FATAL_ERROR "The gcov configuration requires GCC.")
    endif()
    target_compile_options(contour_instrumentation INTERFACE --coverage -O0 -g)
    target_link_options(contour_instrumentation INTERFACE --coverage)
endif()

get_filename_component(CONTOUR_UPSTREAM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
add_subdirectory("${CONTOUR_UPSTREAM_DIR}/src/clipper2" upstream/clipper2)
target_link_libraries(Clipper2 PRIVATE contour_instrumentation)

set(CONTOUR_CHECKER_DIR "${CONTOUR_UPSTREAM_DIR}/bbs_test_tools/bbs_gcode_checker")
add_library(contour_gcode_checker STATIC "${CONTOUR_CHECKER_DIR}/GCodeChecker.cpp")
target_include_directories(contour_gcode_checker PUBLIC "${CONTOUR_CHECKER_DIR}")
target_link_libraries(contour_gcode_checker PRIVATE contour_instrumentation)

add_executable(contour-check-gcode "${CONTOUR_CHECKER_DIR}/main.cpp")
target_link_libraries(contour-check-gcode PRIVATE contour_gcode_checker contour_instrumentation)

if(BUILD_TESTING)
    add_executable(contour_engine_tests
        tests/main.cpp
        tests/geometry.cpp
        tests/gcode.cpp)
    target_include_directories(contour_engine_tests PRIVATE "${CONTOUR_UPSTREAM_DIR}/tests")
    target_compile_definitions(contour_engine_tests PRIVATE
        CATCH_CONFIG_NO_POSIX_SIGNALS
        CONTOUR_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
    target_link_libraries(contour_engine_tests PRIVATE
        Clipper2 contour_gcode_checker contour_instrumentation)

    foreach(suite geometry gcode known-limitations)
        add_test(NAME "contour.${suite}" COMMAND contour_engine_tests "[${suite}]")
        set_tests_properties("contour.${suite}" PROPERTIES TIMEOUT 60 LABELS "${suite}")
    endforeach()

    add_test(NAME contour.checker-cli
        COMMAND contour-check-gcode "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/line.gcode")
endif()
