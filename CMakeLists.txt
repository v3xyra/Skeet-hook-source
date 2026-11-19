cmake_minimum_required(VERSION 3.21)
project(skeet_reconstructed LANGUAGES C CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(SKEET_COMPILE_JOBS 2 CACHE STRING "Maximum simultaneous client compilations")
set(SKEET_LINK_THREADS 2 CACHE STRING "Maximum LLVM linker threads")
foreach(_skeet_limit SKEET_COMPILE_JOBS SKEET_LINK_THREADS)
  if(NOT "${${_skeet_limit}}" MATCHES "^[1-9][0-9]*$")
    message(FATAL_ERROR "${_skeet_limit} must be a positive integer")
  endif()
endforeach()
set_property(GLOBAL APPEND PROPERTY JOB_POOLS "skeet_compile=${SKEET_COMPILE_JOBS}" "skeet_link=1")

if(NOT WIN32)
  message(FATAL_ERROR "The native client requires a Windows x86 toolchain.")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR "The native client requires 32-bit pointers. Use the native-core-x86 preset in a separate build directory.")
endif()
add_library(skeet_core STATIC)
set_target_properties(skeet_core PROPERTIES JOB_POOL_COMPILE skeet_compile JOB_POOL_LINK skeet_link)
include(cmake/client-sources.cmake)
set_source_files_properties(third_party/miniz/miniz.c PROPERTIES
  COMPILE_DEFINITIONS "MINIZ_NO_STDIO;MINIZ_NO_ARCHIVE_APIS;MINIZ_NO_ZLIB_COMPATIBLE_NAMES")

target_include_directories(skeet_core PUBLIC include)
target_compile_features(skeet_core PUBLIC cxx_std_20)
include(cmake/luajit-x86.cmake)
target_link_libraries(skeet_core PUBLIC user32 gdi32 ntdll shell32 ole32 advapi32)
if(MSVC)
  target_compile_options(skeet_core PRIVATE /W4 /WX /fp:precise)
else()
  target_compile_options(skeet_core PRIVATE -Wall -Wextra -Werror -ffp-contract=off)
  set_source_files_properties(third_party/miniz/miniz.c PROPERTIES COMPILE_OPTIONS -Wno-unused-function)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(skeet_core PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:-fms-extensions>")
  endif()
endif()
# The DLL owns the recovered application and uses the ordinary Windows loader
# lifecycle. It never maps the captured client or executes retained VM code.
add_library(skeet_client SHARED src/client/source_dll.cpp)
set_target_properties(skeet_client PROPERTIES JOB_POOL_COMPILE skeet_compile JOB_POOL_LINK skeet_link)
target_link_libraries(skeet_client PRIVATE skeet_core)
target_compile_definitions(skeet_client PRIVATE SKEET_SOURCE_DLL_BUILD)
set_target_properties(skeet_client PROPERTIES
  PREFIX ""
  OUTPUT_NAME "skeet_client"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
if(MSVC)
  target_compile_options(skeet_client PRIVATE /W4 /WX /fp:precise)
else()
  target_compile_options(skeet_client PRIVATE -Wall -Wextra -Werror -ffp-contract=off -fms-extensions)
  target_link_options(skeet_client PRIVATE -static -Wl,--exclude-all-symbols
    -Wl,--no-insert-timestamp -Wl,--dynamicbase -Wl,--nxcompat)
endif()
if(MINGW AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  target_link_options(skeet_client PRIVATE "-Wl,--threads=${SKEET_LINK_THREADS}")
  # Force every archive member into the link so missing definitions cannot
  # hide behind static archive extraction. This is an internal link check;
  # it has no client bootstrap or exported interface and is never deployed.
  set(_skeet_linkcheck "${CMAKE_CURRENT_BINARY_DIR}/skeet_core.linkcheck.dll")
  add_custom_command(OUTPUT "${_skeet_linkcheck}"
    COMMAND "${CMAKE_CXX_COMPILER}" -static -shared
      -Wl,--whole-archive "$<TARGET_FILE:skeet_core>" -Wl,--no-whole-archive
      "${_skeet_luajit_archive}"
      -Wl,--exclude-all-symbols -Wl,--no-insert-timestamp
      "-Wl,--threads=${SKEET_LINK_THREADS}"
      -luser32 -lgdi32 -lntdll -lshell32 -lole32 -ladvapi32 -o "${_skeet_linkcheck}"
    DEPENDS skeet_core "${_skeet_luajit_archive}"
    COMMENT "Checking all recovered x86 source definitions at link time"
    JOB_POOL skeet_link
    VERBATIM)
  add_custom_target(skeet_core_linkcheck DEPENDS "${_skeet_linkcheck}")
  add_dependencies(skeet_client skeet_core_linkcheck)
endif()
