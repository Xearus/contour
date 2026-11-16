set(LIBDIR "${CONTOUR_UPSTREAM_DIR}/src")
set(LIBDIR_BIN "${CMAKE_CURRENT_BINARY_DIR}/upstream")
list(APPEND CMAKE_MODULE_PATH "${CONTOUR_UPSTREAM_DIR}/cmake/modules")
set(SLIC3R_GUI OFF)
set(SLIC3R_PCH OFF)
set(BBL_RELEASE_TO_PUBLIC 1)
set(BBL_INTERNAL_TESTING 0)
include("${CONTOUR_UPSTREAM_DIR}/version.inc")
set(SLIC3R_APP_NAME Contour)
set(SLIC3R_APP_KEY Contour)

find_package(Boost 1.83 REQUIRED COMPONENTS
    system filesystem thread log log_setup locale regex chrono atomic date_time iostreams)
find_package(Eigen3 3.3 REQUIRED NO_MODULE)
find_package(TBB REQUIRED)
find_package(CGAL REQUIRED)
find_package(cereal REQUIRED)
find_package(NLopt REQUIRED)
find_package(EXPAT REQUIRED)
find_package(PNG REQUIRED)
find_package(ZLIB REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(assimp REQUIRED)
find_package(OpenCASCADE REQUIRED)
find_package(Freetype REQUIRED)
find_package(Threads REQUIRED)

find_path(CONTOUR_NOISE_INCLUDE_DIR noise.h PATH_SUFFIXES libnoise REQUIRED)
find_library(CONTOUR_NOISE_LIBRARY NAMES noise REQUIRED)
find_path(CONTOUR_OPENCV_INCLUDE_DIR opencv2/core.hpp PATH_SUFFIXES opencv4 REQUIRED)
find_library(CONTOUR_OPENCV_CORE NAMES opencv_core REQUIRED)
find_library(CONTOUR_OPENCV_IMGPROC NAMES opencv_imgproc REQUIRED)
find_library(CONTOUR_OPENCV_IMGCODECS NAMES opencv_imgcodecs REQUIRED)

add_library(boost_headeronly INTERFACE)
target_link_libraries(boost_headeronly INTERFACE Boost::boost)
target_compile_definitions(boost_headeronly INTERFACE BOOST_LOG_DYN_LINK)
add_library(boost_libs INTERFACE)
target_link_libraries(boost_libs INTERFACE boost_headeronly
    Boost::system Boost::filesystem Boost::thread Boost::log Boost::log_setup
    Boost::locale Boost::regex Boost::chrono Boost::atomic Boost::date_time Boost::iostreams)

include_directories("${LIBDIR}" "${LIBDIR}/libigl" "${LIBDIR_BIN}"
    "${LIBDIR_BIN}/platform")
include_directories(SYSTEM ${EIGEN3_INCLUDE_DIR} ${CONTOUR_OPENCV_INCLUDE_DIR}
    ${CONTOUR_NOISE_INCLUDE_DIR} ${OpenCASCADE_INCLUDE_DIR})
add_compile_definitions(CONTOUR_HEADLESS USE_TBB TBB_USE_CAPTURED_EXCEPTION=0)
if(MSVC)
    add_compile_definitions(_USE_MATH_DEFINES NOMINMAX BOOST_ALL_NO_LIB)
    add_compile_options(/bigobj /utf-8)
else()
    add_compile_options(-fsigned-char)
endif()

foreach(dependency build-utils admesh boost clipper miniz glu-libtess qhull semver libigl mcut libnest2d)
    add_subdirectory("${LIBDIR}/${dependency}" "upstream/${dependency}")
endforeach()
add_subdirectory("${LIBDIR}/libslic3r" upstream/libslic3r)

add_library(contour_engine SHARED api.cpp)
target_include_directories(contour_engine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_definitions(contour_engine PRIVATE CONTOUR_ENGINE_BUILD)
target_link_libraries(contour_engine PRIVATE libslic3r)
set_target_properties(contour_engine PROPERTIES
    CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN YES
    VERSION 0.1.0 SOVERSION 0)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(contour_engine PRIVATE "LINKER:--exclude-libs,ALL")
endif()

add_executable(contour-slice main.cpp)
target_link_libraries(contour-slice PRIVATE contour_engine)

if(BUILD_TESTING)
    # Expose missing initialization instead of relying on zeroed stack pages.
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag("-ftrivial-auto-var-init=pattern" CONTOUR_HAS_PATTERN_INIT)
    if(CONTOUR_HAS_PATTERN_INIT)
        target_compile_options(contour_engine PRIVATE -ftrivial-auto-var-init=pattern)
    endif()
    add_executable(contour_api_tests test_api.c)
    target_link_libraries(contour_api_tests PRIVATE contour_engine)
    add_test(NAME contour.c-api COMMAND contour_api_tests
        "${CMAKE_CURRENT_BINARY_DIR}/missing-c-api-model.stl"
        "${CMAKE_CURRENT_BINARY_DIR}/unused-c-api-output.gcode")
    set_tests_properties(contour.c-api PROPERTIES TIMEOUT 60 LABELS "pipeline;api")
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_test(NAME contour.model-to-gcode
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_pipeline.py"
            "$<TARGET_FILE:contour-slice>")
    set_tests_properties(contour.model-to-gcode PROPERTIES TIMEOUT 180 LABELS "pipeline")
    add_test(NAME contour.library-model-to-gcode
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_pipeline.py"
            --library "$<TARGET_FILE:contour_engine>")
    set_tests_properties(contour.library-model-to-gcode PROPERTIES
        TIMEOUT 180 LABELS "pipeline;api"
        ENVIRONMENT "DISPLAY=;WAYLAND_DISPLAY=")
endif()
