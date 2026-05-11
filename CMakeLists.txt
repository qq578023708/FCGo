cmake_minimum_required(VERSION 3.16)
project(FCGo VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Qt6 path - adjust this to your Qt installation
if(NOT DEFINED CMAKE_PREFIX_PATH)
    set(CMAKE_PREFIX_PATH "D:/Qt/6.11.0/msvc2022_64")
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets OpenGL OpenGLWidgets Network)

# Translations (optional - don't fail if LinguistTools not found)
find_package(Qt6 QUIET COMPONENTS LinguistTools)

# Windows specific
if(WIN32)
    add_definitions(-DWIN32_LEAN_AND_MEAN -DNOMINMAX)
    # Force UTF-8 source encoding for MSVC
    add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
    add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
endif()

# SDL2
find_package(SDL2 QUIET)
if(NOT SDL2_FOUND)
    set(SDL2_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/SDL2/include")
    set(SDL2_LIBRARY "${CMAKE_CURRENT_SOURCE_DIR}/third_party/SDL2/lib/x64/SDL2.lib")
endif()

# ============================================================
# Source files organized by module
# ============================================================

# Core (NES emulation)
set(CORE_SOURCES
    src/core/cpu/cpu6502.cpp
    src/core/ppu/ppu.cpp
    src/core/apu/apu.cpp
    src/core/mapper/mapper.cpp
    src/core/nes_console.cpp
    src/core/debug/breakpoint.cpp
)
set(CORE_HEADERS
    src/core/types.h
    src/core/cpu/cpu6502.h
    src/core/ppu/ppu.h
    src/core/apu/apu.h
    src/core/mapper/mapper.h
    src/core/mapper/mappers/mapper0.h
    src/core/mapper/mappers/mapper1.h
    src/core/mapper/mappers/mapper2.h
    src/core/mapper/mappers/mapper3.h
    src/core/mapper/mappers/mapper4.h
    src/core/memory/bus.h
    src/core/nes_console.h
    src/core/debug/breakpoint.h
)

# Qt UI
set(QT_SOURCES
    src/qt/main.cpp
    src/qt/mainwindow.cpp
    src/qt/render_widget.cpp
    src/qt/settings_dialog.cpp
    src/qt/input_manager.cpp
)
set(QT_HEADERS
    src/qt/mainwindow.h
    src/qt/render_widget.h
    src/qt/settings_dialog.h
    src/qt/input_manager.h
)

# Audio module
set(AUDIO_SOURCES
    src/qt/audio/sdl_audio_backend.cpp
    src/qt/audio/audio_factory.cpp
)
set(AUDIO_HEADERS
    src/qt/audio/iaudio_backend.h
    src/qt/audio/sdl_audio_backend.h
    src/qt/audio/audio_factory.h
)

# Debugger module
set(DEBUGGER_SOURCES
    src/qt/debugger/memory_viewer.cpp
    src/qt/debugger/memory_search.cpp
    src/qt/debugger/disassembler.cpp
    src/qt/debugger/debugger_window.cpp
    src/qt/debugger/ppu_viewer.cpp
    src/qt/debugger/breakpoint_window.cpp
)
set(DEBUGGER_HEADERS
    src/qt/debugger/memory_viewer.h
    src/qt/debugger/memory_search.h
    src/qt/debugger/disassembler.h
    src/qt/debugger/debugger_window.h
    src/qt/debugger/ppu_viewer.h
    src/qt/debugger/breakpoint_window.h
)

# Network module (Steamworks integration)
set(NETWORK_SOURCES
    src/qt/network/steam_manager.cpp
    src/qt/network/network_lobby_window.cpp
)
set(NETWORK_HEADERS
    src/qt/network/steam_manager.h
    src/qt/network/network_lobby_window.h
)

# Try to find Steamworks SDK
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/steamworks/sdk/public/steam/steam_api.h")
    set(STEAMWORKS_FOUND TRUE)
    set(STEAMWORKS_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/steamworks/sdk/public")
    if(WIN32)
        set(STEAMWORKS_LIBRARY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/steamworks/sdk/redistributable_bin/win64")
    endif()
    message(STATUS "Steamworks SDK found")
else()
    set(STEAMWORKS_FOUND FALSE)
    message(STATUS "Steamworks SDK not found - network multiplayer disabled")
endif()

# Main executable (WIN32 = no console window)
add_executable(FCGo WIN32
    ${CORE_SOURCES} ${CORE_HEADERS}
    ${QT_SOURCES} ${QT_HEADERS}
    ${AUDIO_SOURCES} ${AUDIO_HEADERS}
    ${DEBUGGER_SOURCES} ${DEBUGGER_HEADERS}
    ${RESOURCE_FILES}
)

# Add network module (always compiled, stub when SDK not found)
target_sources(FCGo PRIVATE ${NETWORK_SOURCES} ${NETWORK_HEADERS})

# Add Steamworks SDK if available
if(STEAMWORKS_FOUND)
    target_compile_definitions(FCGo PRIVATE STEAMWORKS_AVAILABLE)
    target_include_directories(FCGo PRIVATE ${STEAMWORKS_INCLUDE_DIR})
    target_link_directories(FCGo PRIVATE ${STEAMWORKS_LIBRARY_DIR})
    target_link_libraries(FCGo PRIVATE steam_api64)
endif()

# ============================================================
# Source groups for Visual Studio
# ============================================================
source_group("Core\\CPU" FILES src/core/cpu/cpu6502.cpp src/core/cpu/cpu6502.h)
source_group("Core\\PPU" FILES src/core/ppu/ppu.cpp src/core/ppu/ppu.h)
source_group("Core\\APU" FILES src/core/apu/apu.cpp src/core/apu/apu.h)
source_group("Core\\Mapper" FILES 
    src/core/mapper/mapper.cpp src/core/mapper/mapper.h
    src/core/mapper/mappers/mapper0.h
    src/core/mapper/mappers/mapper1.h
    src/core/mapper/mappers/mapper2.h
    src/core/mapper/mappers/mapper3.h
    src/core/mapper/mappers/mapper4.h
)
source_group("Core\\Memory" FILES src/core/memory/bus.h)
source_group("Core" FILES src/core/types.h src/core/nes_console.cpp src/core/nes_console.h)

source_group("Qt\\UI" FILES 
    src/qt/main.cpp
    src/qt/mainwindow.cpp src/qt/mainwindow.h
    src/qt/render_widget.cpp src/qt/render_widget.h
    src/qt/settings_dialog.cpp src/qt/settings_dialog.h
    src/qt/input_manager.cpp src/qt/input_manager.h
)

source_group("Qt\\Audio" FILES 
    src/qt/audio/iaudio_backend.h
    src/qt/audio/sdl_audio_backend.cpp src/qt/audio/sdl_audio_backend.h
    src/qt/audio/audio_factory.cpp src/qt/audio/audio_factory.h
)

source_group("Qt\\Debugger" FILES 
    src/qt/debugger/memory_viewer.cpp src/qt/debugger/memory_viewer.h
    src/qt/debugger/memory_search.cpp src/qt/debugger/memory_search.h
    src/qt/debugger/disassembler.cpp src/qt/debugger/disassembler.h
    src/qt/debugger/debugger_window.cpp src/qt/debugger/debugger_window.h
    src/qt/debugger/ppu_viewer.cpp src/qt/debugger/ppu_viewer.h
    src/qt/debugger/breakpoint_window.cpp src/qt/debugger/breakpoint_window.h
)

source_group("Qt\\Network" FILES
    src/qt/network/steam_manager.cpp src/qt/network/steam_manager.h
    src/qt/network/network_lobby_window.cpp src/qt/network/network_lobby_window.h
)

# Resources
set(RESOURCE_FILES
    resources/icons/pause.png
    resources/icons/step.png
    resources/icons/step_over.png
    resources/icons/step_frame.png
)
source_group("Resources\\Icons" FILES ${RESOURCE_FILES})

# ============================================================
# Include directories and linking
# ============================================================
target_include_directories(FCGo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${SDL2_INCLUDE_DIR}
)

target_link_libraries(FCGo PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    Qt6::Network
    ${SDL2_LIBRARY}
)

# Copy SDL2.dll to output directory
if(WIN32)
    add_custom_command(TARGET FCGo POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/SDL2/lib/x64/SDL2.dll"
        $<TARGET_FILE_DIR:FCGo>
    )
    # Copy Steam API DLLs
    if(STEAMWORKS_FOUND)
        add_custom_command(TARGET FCGo POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${STEAMWORKS_LIBRARY_DIR}/steam_api64.dll"
            $<TARGET_FILE_DIR:FCGo>
        )
        add_custom_command(TARGET FCGo POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/steamworks/sdk/redistributable_bin/steam_api.dll"
            $<TARGET_FILE_DIR:FCGo>
        )
    endif()
endif()

# Translations processing using Qt6 recommended approach
if(Qt6LinguistTools_FOUND)
    set(TS_FILES
        src/qt/translations/fcgo_zh_CN.ts
    )
    
    # Use qt_add_lrelease to generate .qm files at build time
    qt_add_lrelease(FCGo
        TS_FILES ${TS_FILES}
        QM_FILES_OUTPUT_VARIABLE QM_FILES
    )
    
    # Add the generated .qm files as resources
    # Note: qt_add_resources will handle the dependency automatically
    qt_add_resources(FCGo "translations"
        PREFIX "/"
        BASE "${CMAKE_CURRENT_BINARY_DIR}"
        FILES ${QM_FILES}
    )
endif()

# Add icon resources
qt_add_resources(FCGo "icons"
    PREFIX "/"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}"
    FILES
        resources/icons/pause.png
        resources/icons/step.png
        resources/icons/step_over.png
        resources/icons/step_frame.png
)

# Auto-deploy Qt and runtime DLLs to build directory after build
if(WIN32)
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/deploy_qt.cmake OPTIONAL)
    if(TARGET Qt6::Core)
        add_custom_command(TARGET FCGo POST_BUILD
            COMMAND ${CMAKE_COMMAND} -DCMAKE_PREFIX_PATH="${CMAKE_PREFIX_PATH}"
                    -DCMAKE_BUILD_TYPE=$<CONFIGURATION>
                    -DQT_DEPLOY_TARGET=$<TARGET_FILE:FCGo>
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/deploy_qt.cmake
            COMMENT "Deploying Qt and runtime DLLs..."
        )
    endif()
endif()
