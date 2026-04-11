cmake_minimum_required(VERSION 3.24)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build." FORCE)
  # Set the possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

project(RedAlert VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(RA_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(RA_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(RA_ENABLE_UDP "Enable UDP multiplayer transport" ON)
option(RA_ENABLE_DVD_MOVIES "Enable DVD-specific movie handling" OFF)
option(RA_EMSCRIPTEN_PACKAGE_GAMEDATA "Package the GameData directory into the emscripten output" OFF)
option(RA_EMSCRIPTEN_LAZY_FETCH_GAMEDATA "Load GameData over HTTP on demand in the emscripten build" ON)

if(EMSCRIPTEN)
    set(RA_ENABLE_UDP OFF CACHE BOOL "Enable UDP multiplayer transport" FORCE)
endif()

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
set(SDL_DISABLE_INSTALL OFF CACHE BOOL "" FORCE)

add_subdirectory(extern/SDL3 EXCLUDE_FROM_ALL)

find_package(Threads REQUIRED)

set(RA_COMMON_DEFINITIONS
    ENGLISH=1
    FIXIT_MULTI_SAVE
    FIXIT_NAME_OVERRIDE
    FIXIT_SCORE_CRASH
    RELEASE_VERSION=0
)

if(RA_ENABLE_UDP)
    list(APPEND RA_COMMON_DEFINITIONS RA_ENABLE_UDP=1)
else()
    list(APPEND RA_COMMON_DEFINITIONS RA_ENABLE_UDP=0)
endif()

if(RA_ENABLE_DVD_MOVIES)
    list(APPEND RA_COMMON_DEFINITIONS RA_ENABLE_DVD_MOVIES=1)
else()
    list(APPEND RA_COMMON_DEFINITIONS RA_ENABLE_DVD_MOVIES=0)
endif()

if(EMSCRIPTEN)
    list(APPEND RA_COMMON_DEFINITIONS INTERNET_OFF)
endif()

if(EMSCRIPTEN AND RA_EMSCRIPTEN_LAZY_FETCH_GAMEDATA)
    list(APPEND RA_COMMON_DEFINITIONS RA_EMSCRIPTEN_LAZY_FETCH_GAMEDATA=1)
else()
    list(APPEND RA_COMMON_DEFINITIONS RA_EMSCRIPTEN_LAZY_FETCH_GAMEDATA=0)
endif()

set(RA_COMMON_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/SDL3_COMPAT/wrappers
    ${CMAKE_SOURCE_DIR}/CODE
    ${CMAKE_SOURCE_DIR}/WIN32LIB/INCLUDE
)

set(RA_GAME_INCLUDE_DIRS
    ${RA_COMMON_INCLUDE_DIRS}
)

file(GLOB RA_GAME_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/CODE/*.CPP"
)
list(FILTER RA_GAME_SOURCES EXCLUDE REGEX "/ALLOC\\.CPP$")
list(FILTER RA_GAME_SOURCES EXCLUDE REGEX "/DTABLE\\.CPP$")
list(FILTER RA_GAME_SOURCES EXCLUDE REGEX "/ITABLE\\.CPP$")
list(FILTER RA_GAME_SOURCES EXCLUDE REGEX "/KEYFRAME\\.CPP$")
list(FILTER RA_GAME_SOURCES EXCLUDE REGEX "/KEYBOARD\\.CPP$")
list(FILTER RA_GAME_SOURCES EXCLUDE REGEX "/(_WSPROTO|WSPROTO|WSPUDP)\\.CPP$")
list(FILTER RA_GAME_SOURCES EXCLUDE REGEX "/LZWOTRAW\\.CPP$")
list(FILTER RA_GAME_SOURCES EXCLUDE REGEX "/MPLIB\\.CPP$")
list(FILTER RA_GAME_SOURCES EXCLUDE REGEX "/MPMGRD\\.CPP$")
list(FILTER RA_GAME_SOURCES EXCLUDE REGEX "/MPLPC\\.CPP$")
list(FILTER RA_GAME_SOURCES EXCLUDE REGEX "/NULL(CONN|MGR)\\.CPP$")
list(FILTER RA_GAME_SOURCES EXCLUDE REGEX "/TARCOM\\.CPP$")
list(FILTER RA_GAME_SOURCES EXCLUDE REGEX "/TEMP\\.CPP$")
list(FILTER RA_GAME_SOURCES EXCLUDE REGEX "/TURRET\\.CPP$")

set(RA_WIN32LIB_GLOBS
    "${CMAKE_SOURCE_DIR}/WIN32LIB/AUDIO/*.CPP"
    "${CMAKE_SOURCE_DIR}/WIN32LIB/DIPTHONG/*.CPP"
    "${CMAKE_SOURCE_DIR}/WIN32LIB/DRAWBUFF/*.CPP"
    "${CMAKE_SOURCE_DIR}/WIN32LIB/FONT/*.CPP"
    "${CMAKE_SOURCE_DIR}/WIN32LIB/IFF/*.CPP"
    "${CMAKE_SOURCE_DIR}/WIN32LIB/KEYBOARD/*.CPP"
    "${CMAKE_SOURCE_DIR}/WIN32LIB/MEM/*.CPP"
    "${CMAKE_SOURCE_DIR}/WIN32LIB/MISC/*.CPP"
    "${CMAKE_SOURCE_DIR}/WIN32LIB/MONO/*.CPP"
    "${CMAKE_SOURCE_DIR}/WIN32LIB/PALETTE/*.CPP"
    "${CMAKE_SOURCE_DIR}/WIN32LIB/PLAYCD/*.CPP"
    "${CMAKE_SOURCE_DIR}/WIN32LIB/PROFILE/*.CPP"
    "${CMAKE_SOURCE_DIR}/WIN32LIB/RAWFILE/*.CPP"
    "${CMAKE_SOURCE_DIR}/WIN32LIB/SHAPE/*.CPP"
    "${CMAKE_SOURCE_DIR}/WIN32LIB/TILE/*.CPP"
    "${CMAKE_SOURCE_DIR}/WIN32LIB/TIMER/*.CPP"
    "${CMAKE_SOURCE_DIR}/WIN32LIB/VQA32/*.CPP"
    "${CMAKE_SOURCE_DIR}/WIN32LIB/WSA/*.CPP"
    "${CMAKE_SOURCE_DIR}/WIN32LIB/WW_WIN/*.CPP"
)
file(GLOB RA_WIN32LIB_SOURCES CONFIGURE_DEPENDS ${RA_WIN32LIB_GLOBS})

list(FILTER RA_WIN32LIB_SOURCES EXCLUDE REGEX "/(TEST|TST)\\.CPP$")
list(FILTER RA_WIN32LIB_SOURCES EXCLUDE REGEX "/(REDBOOK)\\.CPP$")
list(FILTER RA_WIN32LIB_SOURCES EXCLUDE REGEX "/(PROFILE)\.CPP$")
list(FILTER RA_WIN32LIB_SOURCES EXCLUDE REGEX "/(WPROFILE)\.CPP$")
list(FILTER RA_WIN32LIB_SOURCES EXCLUDE REGEX "/RAWFILE/.*\.CPP$")

list(FILTER RA_WIN32LIB_SOURCES EXCLUDE REGEX "/(TEST|TST|REDBOOK)\\.CPP$")

if(EMSCRIPTEN)
    set(RA_EMSCRIPTEN_EFFECTIVE_PACKAGE_GAMEDATA ${RA_EMSCRIPTEN_PACKAGE_GAMEDATA})
    if(RA_EMSCRIPTEN_LAZY_FETCH_GAMEDATA AND RA_EMSCRIPTEN_PACKAGE_GAMEDATA)
        message(STATUS "RA_EMSCRIPTEN_LAZY_FETCH_GAMEDATA is ON; skipping monolithic GameData preloading")
        set(RA_EMSCRIPTEN_EFFECTIVE_PACKAGE_GAMEDATA OFF)
    endif()

    if(RA_EMSCRIPTEN_LAZY_FETCH_GAMEDATA)
        file(GLOB_RECURSE RA_EMSCRIPTEN_GAMEDATA_ENTRIES
            RELATIVE "${CMAKE_SOURCE_DIR}/GameData"
            "${CMAKE_SOURCE_DIR}/GameData/*"
        )
        set(RA_EMSCRIPTEN_GAMEDATA_FILES "")
        foreach(ra_gamedata_entry IN LISTS RA_EMSCRIPTEN_GAMEDATA_ENTRIES)
            if(IS_DIRECTORY "${CMAKE_SOURCE_DIR}/GameData/${ra_gamedata_entry}")
                continue()
            endif()
            if(ra_gamedata_entry MATCHES "^(redalert|redalert-asan)(\\.exe)?$")
                continue()
            endif()
            string(REPLACE "\\" "/" ra_gamedata_entry_normalized "${ra_gamedata_entry}")
            list(APPEND RA_EMSCRIPTEN_GAMEDATA_FILES "${ra_gamedata_entry_normalized}")
        endforeach()
        list(SORT RA_EMSCRIPTEN_GAMEDATA_FILES)
        list(JOIN RA_EMSCRIPTEN_GAMEDATA_FILES "\n" RA_EMSCRIPTEN_GAMEDATA_MANIFEST_CONTENT)
        set(RA_EMSCRIPTEN_GAMEDATA_MANIFEST "${CMAKE_BINARY_DIR}/ra-assets-manifest.txt")
        if(RA_EMSCRIPTEN_GAMEDATA_MANIFEST_CONTENT)
            file(WRITE "${RA_EMSCRIPTEN_GAMEDATA_MANIFEST}" "${RA_EMSCRIPTEN_GAMEDATA_MANIFEST_CONTENT}\n")
        else()
            file(WRITE "${RA_EMSCRIPTEN_GAMEDATA_MANIFEST}" "")
        endif()
    endif()
endif()

add_library(sdl3_compat STATIC
    SDL3_COMPAT/wrappers/win32_compat.cpp
    SDL3_COMPAT/wrappers/sdl_config.cpp
    SDL3_COMPAT/wrappers/sdl_fs.cpp
    SDL3_COMPAT/wrappers/sdl_draw.cpp
)

target_include_directories(sdl3_compat
    PUBLIC
        ${RA_COMMON_INCLUDE_DIRS}
)

target_compile_definitions(sdl3_compat
    PUBLIC
        ${RA_COMMON_DEFINITIONS}
)

add_library(win32lib STATIC ${RA_WIN32LIB_SOURCES})

target_include_directories(win32lib
    PUBLIC
        ${RA_GAME_INCLUDE_DIRS}
)

target_compile_definitions(win32lib
    PUBLIC
        ${RA_COMMON_DEFINITIONS}
)

add_executable(redalert ${RA_GAME_SOURCES})

if(RA_ENABLE_ASAN)
    set(RA_RUNTIME_COPY_NAME "redalert-asan")
else()
    set(RA_RUNTIME_COPY_NAME "redalert")
endif()

target_include_directories(redalert
    PRIVATE
        ${RA_GAME_INCLUDE_DIRS}
)

target_compile_definitions(redalert
    PRIVATE
        ${RA_COMMON_DEFINITIONS}
)

set(RA_BUILD_TARGETS sdl3_compat win32lib redalert)

foreach(target ${RA_BUILD_TARGETS})
    if(MSVC)
        target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
        target_compile_options(${target} PRIVATE /w)
    else()
        if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
        endif()
        target_compile_options(${target} PRIVATE -Wall -fpermissive)
    endif()
endforeach()

if((CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang") AND (RA_ENABLE_ASAN OR RA_ENABLE_UBSAN))
    set(RA_SANITIZER_FLAGS "")
    if(RA_ENABLE_ASAN)
        string(APPEND RA_SANITIZER_FLAGS " -fsanitize=address")
    endif()
    if(RA_ENABLE_UBSAN)
        string(APPEND RA_SANITIZER_FLAGS " -fsanitize=undefined")
    endif()
    separate_arguments(RA_SANITIZER_FLAGS)
    foreach(target ${RA_BUILD_TARGETS})
        target_compile_options(${target} PRIVATE ${RA_SANITIZER_FLAGS})
        target_link_options(${target} PRIVATE ${RA_SANITIZER_FLAGS})
    endforeach()
endif()

if(TARGET SDL3::SDL3-static)
    set(RA_SDL_TARGET SDL3::SDL3-static)
elseif(TARGET SDL3-static)
    set(RA_SDL_TARGET SDL3-static)
else()
    message(FATAL_ERROR "Bundled SDL3 static target was not created")
endif()

target_link_libraries(sdl3_compat
    PUBLIC
        ${RA_SDL_TARGET}
)

target_link_libraries(win32lib
    PUBLIC
        ${RA_SDL_TARGET}
)

target_link_libraries(redalert
    PRIVATE
        win32lib
        sdl3_compat
        ${RA_SDL_TARGET}
        Threads::Threads
)

if(WIN32)
    target_link_libraries(redalert PRIVATE ws2_32)
else()
    target_link_libraries(redalert PRIVATE m)
endif()

if(EMSCRIPTEN)
    set_target_properties(redalert PROPERTIES SUFFIX ".html")
    target_link_options(redalert PRIVATE
        "-sASYNCIFY"
        "-sASYNCIFY_STACK_SIZE=1048576"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sINITIAL_MEMORY=268435456"
        "-sSTACK_SIZE=8388608"
        "-sMAX_WEBGL_VERSION=2"
        "-sMIN_WEBGL_VERSION=1"
        "-sENVIRONMENT=web"
    )
    if(RA_EMSCRIPTEN_LAZY_FETCH_GAMEDATA)
        target_link_options(redalert PRIVATE
            "-sFORCE_FILESYSTEM"
            "-lidbfs.js"
            "--embed-file"
            "${RA_EMSCRIPTEN_GAMEDATA_MANIFEST}@/ra-assets-manifest.txt"
        )
    endif()
    if(RA_EMSCRIPTEN_EFFECTIVE_PACKAGE_GAMEDATA)
        target_link_options(redalert PRIVATE
            "--preload-file"
            "${CMAKE_SOURCE_DIR}/GameData@/"
        )
    else()
        add_custom_command(TARGET redalert POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E rm -f "$<TARGET_FILE_DIR:redalert>/redalert.data"
            VERBATIM
        )
    endif()
else()
    add_custom_command(TARGET redalert POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:redalert>
            "${CMAKE_SOURCE_DIR}/GameData/${RA_RUNTIME_COPY_NAME}${CMAKE_EXECUTABLE_SUFFIX}"
        VERBATIM
    )
endif()
