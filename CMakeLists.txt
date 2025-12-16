cmake_minimum_required(VERSION 3.24)
project(Extempore VERSION 0.8.9)

option(ASSETS "download multimedia assets (approx 500MB)" OFF)
option(BUILD_TESTS "build test targets (including examples)" ON)
option(PACKAGE "set up install targets for packaging" OFF)

option(EXTERNAL_SHLIBS_AUDIO "download & build audio-related external library dependencies" ON)
option(EXTERNAL_SHLIBS_GRAPHICS "download & build graphics-related external library dependencies" ON)
set(EXTERNAL_SHLIBS (EXTERNAL_SHLIBS_AUDIO OR EXTERNAL_SHLIBS_GRAPHICS))

option(EXT_DYLIB "build extempore as a dynamic library" OFF)

## see note about the status of the Jack backend in BUILDING.md
option(JACK "use the Jack Portaudio backend" OFF)

## this is useful because we can group targets together (e.g. all the AOT libs)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

#############################
# dependency version pinning #
#############################

set(DEP_LLVM_VERSION "21.1.7")
set(DEP_PORTAUDIO_VERSION "v19.7.0")
set(DEP_PORTAUDIO_MD5 "54297c72a852669f9987ee7bb9fba0a6")
set(DEP_PORTMIDI_VERSION "v2.0.8")
set(DEP_PORTMIDI_MD5 "212d4fa92fbb40c8bbaee054af138bf8")
set(DEP_RTMIDI_VERSION "6.0.0")
set(DEP_RTMIDI_MD5 "4fd20d9f3227023d4b369f21d29667ef")
set(DEP_KISS_FFT_VERSION "1.3.0")
set(DEP_SNDFILE_COMMIT "ae64caf9b5946d365971c550875000342e763de6")
set(DEP_NANOVG_COMMIT "3c60175fcc2e5fe305b04355cdce35d499c80310")
set(DEP_STB_COMMIT "152a250a702bf28951bb0220d63bc0c99830c498")
set(DEP_GLFW_VERSION "3.2.1")
set(DEP_ASSIMP_VERSION "3.2")

####################
# helper functions #
####################

# Platform detection - sets EXTEMPORE_SYSTEM_NAME, EXTEMPORE_SYSTEM_VERSION, EXTEMPORE_SYSTEM_ARCHITECTURE
function(extempore_detect_platform)
    if(UNIX)
        find_program(UNAME_PROGRAM uname)
        execute_process(COMMAND ${UNAME_PROGRAM} -m
      OUTPUT_VARIABLE UNAME_MACHINE_NAME
      OUTPUT_STRIP_TRAILING_WHITESPACE)
        execute_process(COMMAND ${UNAME_PROGRAM} -r
      OUTPUT_VARIABLE UNAME_OS_RELEASE
      OUTPUT_STRIP_TRAILING_WHITESPACE)
        execute_process(COMMAND ${UNAME_PROGRAM} -s
      OUTPUT_VARIABLE UNAME_OS_NAME
      OUTPUT_STRIP_TRAILING_WHITESPACE)
        set(UNAME_MACHINE_NAME ${UNAME_MACHINE_NAME} PARENT_SCOPE)
    endif()

    if(APPLE)
        set(EXTEMPORE_SYSTEM_NAME "osx" PARENT_SCOPE)
        execute_process(COMMAND sw_vers -productVersion
      OUTPUT_VARIABLE _version
      OUTPUT_STRIP_TRAILING_WHITESPACE)
        string(REGEX MATCH "^[0-9]+\\.?[0-9]*" _version ${_version})
        set(EXTEMPORE_SYSTEM_VERSION ${_version} PARENT_SCOPE)
        set(EXTEMPORE_SYSTEM_ARCHITECTURE ${UNAME_MACHINE_NAME} PARENT_SCOPE)
    elseif(UNIX)
        execute_process(COMMAND lsb_release -is
      OUTPUT_VARIABLE _name
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET)
        if(NOT _name)
            set(_name ${UNAME_OS_NAME})
        endif()
        set(EXTEMPORE_SYSTEM_NAME ${_name} PARENT_SCOPE)
        set(EXTEMPORE_SYSTEM_VERSION ${UNAME_OS_RELEASE} PARENT_SCOPE)
        set(EXTEMPORE_SYSTEM_ARCHITECTURE ${UNAME_MACHINE_NAME} PARENT_SCOPE)
    elseif(WIN32)
        set(EXTEMPORE_SYSTEM_NAME "Windows" PARENT_SCOPE)
        string(REGEX MATCH "^[0-9]+" _version ${CMAKE_SYSTEM_VERSION})
        if(_version LESS 10)
            math(EXPR _version "${_version} + 1")
        endif()
        set(EXTEMPORE_SYSTEM_VERSION ${_version} PARENT_SCOPE)
        set(EXTEMPORE_SYSTEM_ARCHITECTURE ${CMAKE_SYSTEM_PROCESSOR} PARENT_SCOPE)
    else()
        message(FATAL_ERROR "Sorry, Extempore isn't supported on this platform - macOS, Linux & Windows only.")
    endif()
endfunction()

# Add an external project with common settings
function(extempore_add_external name)
    cmake_parse_arguments(ARG "" "URL;URL_MD5;FOLDER" "CMAKE_ARGS" ${ARGN})
    ExternalProject_Add(${name}
    PREFIX ${name}
    URL ${ARG_URL}
    URL_MD5 ${ARG_URL_MD5}
    CMAKE_ARGS
      -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
      -DCMAKE_C_FLAGS=${EXT_DEPS_C_FLAGS}
      -DCMAKE_CXX_FLAGS=${EXT_DEPS_CXX_FLAGS}
      -DCMAKE_INSTALL_PREFIX=${EXT_DEPS_INSTALL_DIR}
      -DCMAKE_POLICY_VERSION_MINIMUM=3.5
      ${ARG_CMAKE_ARGS})
    set_target_properties(${name} PROPERTIES FOLDER ${ARG_FOLDER})
endfunction()

# AOT port counter management
set(EXTEMPORE_AOT_PORT_COUNTER 17099 CACHE INTERNAL "")
function(extempore_get_next_port OUT_VAR)
    set(${OUT_VAR} ${EXTEMPORE_AOT_PORT_COUNTER} PARENT_SCOPE)
    math(EXPR _new_port "${EXTEMPORE_AOT_PORT_COUNTER} - 2")
    set(EXTEMPORE_AOT_PORT_COUNTER ${_new_port} CACHE INTERNAL "" FORCE)
endfunction()

####################
# option wrangling #
####################

if(EXT_DYLIB)
    set(EXTERNAL_SHLIBS_AUDIO OFF)
    set(EXTERNAL_SHLIBS_GRAPHICS OFF)
    set(EXTERNAL_SHLIBS OFF)
    message(STATUS "Building Extempore as a library")
    message(STATUS "Building without external dependencies")
endif()

# ARM64 requires macOS 11.0 (Big Sur) minimum
if(APPLE)
    if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64" OR CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "arm64")
            set(CMAKE_OSX_DEPLOYMENT_TARGET 11.0)
        endif()
    endif()
endif()

if(PACKAGE)
    if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET 10.12)
    endif()
    set(ASSETS ON)
    message(STATUS "Building Extempore for binary distribution (assets directory will be downloaded)")
endif()

# External shared library dependencies
if(EXTERNAL_SHLIBS)
    include(ExternalProject)
    set(EXT_DEPS_INSTALL_DIR ${CMAKE_BINARY_DIR}/deps-install)
    set(EXT_PLATFORM_SHLIBS_DIR ${CMAKE_SOURCE_DIR}/libs/platform-shlibs)
    if(PACKAGE)
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64|ARM64")
            set(EXT_DEPS_C_FLAGS "${CMAKE_C_FLAGS_RELEASE}")
            set(EXT_DEPS_CXX_FLAGS "${CMAKE_CXX_FLAGS_RELEASE}")
            message(STATUS "ARM64 detected: using native CPU tuning")
        else()
            set(EXT_DEPS_C_FLAGS "${CMAKE_C_FLAGS_RELEASE} -mtune=generic")
            set(EXT_DEPS_CXX_FLAGS "${CMAKE_CXX_FLAGS_RELEASE} -mtune=generic")
        endif()
        message(STATUS "compiler flags for packaging:\nC    ${EXT_DEPS_C_FLAGS}\nCXX  ${EXT_DEPS_CXX_FLAGS}")
    endif()
endif()

if(EXT_DYLIB)
    set(EXT_DEPS_C_FLAGS "${EXT_DEPS_C_FLAGS} -fPIC")
    set(EXT_DEPS_CXX_FLAGS "${EXT_DEPS_CXX_FLAGS} -fPIC")
endif()

if(NOT ${CMAKE_SIZEOF_VOID_P} EQUAL 8)
    message(FATAL_ERROR "Extempore currently only runs on 64-bit platforms.")
endif()

# Set a default build type if none was specified
if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "Building 'Release' configuration")
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

####################
# platform/version #
####################

extempore_detect_platform()

########
# PCRE #
########

# current in-tree PCRE version: 8.38

add_library(pcre STATIC
  src/pcre/config.h
  src/pcre/pcre.h
  src/pcre/ucp.h
  src/pcre/pcre_chartables.c
  src/pcre/pcre_compile.c
  src/pcre/pcre_exec.c
  src/pcre/pcre_globals.c
  src/pcre/pcre_internal.h
  src/pcre/pcre_newline.c
  src/pcre/pcre_tables.c)

target_compile_definitions(pcre PRIVATE -DHAVE_CONFIG_H)

if(PACKAGE AND NOT CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64|ARM64")
    target_compile_options(pcre PRIVATE -mtune=generic)
endif()

#############
# portaudio #
#############

include(ExternalProject)

ExternalProject_Add(portaudio_static
  PREFIX portaudio
  URL https://github.com/PortAudio/portaudio/archive/${DEP_PORTAUDIO_VERSION}.zip
  URL_MD5 ${DEP_PORTAUDIO_MD5}
  CMAKE_ARGS
    -DPA_BUILD_STATIC=ON
    -DPA_BUILD_SHARED=OFF
    -DPA_LIBNAME_ADD_SUFFIX=OFF
    -DPA_USE_JACK=${JACK}
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DCMAKE_C_FLAGS=${EXT_DEPS_C_FLAGS}
    -DCMAKE_CXX_FLAGS=${EXT_DEPS_CXX_FLAGS}
    -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/portaudio
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5)

########
# LLVM #
########

# Detect target architecture for LLVM
if(APPLE)
    if(UNAME_MACHINE_NAME STREQUAL "arm64")
        set(LLVM_TARGET_ARCH "AArch64")
    else()
        set(LLVM_TARGET_ARCH "X86")
    endif()
elseif(UNIX)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
        set(LLVM_TARGET_ARCH "AArch64")
    else()
        set(LLVM_TARGET_ARCH "X86")
    endif()
else()
    set(LLVM_TARGET_ARCH "X86")
endif()

message(STATUS "LLVM target architecture: ${LLVM_TARGET_ARCH}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

set(LLVM_TARGETS_TO_BUILD ${LLVM_TARGET_ARCH} CACHE STRING "" FORCE)
set(LLVM_ENABLE_TERMINFO OFF CACHE BOOL "" FORCE)
set(LLVM_ENABLE_ZLIB OFF CACHE BOOL "" FORCE)
set(LLVM_ENABLE_ZSTD OFF CACHE BOOL "" FORCE)
set(LLVM_ENABLE_LIBXML2 OFF CACHE BOOL "" FORCE)
set(LLVM_INCLUDE_TOOLS OFF CACHE BOOL "" FORCE)
set(LLVM_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(LLVM_INCLUDE_UTILS OFF CACHE BOOL "" FORCE)
set(LLVM_BUILD_RUNTIME OFF CACHE BOOL "" FORCE)
set(LLVM_INCLUDE_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LLVM_INCLUDE_TESTS OFF CACHE BOOL "" FORCE)
set(LLVM_INCLUDE_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(LLVM_INCLUDE_DOCS OFF CACHE BOOL "" FORCE)
set(LLVM_ENABLE_BINDINGS OFF CACHE BOOL "" FORCE)
set(LLVM_ENABLE_OCAMLDOC OFF CACHE BOOL "" FORCE)

FetchContent_Declare(LLVM
  URL https://github.com/llvm/llvm-project/releases/download/llvmorg-${DEP_LLVM_VERSION}/llvm-project-${DEP_LLVM_VERSION}.src.tar.xz
  SOURCE_SUBDIR llvm)

FetchContent_MakeAvailable(LLVM)

# LLVM components needed by Extempore:
#   - OrcJIT: for LLJIT runtime compilation
#   - Target architecture: native target codegen (X86 or AArch64)
#   - AsmParser: for parsing LLVM IR text
#   - Passes: for optimization passes
#   - MCDisassembler: for disassembly support
set(EXTEMPORE_LLVM_COMPONENTS
  OrcJIT
  ${LLVM_TARGET_ARCH}
  AsmParser
  Passes
  MCDisassembler
  IRPrinter
)

llvm_map_components_to_libnames(LLVM_LIBRARIES ${EXTEMPORE_LLVM_COMPONENTS})
message(STATUS "LLVM libraries: ${LLVM_LIBRARIES}")

# Set LLVM include directories (FetchContent doesn't set LLVM_INCLUDE_DIRS)
set(LLVM_INCLUDE_DIRS
  ${llvm_SOURCE_DIR}/llvm/include
  ${llvm_BINARY_DIR}/include)

#############
# extempore #
#############

set(EXTEMPORE_SOURCES
  src/Extempore.cpp
  src/AudioDevice.cpp
  src/EXTZones.cpp
  src/EXTClosureAddressTable.cpp
  src/EXTLLVM.cpp
  src/EXTThread.cpp
  src/shims/__hash_memory.cpp
  src/OSC.cpp
  src/Scheme.cpp
  src/SchemeFFI.cpp
  src/SchemeProcess.cpp
  src/SchemeREPL.cpp
  src/TaskScheduler.cpp
  src/UNIV.cpp)

if(EXT_DYLIB)
    include(${CMAKE_SOURCE_DIR}/CMakeRC.cmake)

    cmrc_add_resource_library(rc_xtm NAMESPACE "xtm"
    runtime/bitcode.ll
    runtime/init.ll
    runtime/init.xtm
    runtime/inline.ll
    runtime/llvmir.xtm
    runtime/llvmti.xtm
    runtime/scheme.xtm)

    add_library(extempore SHARED ${EXTEMPORE_SOURCES})
    target_link_libraries(extempore PRIVATE rc_xtm)
else()
    add_executable(extempore ${EXTEMPORE_SOURCES})
endif()

if(MSVC)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /DEF:${CMAKE_SOURCE_DIR}/src/extempore.def")
endif()

target_include_directories(extempore PRIVATE include)

# suppress the warning about the opcode switch statement
if(UNIX)
    set_source_files_properties(src/Scheme.cpp PROPERTIES COMPILE_FLAGS -Wno-switch)
endif()

# dependencies
add_dependencies(extempore pcre portaudio_static)

if(WIN32)
    target_include_directories(extempore PRIVATE src/networking-ts-impl/include)
endif()

target_include_directories(extempore PRIVATE
  src/pcre
  ${CMAKE_BINARY_DIR}/portaudio/include
  ${LLVM_INCLUDE_DIRS})

target_link_directories(extempore PRIVATE ${CMAKE_BINARY_DIR}/portaudio/lib)
target_link_libraries(extempore PRIVATE pcre portaudio${CMAKE_STATIC_LIBRARY_SUFFIX} ${LLVM_LIBRARIES})

if(UNIX AND NOT APPLE)
    target_link_libraries(extempore PRIVATE asound)
endif()

# compiler options
if(PACKAGE)
    target_compile_definitions(extempore PRIVATE -DEXT_SHARE_DIR=".")
    if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64|ARM64")
        target_compile_options(extempore PRIVATE -mtune=generic)
    endif()
elseif(EXT_SHARE_DIR)
    target_compile_definitions(extempore PRIVATE -DEXT_SHARE_DIR="${EXT_SHARE_DIR}")
elseif(EXT_DYLIB)
    target_compile_definitions(extempore PRIVATE -DEXT_DYLIB=1 -DEXT_SHARE_DIR=".")
else()
    target_compile_definitions(extempore PRIVATE -DEXT_SHARE_DIR="${CMAKE_SOURCE_DIR}")
endif()

# platform-specific config
if(UNIX)
    target_compile_definitions(extempore PRIVATE
    -D_GNU_SOURCE
    -D__STDC_CONSTANT_MACROS
    -D__STDC_FORMAT_MACROS
    -D__STDC_LIMIT_MACROS)
    target_compile_options(extempore PRIVATE
    -std=c++17
    -fvisibility-inlines-hidden
    -fno-rtti
    -fno-common
    -Woverloaded-virtual
    -Wno-unused-result)
    target_link_libraries(extempore PRIVATE pthread)
endif()

if(WIN32)
    target_compile_definitions(extempore PRIVATE -DPCRE_STATIC -D_CRT_SECURE_NO_WARNINGS)
elseif(APPLE)
    set(CMAKE_C_COMPILER clang)
    set(CMAKE_CXX_COMPILER clang++)
    set_source_files_properties(src/Extempore.cpp src/SchemeFFI.cpp src/UNIV.cpp
    PROPERTIES COMPILE_FLAGS "-x objective-c++")
    target_link_libraries(extempore PRIVATE
    "-framework Cocoa"
    "-framework CoreAudio"
    "-framework AudioUnit"
    "-framework AudioToolbox")
elseif(UNIX AND NOT APPLE)
    set_property(TARGET pcre PROPERTY POSITION_INDEPENDENT_CODE ON)
    set_property(TARGET extempore PROPERTY POSITION_INDEPENDENT_CODE ON)
    target_link_libraries(extempore PRIVATE dl)
endif()

if(WIN32)
    set_target_properties(extempore PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_SOURCE_DIR}
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}
    LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_SOURCE_DIR}
    LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_SOURCE_DIR}/libs/platform-shlibs
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/libs/platform-shlibs)
endif()

##########
# assets #
##########

add_custom_target(assets
  COMMAND ${CMAKE_COMMAND}
    -DASSETS_PATH=${CMAKE_SOURCE_DIR}/assets
    -DASSETS_GIT_REF=0c9f32c
    -P ${CMAKE_SOURCE_DIR}/extras/cmake/download_assets.cmake)

if(ASSETS)
    add_dependencies(extempore assets)
endif()

###########
# install #
###########

if(NOT PACKAGE)
    install(TARGETS extempore RUNTIME DESTINATION bin)
else()
    install(TARGETS extempore RUNTIME DESTINATION ".")
    install(DIRECTORY assets runtime libs examples tests
    DESTINATION "."
    PATTERN ".DS_Store" EXCLUDE)
endif()

###################
# AOT compilation #
###################

if(WIN32)
    macro(aotcompile file)
        configure_file(
      ${CMAKE_SOURCE_DIR}/extras/cmake/${file}.cmake.in
      ${CMAKE_SOURCE_DIR}/extras/cmake/${file}.cmake
      @ONLY)
        add_custom_target(${file} ALL
      COMMAND ${CMAKE_COMMAND} -P ${CMAKE_SOURCE_DIR}/extras/cmake/${file}.cmake
      COMMENT "Ahead-of-time compiling the ${file} standard library..."
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
        set_target_properties(${file} PROPERTIES FOLDER AOT)
    endmacro()

    aotcompile(aot)

else()
    # Unix: parallel AOT compilation with proper dependencies
    # Use timeout to kill stuck processes and release ports
    if(APPLE)
        find_program(TIMEOUT_CMD gtimeout)
        if(NOT TIMEOUT_CMD)
            find_program(TIMEOUT_CMD timeout)
        endif()
    else()
        find_program(TIMEOUT_CMD timeout)
    endif()
    set(AOT_TIMEOUT_SECONDS 300)

    macro(aotcompile_lib libfile group)
        get_filename_component(_basename ${libfile} NAME_WE)
        set(_targetname aot_${_basename})
        set(_filename ${CMAKE_SOURCE_DIR}/libs/aot-cache/xtm${_basename}.ll)

        extempore_get_next_port(_port)

        if(TIMEOUT_CMD)
            set(_extempore_cmd ${TIMEOUT_CMD} --kill-after=10 ${AOT_TIMEOUT_SECONDS} $<TARGET_FILE:extempore>)
        else()
            set(_extempore_cmd $<TARGET_FILE:extempore>)
        endif()

        if(PACKAGE)
            add_custom_command(OUTPUT ${_filename}
        COMMAND ${_extempore_cmd} --nobase --noaudio --mcpu=generic --attr=none --port=${_port}
          --eval "(impc:aot:compile-xtm-file \"${libfile}\")"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        VERBATIM)
        else()
            add_custom_command(OUTPUT ${_filename}
        COMMAND ${_extempore_cmd} --nobase --noaudio --port=${_port}
          --eval "(impc:aot:compile-xtm-file \"${libfile}\")"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        VERBATIM)
        endif()

        add_custom_target(${_targetname} DEPENDS ${_filename} extempore)
        set_target_properties(${_targetname} PROPERTIES FOLDER AOT)

        if(NOT ${group} STREQUAL "core")
            add_dependencies(${_targetname} external_shlibs_${group})
            add_dependencies(aot_external_${group} ${_targetname})
        else()
            add_dependencies(aot_core ${_targetname})
        endif()

        foreach(_dep ${ARGN})
            add_dependencies(${_targetname} aot_${_dep})
        endforeach()
    endmacro()

    # core libs
    add_custom_target(aot_core)
    aotcompile_lib(libs/base/base.xtm core)
    aotcompile_lib(libs/core/math.xtm core base)
    aotcompile_lib(libs/core/rational.xtm core base)
    aotcompile_lib(libs/core/audiobuffer.xtm core base)
    aotcompile_lib(libs/core/audio_dsp.xtm core base rational audiobuffer)
    aotcompile_lib(libs/core/instruments.xtm core base audio_dsp)
    set_target_properties(aot_core PROPERTIES FOLDER AOT)
    add_dependencies(aot_core extempore)
endif()

add_custom_target(clean_aot
  COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_SOURCE_DIR}/libs/aot-cache
  COMMENT "Removing AOT-compiled libs")

########################################
# external shared library dependencies #
########################################

if(EXTERNAL_SHLIBS_AUDIO)
    extempore_add_external(portmidi
    URL https://github.com/PortMidi/portmidi/archive/${DEP_PORTMIDI_VERSION}.zip
    URL_MD5 ${DEP_PORTMIDI_MD5}
    FOLDER EXTERNAL_SHLIBS)

    extempore_add_external(rtmidi
    URL https://github.com/thestk/rtmidi/archive/${DEP_RTMIDI_VERSION}.zip
    URL_MD5 ${DEP_RTMIDI_MD5}
    FOLDER EXTERNAL_SHLIBS
    CMAKE_ARGS -DRTMIDI_BUILD_TESTING=OFF
      $<$<BOOL:${WIN32}>:-DCMAKE_INSTALL_LIBDIR=${EXT_DEPS_INSTALL_DIR}>
      $<$<BOOL:${WIN32}>:-DCMAKE_INSTALL_BINDIR=${EXT_DEPS_INSTALL_DIR}>)

    extempore_add_external(kiss_fft
    URL https://github.com/extemporelang/kiss_fft/archive/${DEP_KISS_FFT_VERSION}.zip
    FOLDER EXTERNAL_SHLIBS)

    extempore_add_external(sndfile
    URL https://github.com/erikd/libsndfile/archive/${DEP_SNDFILE_COMMIT}.zip
    FOLDER EXTERNAL_SHLIBS
    CMAKE_ARGS
      -DBUILD_SHARED_LIBS=ON
      -DBUILD_PROGRAMS=OFF
      -DBUILD_EXAMPLES=OFF
      -DENABLE_EXTERNAL_LIBS=OFF
      -DBUILD_TESTING=OFF
      -DENABLE_CPACK=OFF
      -DENABLE_PACKAGE_CONFIG=OFF
      $<$<BOOL:${WIN32}>:-DENABLE_STATIC_RUNTIME=OFF>)

    if(UNIX)
        add_custom_target(aot_external_audio ALL)
        set_target_properties(aot_external_audio PROPERTIES FOLDER AOT)
        aotcompile_lib(libs/external/fft.xtm audio base math)
        aotcompile_lib(libs/external/sndfile.xtm audio base)
        aotcompile_lib(libs/external/audio_dsp_ext.xtm audio base fft sndfile)
        aotcompile_lib(libs/external/instruments_ext.xtm audio base sndfile instruments)
        aotcompile_lib(libs/external/portmidi.xtm audio base)

        add_custom_target(external_shlibs_audio
      COMMENT "moving shared libs into ${EXT_PLATFORM_SHLIBS_DIR}"
      DEPENDS sndfile kiss_fft portmidi rtmidi
      COMMAND ${CMAKE_COMMAND} -E make_directory ${EXT_PLATFORM_SHLIBS_DIR}
      COMMAND ${CMAKE_COMMAND} -E copy libkiss_fft${CMAKE_SHARED_LIBRARY_SUFFIX} ${EXT_PLATFORM_SHLIBS_DIR}
      COMMAND ${CMAKE_COMMAND} -E copy libportmidi${CMAKE_SHARED_LIBRARY_SUFFIX} ${EXT_PLATFORM_SHLIBS_DIR}
      COMMAND ${CMAKE_COMMAND} -E copy librtmidi${CMAKE_SHARED_LIBRARY_SUFFIX} ${EXT_PLATFORM_SHLIBS_DIR}
      COMMAND ${CMAKE_COMMAND} -E copy libsndfile${CMAKE_SHARED_LIBRARY_SUFFIX} ${EXT_PLATFORM_SHLIBS_DIR}
      WORKING_DIRECTORY ${EXT_DEPS_INSTALL_DIR}/lib)
        set_target_properties(external_shlibs_audio PROPERTIES FOLDER EXTERNAL_SHLIBS)
        add_dependencies(aot_external_audio extempore external_shlibs_audio)
    endif()
endif()

if(EXTERNAL_SHLIBS_GRAPHICS)
    extempore_add_external(nanovg
    URL https://github.com/extemporelang/nanovg/archive/${DEP_NANOVG_COMMIT}.tar.gz
    FOLDER EXTERNAL_SHLIBS
    CMAKE_ARGS -DEXTEMPORE_LIB_PATH=${CMAKE_SOURCE_DIR}/libs/platform-shlibs/extempore.lib)
    add_dependencies(nanovg extempore)

    extempore_add_external(stb_image
    URL https://github.com/extemporelang/stb/archive/${DEP_STB_COMMIT}.zip
    FOLDER EXTERNAL_SHLIBS)

    extempore_add_external(glfw3
    URL https://github.com/glfw/glfw/releases/download/${DEP_GLFW_VERSION}/glfw-${DEP_GLFW_VERSION}.zip
    FOLDER EXTERNAL_SHLIBS
    CMAKE_ARGS
      -DBUILD_SHARED_LIBS=ON
      -DGLFW_BUILD_EXAMPLES=OFF
      -DGLFW_BUILD_TESTS=OFF)

    extempore_add_external(assimp
    URL https://github.com/assimp/assimp/archive/v${DEP_ASSIMP_VERSION}.zip
    FOLDER EXTERNAL_SHLIBS
    CMAKE_ARGS
      -DCMAKE_DEBUG_POSTFIX=
      -DASSIMP_BUILD_ASSIMP_TOOLS=OFF
      -DASSIMP_BUILD_SAMPLES=OFF
      -DASSIMP_BUILD_TESTS=OFF)

    if(UNIX)
        add_custom_target(aot_external_graphics ALL)
        set_target_properties(aot_external_graphics PROPERTIES FOLDER AOT)

        aotcompile_lib(libs/external/stb_image.xtm graphics base)
        aotcompile_lib(libs/external/glfw3.xtm graphics base)

        if(DEFINED ENV{EXTEMPORE_FORCE_GL_GETPROCADDRESS})
            set(GL_BIND_METHOD getprocaddress)
        else()
            set(GL_BIND_METHOD directbind)
        endif()

        aotcompile_lib(libs/external/gl/glcore-${GL_BIND_METHOD}.xtm graphics base)
        aotcompile_lib(libs/external/gl/gl-objects.xtm graphics base math glcore-${GL_BIND_METHOD} stb_image)
        aotcompile_lib(libs/external/gl/gl-objects2.xtm graphics base glcore-${GL_BIND_METHOD} stb_image)
        aotcompile_lib(libs/external/graphics-pipeline.xtm graphics base math glcore-${GL_BIND_METHOD} stb_image gl-objects2)
        aotcompile_lib(libs/external/nanovg.xtm graphics base glcore-${GL_BIND_METHOD})
        aotcompile_lib(libs/external/assimp.xtm graphics base stb_image graphics-pipeline)
        aotcompile_lib(libs/external/gl/glcompat-${GL_BIND_METHOD}.xtm graphics base)

        add_custom_target(external_shlibs_graphics
      COMMENT "moving shared libs into ${EXT_PLATFORM_SHLIBS_DIR}"
      DEPENDS assimp glfw3 stb_image nanovg
      COMMAND ${CMAKE_COMMAND} -E make_directory ${EXT_PLATFORM_SHLIBS_DIR}
      COMMAND ${CMAKE_COMMAND} -E copy libassimp${CMAKE_SHARED_LIBRARY_SUFFIX} ${EXT_PLATFORM_SHLIBS_DIR}
      COMMAND ${CMAKE_COMMAND} -E copy libglfw${CMAKE_SHARED_LIBRARY_SUFFIX} ${EXT_PLATFORM_SHLIBS_DIR}
      COMMAND ${CMAKE_COMMAND} -E copy libnanovg${CMAKE_SHARED_LIBRARY_SUFFIX} ${EXT_PLATFORM_SHLIBS_DIR}
      COMMAND ${CMAKE_COMMAND} -E copy libstb_image${CMAKE_SHARED_LIBRARY_SUFFIX} ${EXT_PLATFORM_SHLIBS_DIR}
      WORKING_DIRECTORY ${EXT_DEPS_INSTALL_DIR}/lib)
        set_target_properties(external_shlibs_graphics PROPERTIES FOLDER EXTERNAL_SHLIBS)
        add_dependencies(aot_external_graphics extempore external_shlibs_graphics)
    endif()
endif()

# Windows external shlibs handling
if(WIN32 AND EXTERNAL_SHLIBS_AUDIO)
    if(EXTERNAL_SHLIBS_GRAPHICS)
        aotcompile(aot_external)

        add_custom_target(external_shlibs_audio
      COMMENT "moving .dll and .lib files into ${EXT_PLATFORM_SHLIBS_DIR}"
      DEPENDS sndfile kiss_fft portmidi rtmidi
      COMMAND ${CMAKE_COMMAND} -E make_directory ${EXT_PLATFORM_SHLIBS_DIR}
      COMMAND ${CMAKE_COMMAND} -E copy bin/sndfile.dll ${EXT_PLATFORM_SHLIBS_DIR}
      COMMAND ${CMAKE_COMMAND} -E copy lib/kiss_fft.dll ${EXT_PLATFORM_SHLIBS_DIR}
      COMMAND ${CMAKE_COMMAND} -E copy lib/kiss_fft.lib ${EXT_PLATFORM_SHLIBS_DIR}
      COMMAND ${CMAKE_COMMAND} -E copy lib/portmidi.dll ${EXT_PLATFORM_SHLIBS_DIR}
      COMMAND ${CMAKE_COMMAND} -E copy lib/portmidi.lib ${EXT_PLATFORM_SHLIBS_DIR}
      COMMAND ${CMAKE_COMMAND} -E copy rtmidi.dll ${EXT_PLATFORM_SHLIBS_DIR}
      COMMAND ${CMAKE_COMMAND} -E copy rtmidi.lib ${EXT_PLATFORM_SHLIBS_DIR}
      COMMAND ${CMAKE_COMMAND} -E copy lib/sndfile.lib ${EXT_PLATFORM_SHLIBS_DIR}
      WORKING_DIRECTORY ${EXT_DEPS_INSTALL_DIR})
        set_target_properties(external_shlibs_audio PROPERTIES FOLDER EXTERNAL_SHLIBS)

        add_custom_target(external_shlibs_graphics
      COMMENT "moving .dll and .lib files into ${EXT_PLATFORM_SHLIBS_DIR}"
      DEPENDS assimp glfw3 stb_image nanovg
      COMMAND ${CMAKE_COMMAND} -E make_directory ${EXT_PLATFORM_SHLIBS_DIR}
      COMMAND ${CMAKE_COMMAND} -E copy bin/assimp-vc130-mt.dll ${EXT_PLATFORM_SHLIBS_DIR}
      COMMAND ${CMAKE_COMMAND} -E copy lib/assimp-vc130-mt.lib ${EXT_PLATFORM_SHLIBS_DIR}
      COMMAND ${CMAKE_COMMAND} -E copy lib/glfw3.dll ${EXT_PLATFORM_SHLIBS_DIR}
      COMMAND ${CMAKE_COMMAND} -E copy lib/glfw3dll.lib ${EXT_PLATFORM_SHLIBS_DIR}/glfw3.lib
      COMMAND ${CMAKE_COMMAND} -E copy lib/nanovg.dll ${EXT_PLATFORM_SHLIBS_DIR}
      COMMAND ${CMAKE_COMMAND} -E copy lib/nanovg.lib ${EXT_PLATFORM_SHLIBS_DIR}
      COMMAND ${CMAKE_COMMAND} -E copy lib/stb_image.dll ${EXT_PLATFORM_SHLIBS_DIR}
      COMMAND ${CMAKE_COMMAND} -E copy lib/stb_image.lib ${EXT_PLATFORM_SHLIBS_DIR}
      WORKING_DIRECTORY ${EXT_DEPS_INSTALL_DIR})
        set_target_properties(external_shlibs_graphics PROPERTIES FOLDER EXTERNAL_SHLIBS)

        add_dependencies(aot_external extempore external_shlibs_audio external_shlibs_graphics)

    else()
        aotcompile(aot_external_audio)

        add_custom_target(external_shlibs_audio
      COMMENT "moving .dll and .lib files into ${EXT_PLATFORM_SHLIBS_DIR}"
      DEPENDS sndfile kiss_fft portmidi rtmidi
      COMMAND ${CMAKE_COMMAND} -E make_directory ${EXT_PLATFORM_SHLIBS_DIR}
      COMMAND ${CMAKE_COMMAND} -E copy lib/kiss_fft.dll ${EXT_PLATFORM_SHLIBS_DIR}
      COMMAND ${CMAKE_COMMAND} -E copy lib/kiss_fft.lib ${EXT_PLATFORM_SHLIBS_DIR}
      COMMAND ${CMAKE_COMMAND} -E copy lib/portmidi.dll ${EXT_PLATFORM_SHLIBS_DIR}
      COMMAND ${CMAKE_COMMAND} -E copy lib/portmidi.lib ${EXT_PLATFORM_SHLIBS_DIR}
      COMMAND ${CMAKE_COMMAND} -E copy lib/rtmidi.dll ${EXT_PLATFORM_SHLIBS_DIR}
      COMMAND ${CMAKE_COMMAND} -E copy lib/rtmidi.lib ${EXT_PLATFORM_SHLIBS_DIR}
      COMMAND ${CMAKE_COMMAND} -E copy bin/sndfile.dll ${EXT_PLATFORM_SHLIBS_DIR}
      COMMAND ${CMAKE_COMMAND} -E copy lib/sndfile.lib ${EXT_PLATFORM_SHLIBS_DIR}
      WORKING_DIRECTORY ${EXT_DEPS_INSTALL_DIR})
        set_target_properties(external_shlibs_audio PROPERTIES FOLDER EXTERNAL_SHLIBS)

        add_dependencies(aot_external_audio extempore external_shlibs_audio)
    endif()

elseif(WIN32 AND EXTERNAL_SHLIBS_GRAPHICS)
    aotcompile(aot_external_graphics)

    add_custom_target(external_shlibs_graphics
    COMMENT "moving .dll and .lib files into ${EXT_PLATFORM_SHLIBS_DIR}"
    DEPENDS assimp glfw3 stb_image nanovg
    COMMAND ${CMAKE_COMMAND} -E make_directory ${EXT_PLATFORM_SHLIBS_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy bin/assimp-vc130-mt.dll ${EXT_PLATFORM_SHLIBS_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy lib/assimp-vc130-mt.lib ${EXT_PLATFORM_SHLIBS_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy lib/glfw3.dll ${EXT_PLATFORM_SHLIBS_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy lib/glfw3dll.lib ${EXT_PLATFORM_SHLIBS_DIR}/glfw3.lib
    COMMAND ${CMAKE_COMMAND} -E copy lib/nanovg.dll ${EXT_PLATFORM_SHLIBS_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy lib/nanovg.lib ${EXT_PLATFORM_SHLIBS_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy lib/stb_image.dll ${EXT_PLATFORM_SHLIBS_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy lib/stb_image.lib ${EXT_PLATFORM_SHLIBS_DIR}
    WORKING_DIRECTORY ${EXT_DEPS_INSTALL_DIR})
    set_target_properties(external_shlibs_graphics PROPERTIES FOLDER EXTERNAL_SHLIBS)

    add_dependencies(aot_external_graphics extempore external_shlibs_graphics)
endif()

if(APPLE)
    add_custom_command(TARGET extempore POST_BUILD
    COMMENT "clear all file attributes (to avoid the 'extempore is damaged' error on Big Sur)"
    COMMAND xattr -cr "$<TARGET_FILE_DIR:extempore>")
endif()

#########
# tests #
#########

if(BUILD_TESTS)
    include(CTest)

    macro(extempore_add_test testfile label)
        extempore_get_next_port(_port)
        add_test(NAME ${testfile}
      COMMAND extempore --noaudio --term nocolor --port=${_port}
        --eval "(xtmtest-run-tests \"${testfile}\" #t #t)")
        set_tests_properties(${testfile} PROPERTIES
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      LABELS ${label})
    endmacro()

    macro(extempore_add_example_as_test examplefile timeout label)
        extempore_get_next_port(_port)
        add_test(NAME ${examplefile}
      COMMAND extempore --noaudio --term nocolor --port=${_port}
        --eval "(sys:load-then-quit \"${examplefile}\" ${timeout})")
        set_tests_properties(${examplefile} PROPERTIES
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      TIMEOUT 300
      LABELS ${label})
    endmacro()

    # tests - core
    extempore_add_test(tests/core/system.xtm libs-core)
    extempore_add_test(tests/core/adt.xtm libs-core)
    extempore_add_test(tests/core/math.xtm libs-core)
    extempore_add_test(tests/core/std.xtm libs-core)
    extempore_add_test(tests/core/xtlang.xtm libs-core)
    extempore_add_test(tests/core/generics.xtm libs-core)
    # tests - external
    extempore_add_test(tests/external/fft.xtm libs-external)
    # examples - core
    extempore_add_example_as_test(examples/core/audio_101.xtm 10 examples-audio)
    extempore_add_example_as_test(examples/core/fmsynth.xtm 10 examples-audio)
    extempore_add_example_as_test(examples/core/mtaudio.xtm 10 examples-audio)
    extempore_add_example_as_test(examples/core/nbody_lang_shootout.xtm 10 examples-core)
    extempore_add_example_as_test(examples/core/scheduler.xtm 10 examples-audio)
    extempore_add_example_as_test(examples/core/topclock_metro.xtm 10 examples-audio)
    extempore_add_example_as_test(examples/core/typeclasses.xtm 10 examples-core)
    extempore_add_example_as_test(examples/core/xthread.xtm 10 examples-core)
    # examples - external
    extempore_add_example_as_test(examples/external/audio_player.xtm 10 examples-audio)
    extempore_add_example_as_test(examples/external/convolution_reverb.xtm 10 examples-audio)
    extempore_add_example_as_test(examples/external/electrofunk.xtm 10 examples-audio)
    extempore_add_example_as_test(examples/external/gl-compatibility.xtm 10 examples-graphics)
    extempore_add_example_as_test(examples/external/granulator.xtm 10 examples-audio)
    extempore_add_example_as_test(examples/external/openvg.xtm 10 examples-graphics)
    extempore_add_example_as_test(examples/external/portmidi-output.xtm 10 examples-audio)
    extempore_add_example_as_test(examples/external/portmidi.xtm 10 examples-audio)
    extempore_add_example_as_test(examples/external/raymarcher.xtm 10 examples-graphics)
    extempore_add_example_as_test(examples/external/sampler.xtm 10 examples-audio)
    extempore_add_example_as_test(examples/external/shader-tutorials/arrows.xtm 10 examples-graphics)
    extempore_add_example_as_test(examples/external/shader-tutorials/framebuffer.xtm 10 examples-graphics)
    extempore_add_example_as_test(examples/external/shader-tutorials/heatmap.xtm 10 examples-graphics)
    extempore_add_example_as_test(examples/external/shader-tutorials/particles.xtm 10 examples-graphics)
    extempore_add_example_as_test(examples/external/shader-tutorials/points.xtm 10 examples-graphics)
    extempore_add_example_as_test(examples/external/shader-tutorials/shadertoy.xtm 10 examples-graphics)
    extempore_add_example_as_test(examples/external/shader-tutorials/simple-triangle.xtm 10 examples-graphics)
    extempore_add_example_as_test(examples/external/shader-tutorials/texture.xtm 10 examples-graphics)
    extempore_add_example_as_test(examples/external/shader-tutorials/triangle.xtm 10 examples-graphics)
    extempore_add_example_as_test(examples/external/sing_a_song.xtm 10 examples-audio)
    extempore_add_example_as_test(examples/external/spectrogram.xtm 10 examples-graphics)
    extempore_add_example_as_test(examples/external/xtmrender1.xtm 10 examples-graphics)
    extempore_add_example_as_test(examples/external/xtmrender2.xtm 10 examples-graphics)
    extempore_add_example_as_test(examples/external/xtmrender3.xtm 10 examples-graphics)
    extempore_add_example_as_test(examples/external/xtmrender4.xtm 10 examples-graphics)
endif()

##########
# xtmdoc #
##########

add_custom_target(xtmdoc
  COMMAND extempore --port 17095
    --eval "(begin (sys:load \"libs/core/audio_dsp.xtm\") (sys:load \"libs/core/instruments.xtm\") (sys:load \"libs/core/math.xtm\") (sys:load \"libs/base/base.xtm\") (sys:load \"libs/external/fft.xtm\") (sys:load \"libs/external/gl.xtm\") (sys:load \"libs/external/glfw3.xtm\") (sys:load \"libs/external/instruments_ext.xtm\") (sys:load \"libs/external/nanovg.xtm\") (sys:load \"libs/external/sndfile.xtm\") (sys:load \"libs/external/stb_image.xtm\") (xtmdoc-export-caches-to-json \"/tmp/xtmdoc.json\" #f) (quit 0))"
  COMMENT "Generating xtmdoc output in /tmp/xtmdoc.json"
  VERBATIM)
add_dependencies(xtmdoc extempore)

#########
# cpack #
#########

set(CPACK_PACKAGE_NAME "Extempore")
set(CPACK_PACKAGE_VENDOR "Andrew Sorensen")
set(CPACK_PACKAGE_CONTACT "Ben Swift")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
string(TIMESTAMP EXTEMPORE_BUILD_DATE "%Y%m%d")
set(CPACK_PACKAGE_FILE_NAME extempore)
set(CPACK_GENERATOR ZIP)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "The Extempore programming environment (https://extemporelang.github.io)")

include(CPack)
