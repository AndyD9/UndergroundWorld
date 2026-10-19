cmake_minimum_required(VERSION 3.24)
project(UndergroundWorld VERSION 0.1.0 LANGUAGES CXX)
option(UGWORLD_BUILD_CLIENT "Build Windows x86 diagnostic ASI" ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# Static CRT: players must not need the x86 VC++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
if(UGWORLD_BUILD_CLIENT)
  if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "UGWorld.asi requires Windows x86: configure with -A Win32")
  endif()
  add_library(UGWorld SHARED
    client/src/core/plugin.cpp client/src/core/identity.cpp client/src/core/log.cpp
    client/src/game/frame_hook.cpp client/src/game/telemetry.cpp)
  target_compile_definitions(UGWorld PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
  target_link_libraries(UGWorld PRIVATE bcrypt)
  set_target_properties(UGWorld PROPERTIES PREFIX "" SUFFIX ".asi")
  if(MSVC)
    target_compile_options(UGWorld PRIVATE /W4 /permissive-)
  endif()
endif()
# Server and transport deliberately remain design documents until the local car milestone.
