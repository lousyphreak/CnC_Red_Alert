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

set(RA_COMMON_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/SDL3_COMPAT/wrappers
    ${CMAKE_SOURCE_DIR}/CODE
    ${CMAKE_SOURCE_DIR}/WIN32LIB/INCLUDE
)

set(RA_CASEFIX_SOURCE_DIRS
    ${CMAKE_SOURCE_DIR}/CODE
    ${CMAKE_SOURCE_DIR}/WIN32LIB/INCLUDE
    ${CMAKE_SOURCE_DIR}/WIN32LIB/AUDIO
    ${CMAKE_SOURCE_DIR}/WIN32LIB/DIPTHONG
    ${CMAKE_SOURCE_DIR}/WIN32LIB/DRAWBUFF
    ${CMAKE_SOURCE_DIR}/WIN32LIB/FONT
    ${CMAKE_SOURCE_DIR}/WIN32LIB/IFF
    ${CMAKE_SOURCE_DIR}/WIN32LIB/KEYBOARD
    ${CMAKE_SOURCE_DIR}/WIN32LIB/MEM
    ${CMAKE_SOURCE_DIR}/WIN32LIB/MISC
    ${CMAKE_SOURCE_DIR}/WIN32LIB/MONO
    ${CMAKE_SOURCE_DIR}/WIN32LIB/PALETTE
    ${CMAKE_SOURCE_DIR}/WIN32LIB/PLAYCD
    ${CMAKE_SOURCE_DIR}/WIN32LIB/PROFILE
    ${CMAKE_SOURCE_DIR}/WIN32LIB/RAWFILE
    ${CMAKE_SOURCE_DIR}/WIN32LIB/SHAPE
    ${CMAKE_SOURCE_DIR}/WIN32LIB/TILE
    ${CMAKE_SOURCE_DIR}/WIN32LIB/TIMER
    ${CMAKE_SOURCE_DIR}/WIN32LIB/VQA32
    ${CMAKE_SOURCE_DIR}/WIN32LIB/WSA
    ${CMAKE_SOURCE_DIR}/WIN32LIB/WW_WIN
    ${CMAKE_SOURCE_DIR}/WIN32LIB/WINCOMM
)

set(RA_CASEFIX_INCLUDE_DIRS "")
foreach(ra_casefix_dir IN LISTS RA_CASEFIX_SOURCE_DIRS)
    file(RELATIVE_PATH ra_casefix_rel ${CMAKE_SOURCE_DIR} ${ra_casefix_dir})
    string(REPLACE "/" "__" ra_casefix_rel_sanitized ${ra_casefix_rel})
    set(ra_casefix_out_dir "${CMAKE_BINARY_DIR}/generated/casefix/${ra_casefix_rel_sanitized}")
    file(MAKE_DIRECTORY "${ra_casefix_out_dir}")
    file(GLOB ra_casefix_headers CONFIGURE_DEPENDS "${ra_casefix_dir}/*.H")
    foreach(ra_casefix_header IN LISTS ra_casefix_headers)
        get_filename_component(ra_casefix_name "${ra_casefix_header}" NAME)
        get_filename_component(ra_casefix_stem "${ra_casefix_header}" NAME_WE)
        get_filename_component(ra_casefix_ext "${ra_casefix_header}" EXT)
        string(TOLOWER "${ra_casefix_name}" ra_casefix_lower)
        string(TOLOWER "${ra_casefix_stem}" ra_casefix_stem_lower)
        string(TOLOWER "${ra_casefix_ext}" ra_casefix_ext_lower)
        file(WRITE "${ra_casefix_out_dir}/${ra_casefix_lower}" "#include \"${ra_casefix_header}\"\n")
        file(WRITE "${ra_casefix_out_dir}/${ra_casefix_stem}${ra_casefix_ext_lower}" "#include \"${ra_casefix_header}\"\n")
        file(WRITE "${ra_casefix_out_dir}/${ra_casefix_stem_lower}${ra_casefix_ext}" "#include \"${ra_casefix_header}\"\n")
    endforeach()
    list(APPEND RA_CASEFIX_INCLUDE_DIRS "${ra_casefix_out_dir}" "${ra_casefix_dir}")
endforeach()

list(APPEND RA_COMMON_INCLUDE_DIRS ${RA_CASEFIX_INCLUDE_DIRS})

set(RA_GAME_INCLUDE_DIRS
    ${RA_COMMON_INCLUDE_DIRS}
)

file(GLOB RA_GAME_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/CODE/*.CPP"
)
list(FILTER RA_GAME_SOURCES EXCLUDE REGEX "/ALLOC\\.CPP$")
list(FILTER RA_GAME_SOURCES EXCLUDE REGEX "/BMP8\\.CPP$")
list(FILTER RA_GAME_SOURCES EXCLUDE REGEX "/DIBFILE\\.CPP$")
list(FILTER RA_GAME_SOURCES EXCLUDE REGEX "/DIBUTIL\\.CPP$")
list(FILTER RA_GAME_SOURCES EXCLUDE REGEX "/DTABLE\\.CPP$")
list(FILTER RA_GAME_SOURCES EXCLUDE REGEX "/DPMI\\.CPP$")
list(FILTER RA_GAME_SOURCES EXCLUDE REGEX "/GETCPU\\.CPP$")
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
list(APPEND RA_GAME_SOURCES
    "${CMAKE_SOURCE_DIR}/CODE/DIBCOMPAT.CPP"
)

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

add_library(sdl3_compat STATIC
    SDL3_COMPAT/wrappers/win32_compat.cpp
    SDL3_COMPAT/wrappers/sdl_fs.cpp
    SDL3_COMPAT/wrappers/sdl_draw.cpp
    SDL3_COMPAT/wrappers/profile_compat.cpp
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

foreach(target sdl3_compat win32lib redalert)
    if(MSVC)
        target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
        target_compile_options(${target} PRIVATE /w)
    else()
        if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
            target_compile_definitions(${target} PRIVATE __cdecl=)
        endif()
        target_compile_options(${target} PRIVATE -w -fpermissive)
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
    foreach(target sdl3_compat win32lib redalert)
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

target_link_libraries(redalert
    PRIVATE
        win32lib
        sdl3_compat
        ${RA_SDL_TARGET}
        Threads::Threads
        m
)

add_custom_command(TARGET redalert POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:redalert>
        "${CMAKE_SOURCE_DIR}/GameData/${RA_RUNTIME_COPY_NAME}${CMAKE_EXECUTABLE_SUFFIX}"
    VERBATIM
)
