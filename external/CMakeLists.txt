project(external C CXX)

set(CMAKE_FOLDER external) # This will regroup all the external targets in a subfolder in IDEs such as Visual Studio

set(BUILD_TESTING_BCKP ${BUILD_TESTING})
set(BUILD_TESTING OFF CACHE BOOL "Force disable of tests for external dependencies" FORCE)

set(CUSTOM_COVERAGE_EXCLUDE ${CUSTOM_COVERAGE_EXCLUDE} "external" PARENT_SCOPE) # Replaced in CTestCustom.cmake.in

# Disable testing or fuzzing if we can
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_FUZZING OFF CACHE BOOL "" FORCE)
# Build everything statically so we can bundle everything in one executable
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(SDL_SHARED_ENABLED_BY_DEFAULT OFF CACHE BOOL "" FORCE)
set(SDL_STATIC_ENABLED_BY_DEFAULT ON CACHE BOOL "" FORCE)

# Use SLD2 for platform abstraction
add_subdirectory(SDL2 EXCLUDE_FROM_ALL)
# Treat SDL2 as a system include as to ignore the warnings
target_set_warnings(SDL2-static DISABLE ALL)
#target_set_warnings(SDL2main DISABLE ALL)

# Force everything to be built statically
set(SDL2IMAGE_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_ZLIB_SHARED OFF CACHE BOOL "" FORCE)
# Don't bother with demos and samples
set(SDL2IMAGE_SAMPLES_DEFAULT OFF CACHE BOOL "" FORCE)
# Use built-in libraries to avoid extra work
set(SDL2IMAGE_VENDORED ON CACHE BOOL "" FORCE)
set(SDL2IMAGE_ZLIB ON CACHE BOOL "" FORCE)
set(SDL2IMAGE_PNG_VENDORED ON CACHE BOOL "" FORCE)
set(SDL2IMAGE_ZLIB_VENDORED ON CACHE BOOL "" FORCE)
# Use libpng for handling png files instead of alternatives
#set(SDL2IMAGE_BACKEND_STB OFF CACHE BOOL "" FORCE)
# Disable loading AVIF images as it requires setting up NASM
set(SDL2IMAGE_AVIF OFF CACHE BOOL "" FORCE) # large and unnecessary library
set(SDL2IMAGE_WEBP OFF CACHE BOOL "" FORCE) # large and unnecessary library
# Use SDL_image for image abstraction
add_subdirectory(SDL_image EXCLUDE_FROM_ALL)
# Treat SDL2_image as a system include as to ignore the warnings
target_set_warnings(SDL2_image DISABLE ALL)

# We use zlib ourselves and if SDL_image didn't include it, we can do it ourselves, manually
if (NOT ZLIB_LIBRARY OR ZLIB_LIBRARY STREQUAL "")
    # disable build of zlib example programs
    set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "zlib examples" FORCE)
    # The engine uses ZLIB for itself and SDL_image includes the sources in its vendor folder
    add_subdirectory(SDL_image/external/zlib EXCLUDE_FROM_ALL)
    # Make the correct ZLIb available (static, always)
    if(SDL2IMAGE_ZLIB_SHARED)
        set(ZLIB_LIBRARY zlib)
    else()
        set(ZLIB_LIBRARY zlibstatic)
    endif()
    if(NOT TARGET ZLIB::ZLIB)
        add_library(ZLIB::ZLIB ALIAS ${ZLIB_LIBRARY})
    endif()
endif()

# Use built-in libraries to avoid extra work
set(SDL2MIXER_VENDORED ON CACHE BOOL "" FORCE)
# Use SDL_mixer for sound abstraction
add_subdirectory(SDL_mixer EXCLUDE_FROM_ALL)
# Treat SDL2_mixer as a system include as to ignore the warnings
target_set_warnings(SDL2_mixer DISABLE ALL)

# Don't build the binary program
set(MAXMINDDB_BUILD_BINARIES OFF CACHE BOOL "" FORCE)
# Use libmaxminddb for geolocation
add_subdirectory(libmaxminddb EXCLUDE_FROM_ALL)
# Treat libmaxminddb as a system include as to ignore the warnings
target_set_warnings(maxminddb DISABLE ALL)

# Doctest for unit tests
add_library(doctest INTERFACE)
target_include_directories(
    doctest
    INTERFACE
    doctest/doctest # note : will expose the parts/ folder...
)
add_library(doctest::doctest ALIAS doctest)
target_compile_features(doctest INTERFACE cxx_std_11)

set(BUILD_TESTING ${BUILD_TESTING_BCKP} CACHE BOOL "Build tests (default variable for CTest)" FORCE) #Set it back to its past value
