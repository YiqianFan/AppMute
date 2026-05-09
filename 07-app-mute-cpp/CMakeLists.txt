cmake_minimum_required(VERSION 3.15)
project(MuteApp CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(/W3 /utf-8)

add_executable(MuteApp
    src/main.cpp
    src/config.cpp
    src/hotkey.cpp
    src/audio.cpp
    src/tray.cpp
    src/dialog.cpp
)

target_compile_options(MuteApp PRIVATE /EHsc)

target_link_libraries(MuteApp PRIVATE
    ole32 oleaut32 uuid ksuser Mmdevapi
    comctl32
    Psapi.lib
)

# manifest for visual styles
target_sources(MuteApp PRIVATE res/app.rc)

# app.rc for version info and manifest
add_custom_command(TARGET MuteApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:MuteApp> ${CMAKE_SOURCE_DIR}/build/MuteApp.exe
)

# manifest embedded via source
set_target_properties(MuteApp PROPERTIES
    VS_GLOBAL_Manifest "${CMAKE_SOURCE_DIR}/res/app.manifest"
)