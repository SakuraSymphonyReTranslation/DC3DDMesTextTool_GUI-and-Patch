cmake_minimum_required(VERSION 3.15)
project(DC3DDPatch)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_CXX_STANDARD 20)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Error: Building for x64, but x86 expected. Use x86 Native Tools Command Prompt.")
endif()

option(BUILD_DC4PH "Build for Da Capo 4 Plus Harmony" OFF)

if(BUILD_DC4PH)
    message(STATUS "Configuring build for Da Capo 4 Plus Harmony...")
endif()

# Detours library
set(DETOURS_DIR "${CMAKE_SOURCE_DIR}/Detours/src")
include_directories(${DETOURS_DIR})

add_library(detours STATIC
    ${DETOURS_DIR}/detours.cpp
    ${DETOURS_DIR}/modules.cpp
    ${DETOURS_DIR}/disasm.cpp
    ${DETOURS_DIR}/image.cpp
    ${DETOURS_DIR}/creatwth.cpp
    ${DETOURS_DIR}/disolx86.cpp
)

# ============================================================================
# Patch DLL - injected by launcher
# ============================================================================
add_library(DC3DDPatch SHARED
    src/dc4patch.cpp
)

target_link_libraries(DC3DDPatch PRIVATE 
    detours
    gdi32
    user32
    advapi32
    comdlg32
)

set_target_properties(DC3DDPatch PROPERTIES
    LINK_FLAGS "/DYNAMICBASE:NO"
)

if(BUILD_DC4PH)
    set_target_properties(DC3DDPatch PROPERTIES OUTPUT_NAME "DC4PHPatch")
    target_compile_definitions(DC3DDPatch PRIVATE GAME_DC4PH)
else()
    set_target_properties(DC3DDPatch PROPERTIES OUTPUT_NAME "DC3DDPatch")
    target_compile_definitions(DC3DDPatch PRIVATE GAME_DC3DD)
endif()

# ============================================================================
# Launcher - starts the game and injects patch DLL (no Locale Emulator dependency)
# ============================================================================
add_executable(DC3DDLauncher WIN32
    src/launcher.cpp
    src/launcher.rc
)

target_include_directories(DC3DDLauncher PRIVATE src)

target_link_libraries(DC3DDLauncher PRIVATE
    user32
    advapi32
)

if(BUILD_DC4PH)
    set_target_properties(DC3DDLauncher PROPERTIES OUTPUT_NAME "DC4PHLauncher")
    target_compile_definitions(DC3DDLauncher PRIVATE GAME_DC4PH)
else()
    set_target_properties(DC3DDLauncher PROPERTIES OUTPUT_NAME "DC3DDLauncher")
    target_compile_definitions(DC3DDLauncher PRIVATE GAME_DC3DD)
endif()
