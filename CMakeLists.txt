#
# Main CMakelists for the VALHALLA project.
#
# It aims to be a template and a CMake reference, and as such is documented as much as possible.
# While everything might not fit in any project, it should give good defaults and avoid CMake antipatterns.
# If you disagree with some pieces of advice given here, please discuss it with me by opening a Github Issue !
#
# Project specific options :
#   - VALHALLA_BUILD_TESTS (requires BUILD_TESTING set to ON)
# Other options might be available through the cmake scripts including (not exhaustive):
#   - ENABLE_WARNINGS_SETTINGS
#   - ENABLE_LTO
#

# Tells cmake the minimum version supported is 3.16, but has been updated to work with 3.29 policies
# It is recommended to use the `cmake_minimum_required(VERSION <min>[...<max>])` syntax to indicate the max version that was known to work.
# Many projects broke when CMake 4.0 was released even though they should have been compatible.
cmake_minimum_required(VERSION 3.16...3.29)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "Do not build in-source. Please remove CMakeCache.txt and the CMakeFiles/ directory. Then build out-of-source.")
endif()

# Put the project early since modules might need to detect the compiler.
# More information https://cmake.org/cmake/help/latest/command/project.html
project(
    "VALHALLA" # This will exposed as the variable PROJECT_NAME.
    VERSION 0.1.0 # Used for installation and defines variables PROJECT_VERSION_MAJOR, PROJECT_VERSION_MINOR, PROJECT_VERSION_PATCH, and PROJECT_VERSION_TWEAK.
    LANGUAGES C CXX # Used to determine the languages to use based on file extensions
)

############################
##  Modules and scripts   ##
############################

# Standard CMake modules

include(CTest)                          # Must be called before adding tests but after calling project(). This automatically calls enable_testing() and configures ctest targets when using Make/Ninja
include(CMakeDependentOption)           # This is a really useful scripts that creates options that depends on other options. It can even be used with generator expressions !
include(GNUInstallDirs)                 # This will define the default values for installation directories (all platforms even if named GNU)
include(InstallRequiredSystemLibraries) # Tell CMake that the `install` target needs to install required system libraries (eg: Windows SDK)
include(CMakePackageConfigHelpers)      # Helper to create relocatable packages

# Custom modules and scripts

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake") # Make our cmake scripts available

include(LTO)
include(Warnings)
include(CopyDllsForDebug)
include(Coverage)

###############
##  OPTIONS  ##
###############

# You should try to give as much control over the project setup to the user.
# When modifying compile flags for example, if they are not mandatory, provide an option.

option(${PROJECT_NAME}_INSTALL "Should ${PROJECT_NAME} be added to the install list? Useful if included using add_subdirectory." ON)

# Use your own option for tests, in case people use your library through add_subdirectory
cmake_dependent_option(${PROJECT_NAME}_BUILD_TESTS
    "Enable ${PROJECT_NAME} project tests targets" ON # By default we want tests if CTest is enabled
    "BUILD_TESTING" OFF # Stay coherent with CTest variables
)

# External dependencies
add_subdirectory(external EXCLUDE_FROM_ALL)

# It is always easier to navigate in an IDE when projects are organized in folders.
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Whe building a shared library, you do not want to export all symbols by default
# gcc (and hence clang) are wrong about this.
#
# For more information, see https://gcc.gnu.org/wiki/Visibility and https://www.youtube.com/embed/m0DwB4OvDXk
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)

###############
##  Project  ##
###############

# Check for LTO support (needs to be after project(...) )
find_lto(CXX)

# We use this on windows to add executable icons and other resources
# This is a stupid workaround because if we use .rc files, it will cause our build to fail
#   This seems to be caused by the fact that CMake doesn't wrapp paths with quotes and if they contain white-space,
#   we will get a bunch of "No such file or directory" compiler errors. And I haven't been able to find a workaround, yet
#   other than "don't have your compiler in a path with spaces!"
if(MSYS OR MINGW)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(RCEDIT_EXE_NAME "rcedit-x64.exe")
    else()
        set(RCEDIT_EXE_NAME "rcedit-x86.exe")
    endif()
    include(FetchContent)
    FetchContent_Declare(
        rcedit
        URL "https://github.com/electron/rcedit/releases/download/v2.0.0/${RCEDIT_EXE_NAME}"
        DOWNLOAD_NO_EXTRACT TRUE
        DOWNLOAD_NAME ${RCEDIT_EXE_NAME}
    )
    FetchContent_MakeAvailable(rcedit)
    FetchContent_GetProperties(rcedit SOURCE_DIR RCEDIT_SOURCE_DIR)
    cmake_path(APPEND RCEDIT_SOURCE_DIR "${RCEDIT_EXE_NAME}" OUTPUT_VARIABLE RCEDIT_EXE_PATH)
endif()

#==========================#
#  Enet library            #
#==========================#

# Note that we use both the include and source folders for headers, based on wether they are part of the public API or not.
# For more information about this rationale, see https://github.com/vector-of-bool/pitchfork
add_library(enet
    source/enet/callbacks.c
    source/enet/compress.c
    source/enet/host.c
    source/enet/list.c
    source/enet/packet.c
    source/enet/peer.c
    source/enet/protocol.c
    # While these two could be added only on their speciffic platform by cmake, they have include guards for anything else and works better this way
    source/enet/unix.c
    source/enet/win32.c
    # Headers (not necessary but...)
    source/enet/include/enet/callbacks.h
    source/enet/include/enet/enet.h
    source/enet/include/enet/list.h
    source/enet/include/enet/protocol.h
    source/enet/include/enet/time.h
    source/enet/include/enet/types.h
    source/enet/include/enet/unix.h
    source/enet/include/enet/utility.h
    source/enet/include/enet/win32.h
)

# Setup include folders
target_include_directories(enet
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/source/enet/include>
    PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/source/enet>
)

# Platform speciffic options
if(MSVC OR MSYS OR MINGW)
    target_link_libraries(enet PUBLIC ws2_32)
endif()

# Check for existence of some platform speciffic functions
include(CheckSymbolExists)
# Check for function getaddrinfo
check_symbol_exists(getaddrinfo "netdb.h" HAS_GETADDRINFO)
if(HAS_GETADDRINFO)
    target_compile_definitions(enet PRIVATE HAS_GETADDRINFO=1)
endif()
# Check for function getnameinfo
check_symbol_exists(getnameinfo "netdb.h" HAS_GETNAMEINFO)
if(HAS_GETNAMEINFO)
    target_compile_definitions(enet PRIVATE HAS_GETNAMEINFO=1)
endif()
# Check for function gethostbyaddr_r
check_symbol_exists(gethostbyaddr_r "netdb.h" HAS_GETHOSTBYADDR_R)
if(HAS_GETHOSTBYADDR_R)
    target_compile_definitions(enet PRIVATE HAS_GETHOSTBYADDR_R=1)
endif()
# Check for function gethostbyname_r
check_symbol_exists(gethostbyname_r "netdb.h" HAS_GETHOSTBYNAME_R)
if(HAS_GETHOSTBYNAME_R)
    target_compile_definitions(enet PRIVATE HAS_GETHOSTBYNAME_R=1)
endif()
# Check for function poll
check_symbol_exists(poll "poll.h" HAS_POLL)
if(HAS_POLL)
    target_compile_definitions(enet PRIVATE HAS_POLL=1)
endif()
# Check for function fcntl
check_symbol_exists(fcntl "fcntl.h" HAS_FCNTL)
if(HAS_FCNTL)
    target_compile_definitions(enet PRIVATE HAS_FCNTL=1)
endif()
# Check for function inet_pton
check_symbol_exists(inet_pton "arpa/inet.h" HAS_INET_PTON)
if(HAS_INET_PTON)
    target_compile_definitions(enet PRIVATE HAS_INET_PTON=1)
endif()
# Check for function inet_ntop
check_symbol_exists(inet_ntop "arpa/inet.h" HAS_INET_NTOP)
if(HAS_INET_NTOP)
    target_compile_definitions(enet PRIVATE HAS_INET_NTOP=1)
endif()

include(CheckStructHasMember)
# Check for function struct msghdr has msg_flags member
CHECK_STRUCT_HAS_MEMBER("struct msghdr" msg_flags "sys/socket.h" HAS_MSGHDR_FLAGS LANGUAGE C)
if(HAS_INET_NTOP)
    target_compile_definitions(enet PRIVATE HAS_MSGHDR_FLAGS=1)
endif()

include(CheckTypeSize)
# Check for size of socklen_t
check_type_size(socklen_t SIZEOF_SOCKLEN_T)
if(HAVE_SIZEOF_SOCKLEN_T AND NOT "${SIZEOF_SOCKLEN_T}" STREQUAL "" AND NOT "${SIZEOF_SOCKLEN_T}" STREQUAL "0")
    target_compile_definitions(enet PRIVATE HAS_SOCKLEN_T=1)
endif()

#==========================#
#  System info             #
#==========================#

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(VH_ARCH_BITS "64")
else()
    set(VH_ARCH_BITS "32")
endif()

if(MSVC OR MSYS OR MINGW)
    set(VH_BIN_DIR_NAME "bin${VH_ARCH_BITS}")
    set(VH_BIN_EXT ".exe")
    set(CLIENT_BIN_NAME "valhalla")
    set(SERVER_BIN_NAME "valhalla_server")
    set(MASTER_BIN_NAME "valhalla_master")
elseif(UNIX AND NOT APPLE)
    set(VH_BIN_DIR_NAME "bin_unix")
    set(VH_BIN_EXT "")
    set(CLIENT_BIN_NAME "linux_${VH_ARCH_BITS}_client")
    set(SERVER_BIN_NAME "linux_${VH_ARCH_BITS}_server")
    set(MASTER_BIN_NAME "linux_${VH_ARCH_BITS}_master")
endif()

#==========================#
#  Client executable       #
#==========================#

# Client source files
set(CLIENT_SOURCES
    source/shared/crypto.cpp
	source/shared/geom.cpp
	source/shared/glemu.cpp
	source/shared/stream.cpp
	source/shared/tools.cpp
	source/shared/zip.cpp
	source/engine/aa.cpp
	source/engine/bih.cpp
	source/engine/blend.cpp
	source/engine/client.cpp
	source/engine/command.cpp
	source/engine/console.cpp
	source/engine/dynlight.cpp
	source/engine/grass.cpp
	source/engine/light.cpp
	source/engine/main.cpp
	source/engine/material.cpp
	source/engine/menus.cpp
	source/engine/movie.cpp
	source/engine/normal.cpp
	source/engine/octa.cpp
	source/engine/octaedit.cpp
	source/engine/octarender.cpp
	source/engine/physics.cpp
	source/engine/pvs.cpp
	source/engine/rendergl.cpp
	source/engine/renderlights.cpp
	source/engine/rendermodel.cpp
	source/engine/renderparticles.cpp
	source/engine/rendersky.cpp
	source/engine/rendertext.cpp
	source/engine/renderva.cpp
	source/engine/server.cpp
	source/engine/serverbrowser.cpp
	source/engine/shader.cpp
	source/engine/sound.cpp
	source/engine/stain.cpp
	source/engine/texture.cpp
	source/engine/ui.cpp
	source/engine/liquid.cpp
	source/engine/world.cpp
	source/engine/worldio.cpp
	source/game/ai.cpp
	source/game/gameclient.cpp
	source/game/entity.cpp
	source/game/game.cpp
	source/game/render.cpp
	source/game/scoreboard.cpp
	source/game/gameserver.cpp
	source/game/waypoint.cpp
	source/game/monster.cpp
	source/game/weapon.cpp
	source/game/gamephysics.cpp
	source/game/hud.cpp
	source/game/projectile.cpp
	source/game/announcer.cpp
	source/game/camera.cpp
	source/game/event.cpp
	source/game/query.cpp
	source/game/worlddata.cpp
	source/game/gameshader.cpp
)

# Always list the source files explicitly, including headers so that they are listed in the IDE
# If you need to use files based on a variable value, use target_sources
if(WIN32)
    add_executable(Client WIN32 ${CLIENT_SOURCES})
else()
    add_executable(Client ${CLIENT_SOURCES})
endif()

# Configure include folders
target_include_directories(Client
    PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/source/shared>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/source/engine>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/source/game>
)

# Configure third-party libraries
target_link_libraries(Client
    #PUBLIC # Useful for libraries, see https://cmake.org/cmake/help/latest/manual/cmake-buildsystem.7.html for more details about transitive usage requirements.
        #libraries/targets to link when linking this library
        #this will automatically setup the needed flags and dependencies when linking against this target
    PRIVATE # The following libraries are only linked for this target, and its flags/dependencies will not be used when linking against this target
        general zlibstatic enet SDL2::SDL2-static SDL2_image::SDL2_image-static SDL2_mixer::SDL2_mixer-static # It is possible to link some libraries for debug or optimized builds only
        #debug DEBUGLIBS
        #optimized RELEASELIBS
)

# Specify that we're linking to a static version of zlib
target_compile_definitions(Client PRIVATE ZLIB_STATIC=1)

# Platform speciffic options
if(MSVC OR MSYS OR MINGW)
    if(MSYS OR MINGW)
        target_compile_options(Client PRIVATE -fomit-frame-pointer -ffast-math -Wimplicit-fallthrough -fsigned-char -fno-exceptions -fno-rtti)
        target_link_options(Client PRIVATE -mwindows -static-libgcc -static-libstdc++)
        # Configure executable resources post-build (workaround for .rc causing build errors if compiler path includes spaces)
        add_custom_command(
            TARGET Client POST_BUILD
            COMMAND ${RCEDIT_EXE_PATH} $<TARGET_FILE:Client> --set-icon "${CMAKE_CURRENT_SOURCE_DIR}/source/vcpp/valhalla.ico"
            COMMENT "Adding client executable icon."
            VERBATIM
        )
    else()
        # MSVC needs this or release builds will fail
        target_link_libraries(Client PRIVATE dbghelp)
        # We don't need the workaround for MSVC
        enable_language("RC")
        target_sources(Client PRIVATE source/vcpp/valhalla.rc)
    endif()
    target_link_libraries(Client PRIVATE winmm)
elseif(UNIX AND NOT APPLE)
    # Find and link X11
    find_package(X11)
    if(X11_FOUND)
        target_link_libraries(Client PRIVATE X11::X11)
    endif()
endif()
# Put the executable in the bin folder
add_custom_command(
    TARGET Client POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:Client> "${CMAKE_CURRENT_SOURCE_DIR}/bin/${VH_BIN_DIR_NAME}/${CLIENT_BIN_NAME}${VH_BIN_EXT}"
    COMMENT "Copying client executable to bin/${VH_BIN_DIR_NAME} folder."
    VERBATIM
)

# Find and link OpeGL
find_package(OpenGL COMPONENTS OpenGL)
if(OpenGL_FOUND)
    target_link_libraries(Client PRIVATE OpenGL::GL)
endif()

# Require c++11, this is better than setting CMAKE_CXX_STANDARD since it won't pollute other targets
# note : cxx_std_* features were added in CMake 3.8.2
target_compile_features(Client PRIVATE cxx_std_11)

# CMake scripts extensions
# Helper that can set default warning flags for you
if(NOT MSVC)
    target_set_warnings(Client ENABLE ALL DISABLE Annoying extra pedantic unused ignored-qualifiers cast-function-type)
else()
    target_set_warnings(Client DISABLE ALL) # MSVC has a lot to say about this code. let's ignore it
endif()

target_enable_lto(Client optimized) #enable lto if available for non-debug configurations
copy_dlls_for_debug(Client "" "") # Copy dependencies next to the executable (DLLs for example)

# Setup our project as the startup project for Visual so that people don't need to do it manually
set_directory_properties(PROPERTIES VS_STARTUP_PROJECT Client)


#==========================#
#  Server executable       #
#==========================#

# Server source files
set(SERVER_SOURCES
	source/shared/crypto.cpp
	source/shared/stream.cpp
	source/shared/tools.cpp
	source/engine/command.cpp
	source/engine/server.cpp
	source/engine/worldio.cpp
	source/game/entity.cpp
	source/game/gameserver.cpp
)

# Always list the source files explicitly, including headers so that they are listed in the IDE
# If you need to use files based on a variable value, use target_sources
if(WIN32)
    add_executable(Server WIN32 ${SERVER_SOURCES})
else()
    add_executable(Server ${SERVER_SOURCES})
endif()

# Configure include folders
target_include_directories(Server
    PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/source/shared>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/source/engine>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/source/game>
)

# Configure third-party libraries
target_link_libraries(Server
    #PUBLIC # Useful for libraries, see https://cmake.org/cmake/help/latest/manual/cmake-buildsystem.7.html for more details about transitive usage requirements.
        #libraries/targets to link when linking this library
        #this will automatically setup the needed flags and dependencies when linking against this target
    PRIVATE # The following libraries are only linked for this target, and its flags/dependencies will not be used when linking against this target
        general zlibstatic enet# It is possible to link some libraries for debug or optimized builds only
        #debug DEBUGLIBS
        #optimized RELEASELIBS
)

# Specify that we're linking to a static version of zlib and rendering is not necessary
target_compile_definitions(Server PRIVATE ZLIB_STATIC=1 STANDALONE=1)

# TODO: find out why lib maxminddb doesn't like MSVC
if(NOT MSVC)
    target_link_libraries(Server PRIVATE maxminddb::maxminddb)
    target_compile_definitions(Server PRIVATE HAVE_MAXMINDDB=1)
endif()

# Platform speciffic options
if(MSVC OR MSYS OR MINGW)
    if(MSYS OR MINGW)
        target_compile_options(Server PRIVATE -fomit-frame-pointer -ffast-math -Wimplicit-fallthrough -fsigned-char -fno-exceptions -fno-rtti)
        target_link_options(Server PRIVATE -mwindows -static-libgcc -static-libstdc++)
        # Configure executable resources post-build (workaround for .rc causing build errors if compiler path includes spaces)
        add_custom_command(
            TARGET Server POST_BUILD
            COMMAND ${RCEDIT_EXE_PATH} $<TARGET_FILE:Server> --set-icon "${CMAKE_CURRENT_SOURCE_DIR}/source/vcpp/valhalla.ico"
            COMMENT "Adding server executable icon."
            VERBATIM
        )
    else()
        # We don't need the workaround for MSVC
        enable_language("RC")
        target_sources(Server PRIVATE source/vcpp/valhalla.rc)
    endif()
    target_link_libraries(Server PRIVATE opengl32 winmm)
endif()

# Put the executable in the bin folder
add_custom_command(
    TARGET Server POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:Server> "${CMAKE_CURRENT_SOURCE_DIR}/bin/${VH_BIN_DIR_NAME}/${SERVER_BIN_NAME}${VH_BIN_EXT}"
    COMMENT "Copying server executable to bin/${VH_BIN_DIR_NAME} folder."
    VERBATIM
)

# Require c++11, this is better than setting CMAKE_CXX_STANDARD since it won't pollute other targets
# note : cxx_std_* features were added in CMake 3.8.2
target_compile_features(Server PRIVATE cxx_std_11)

# CMake scripts extensions
# Helper that can set default warning flags for you
if(NOT MSVC)
    target_set_warnings(Server ENABLE ALL DISABLE Annoying extra pedantic unused ignored-qualifiers cast-function-type)
else()
    target_set_warnings(Server DISABLE ALL) # MSVC has a lot to say about this code. let's ignore it
endif()

target_enable_lto(Server optimized) #enable lto if available for non-debug configurations
copy_dlls_for_debug(Server "" "") # Copy dependencies next to the executable (DLLs for example)

#==========================#
#  Master executable       #
#==========================#

# Master source files
set(MASTER_SOURCES
	source/shared/crypto.cpp
	source/shared/stream.cpp
	source/shared/tools.cpp
	source/engine/command.cpp
	source/engine/master.cpp
)

# Always list the source files explicitly, including headers so that they are listed in the IDE
# If you need to use files based on a variable value, use target_sources
add_executable(Master ${MASTER_SOURCES})

# Configure include folders
target_include_directories(Master
    PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/source/shared>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/source/engine>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/source/game>
)

# Configure third-party libraries
target_link_libraries(Master
    #PUBLIC # Useful for libraries, see https://cmake.org/cmake/help/latest/manual/cmake-buildsystem.7.html for more details about transitive usage requirements.
        #libraries/targets to link when linking this library
        #this will automatically setup the needed flags and dependencies when linking against this target
    PRIVATE # The following libraries are only linked for this target, and its flags/dependencies will not be used when linking against this target
        general zlibstatic enet # It is possible to link some libraries for debug or optimized builds only
        #debug DEBUGLIBS
        #optimized RELEASELIBS
)

# Specify that we're linking to a static version of zlib and rendering is not necessary
target_compile_definitions(Master PRIVATE ZLIB_STATIC=1 STANDALONE=1)

# TODO: find out why lib maxminddb doesn't like MSVC
if(NOT MSVC)
    target_link_libraries(Master PRIVATE maxminddb::maxminddb)
    target_compile_definitions(Master PRIVATE HAVE_MAXMINDDB=1)
endif()

# Platform speciffic options
if(MSVC OR MSYS OR MINGW)
    if(MSYS OR MINGW)
        target_compile_options(Master PRIVATE -fomit-frame-pointer -ffast-math -Wimplicit-fallthrough -fsigned-char -fno-exceptions -fno-rtti)
        target_link_options(Master PRIVATE -mwindows -static-libgcc -static-libstdc++)
        # Configure executable resources post-build (workaround for .rc causing build errors if compiler path includes spaces)
        add_custom_command(
            TARGET Master POST_BUILD
            COMMAND ${RCEDIT_EXE_PATH} $<TARGET_FILE:Master> --set-icon "${CMAKE_CURRENT_SOURCE_DIR}/source/vcpp/valhalla.ico"
            COMMENT "Adding master executable icon."
            VERBATIM
        )
    else()
        # We don't need the workaround for MSVC
        enable_language("RC")
        target_sources(Master PRIVATE source/vcpp/valhalla.rc)
    endif()
    target_link_libraries(Master PRIVATE opengl32 winmm)
endif()

# Put the executable in the bin folder
add_custom_command(
    TARGET Master POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:Master> "${CMAKE_CURRENT_SOURCE_DIR}/bin/${VH_BIN_DIR_NAME}/${MASTER_BIN_NAME}${VH_BIN_EXT}"
    COMMENT "Copying server executable to bin/${VH_BIN_DIR_NAME} folder."
    VERBATIM
)

# Require c++11, this is better than setting CMAKE_CXX_STANDARD since it won't pollute other targets
# note : cxx_std_* features were added in CMake 3.8.2
target_compile_features(Master PRIVATE cxx_std_11)

# CMake scripts extensions
# Helper that can set default warning flags for you
if(NOT MSVC)
    target_set_warnings(Master ENABLE ALL DISABLE Annoying extra pedantic unused ignored-qualifiers cast-function-type)
else()
    target_set_warnings(Master DISABLE ALL) # MSVC has a lot to say about this code. let's ignore it
endif()

target_enable_lto(Master optimized) #enable lto if available for non-debug configurations
copy_dlls_for_debug(Master "" "") # Copy dependencies next to the executable (DLLs for example)

#===========#
#   Tests   #
#===========#

if(${PROJECT_NAME}_BUILD_TESTS)
    # Let the user add options to the test runner if needed
    set(TEST_RUNNER_PARAMS "--force-colors=true" CACHE STRING "Options to add to our test runners commands")
    # In a real project you most likely want to exclude test folders
    # list(APPEND CUSTOM_COVERAGE_EXCLUDE "/test/")
    add_subdirectory(tests)
    # You can setup some custom variables and add them to the CTestCustom.cmake.in template to have custom ctest settings
    # For example, you can exclude some directories from the coverage reports such as third-parties and tests
    configure_file(
        ${CMAKE_CURRENT_LIST_DIR}/cmake/CTestCustom.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/CTestCustom.cmake
        @ONLY
    )
endif()

###############
## Packaging ##
###############

if(${PROJECT_NAME}_INSTALL)
    # If we want to use CPack, we need to include it so that it populates variables from our CMakeLists.txt.
    # This will also create a `package` target on supported build systems (make, ninja, VS).
    # There are various CPACK_* variables you can set before `include(CPack)` to configure it (see https://cmake.org/cmake/help/latest/module/CPack.html#variables-common-to-all-cpack-generators).
    set(CPACK_RESOURCE_FILE_README ${CMAKE_CURRENT_LIST_DIR}/readme.md)
    include(CPack)
    
    # Let users choose where to install the cmake package descriptions
    # For that we make use of the CMake Cache
    set(${PROJECT_NAME}_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}" CACHE STRING "Path to install ${PROJECT_NAME} Config*.cmake files to.")
    set(${PROJECT_NAME}_MODULE_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake"               CACHE STRING "Path to install ${PROJECT_NAME}'s .cmake  module files to.")

    # Use version checking helper provided by CMake so that users can safely use a version number in their find_package calls
    write_basic_package_version_file(
        ${PROJECT_NAME}ConfigVersion.cmake # The name of the version file needed by find_package.
        VERSION ${PROJECT_VERSION}         # The version of the project, already set by the `project` command at the top of this file
        COMPATIBILITY SameMajorVersion     # We use semantic versioning, backward compatibity is only guaranteed for a same major version
    )


    # We will need our own file if we have our own dependencies or want some special behavior when the user calls find_package
    # otherwise we could simply install the exports as the ${PROJECT_NAME}Config.cmake
    configure_package_config_file(
        ${PROJECT_SOURCE_DIR}/cmake/${PROJECT_NAME}Config.cmake.in # This is your template file
        ${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake          # This is the resulting file
        INSTALL_DESTINATION ${${PROJECT_NAME}_INSTALL_CMAKEDIR}    # This is where the file will be installed
        # List of paths that needs to be relocated once installed
        # For example if the variable containing the path is named MY_PATH, all instances of @PACKAGE_MY_PATH@ in the template will be replaced by the relocated version of the path
        # This is mostly only needed when you want to install cmake modules or have an unusual layout that cmake is not aware of.
        PATH_VARS ${PROJECT_NAME}_MODULE_INSTALL_DIR # This will be exposed as @PACKAGE_VALHALLA_MODULE_INSTALL_DIR@ in the template file   
        # Imported targets do not require the following macros
        NO_SET_AND_CHECK_MACRO
        NO_CHECK_REQUIRED_COMPONENTS_MACRO 
    )


    # The following will export the targets under the name ${PROJECT_NAME}_Targets, not install them yet
    # It will then need a call to `install(EXPORT)`
    install(
        TARGETS # We can install executables
          Client
          Server
          Master
        EXPORT ${PROJECT_NAME}_Targets
    # Following is only needed pre-cmake3.14
    #    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    #    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    #    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
		# Note PUBLIC_HEADER DESTINATION is necessary only if you provide RUNTIME/LIBRARY/ARCHIVE otherwise CMake will map it to the INCLUDES destination (CMake will warn you with "INSTALL TARGETS - target Client has PUBLIC_HEADER files but no PUBLIC_HEADER DESTINATION." )
	#	PUBLIC_HEADER DESTINATION  ${CMAKE_INSTALL_INCLUDEDIR} 
    # If you want to split between runtime and dev for examples, take a look at COMPONENT, NAMELINK_COMPONENT etc
    # More info in Craig Scott's talk "Deep CMake for library authors" https://www.youtube.com/watch?v=m0DwB4OvDXk
        INCLUDES
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    # This time, install all the exported targets under the ${PROJECT_NAME}_Targets name.
    install(
        EXPORT ${PROJECT_NAME}_Targets 
        NAMESPACE ${PROJECT_NAME}::        # Always specify a namespace so that users can make sure they link targets with transitive properties and not only the library
        FILE ${PROJECT_NAME}Targets.cmake  # This is the file that needs to be included from your *Config.cmake. Otherwise, you could just make this your actual *Config.cmake file.
        DESTINATION ${${PROJECT_NAME}_INSTALL_CMAKEDIR}
    )


    # So far we only installed the exported targets, now install the package config files.
    # If you do not list headers in the PUBLIC_HEADER property, you will need to copy them using `install(FILES)` or `install(DIRECTORY)` too.
    # In that case, you can use CMAKE_INSTALL_INCLUDEDIR as the base destination path.
    install(FILES 
        ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake 
        DESTINATION
            ${${PROJECT_NAME}_INSTALL_CMAKEDIR}
    )

endif()

