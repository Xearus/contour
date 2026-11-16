# Keep the upstream source manifest; exclude resin-only processing and the desktop log sink.
list(FILTER lisbslic3r_sources EXCLUDE REGEX "^(SLA/|SLAPrint|Format/SL1\\.|LogSink\\.)")
get_target_property(cgal_target CGAL::CGAL ALIASED_TARGET)
if(NOT cgal_target)
    set(cgal_target CGAL::CGAL)
endif()
get_target_property(cgal_options ${cgal_target} INTERFACE_COMPILE_OPTIONS)
if(cgal_options)
    set(cgal_rounding_options "${cgal_options}")
    list(FILTER cgal_rounding_options INCLUDE REGEX "frounding-math")
    list(FILTER cgal_options EXCLUDE REGEX "frounding-math")
    set_property(TARGET ${cgal_target} PROPERTY INTERFACE_COMPILE_OPTIONS "${cgal_options}")
endif()
set_property(GLOBAL APPEND PROPERTY JOB_POOLS contour_cgal=1)
add_library(libslic3r_cgal STATIC
    CutSurface.cpp IntersectionPoints.cpp MeshBoolean.cpp TryCatchSignal.cpp Triangulation.cpp)
set_property(TARGET libslic3r_cgal PROPERTY JOB_POOL_COMPILE contour_cgal)
target_include_directories(libslic3r_cgal PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_options(libslic3r_cgal PRIVATE ${cgal_rounding_options})
target_link_libraries(libslic3r_cgal PRIVATE CGAL::CGAL libigl mcut contour_instrumentation)

add_library(libslic3r STATIC ${lisbslic3r_sources}
    "${CMAKE_CURRENT_BINARY_DIR}/libslic3r_version.h")
target_include_directories(libslic3r PUBLIC "${LIBDIR}" "${CMAKE_CURRENT_BINARY_DIR}"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/TextureToColor")
target_link_libraries(libslic3r PUBLIC
    Eigen3::Eigen boost_libs TBB::tbb TBB::tbbmalloc cereal::cereal
    EXPAT::EXPAT PNG::PNG ZLIB::ZLIB OpenSSL::Crypto
    assimp::assimp Freetype::Freetype CGAL::CGAL
    libnest2d admesh libigl miniz clipper nowide glu-libtess qhull semver Clipper2 mcut
    ${CONTOUR_NOISE_LIBRARY} ${CONTOUR_OPENCV_CORE}
    ${CONTOUR_OPENCV_IMGPROC} ${CONTOUR_OPENCV_IMGCODECS}
    TKXDESTEP TKSTEP TKSTEP209 TKSTEPAttr TKSTEPBase TKXCAF TKXSBase TKVCAF
    TKCAF TKLCAF TKCDF TKV3d TKService TKMesh TKBO TKPrim TKHLR TKShHealing
    TKTopAlgo TKGeomAlgo TKBRep TKGeomBase TKG3d TKG2d TKMath TKernel
    Threads::Threads ${CMAKE_DL_LIBS} contour_instrumentation libslic3r_cgal)
target_sources(libslic3r PRIVATE
    SLA/IndexedMesh.cpp
    "${CONTOUR_UPSTREAM_DIR}/engine/slicer/nanosvg.cpp"
    "${LIBDIR}/slic3r/Utils/ColorSpaceConvert.cpp")
if(UNIX AND NOT APPLE)
    target_link_libraries(libslic3r PUBLIC fontconfig)
endif()
if(APPLE)
    target_link_libraries(libslic3r PUBLIC "-framework Foundation" "-framework ModelIO")
endif()
if(WIN32)
    target_link_libraries(libslic3r PUBLIC Psapi)
endif()
