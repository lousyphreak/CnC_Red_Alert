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
option(RA_ENABLE_TCP "Enable TCP multiplayer transport" ON)
option(RA_ENABLE_WOL "Enable WebSocket (WOL replacement) multiplayer transport" ON)
option(RA_ENABLE_DVD_MOVIES "Enable DVD-specific movie handling" OFF)
option(RA_EMSCRIPTEN_PACKAGE_GAMEDATA "Package the GameData directory into the emscripten output" OFF)
option(RA_EMSCRIPTEN_LAZY_FETCH_GAMEDATA "Load GameData over HTTP on demand in the emscripten build" ON)
option(RA_EMSCRIPTEN_DIAGNOSTIC "Enable Emscripten diagnostic build (ASSERTIONS, SAFE_HEAP, stack checks, symbol names)" OFF)
option(RA_ANDROID_PACKAGE_GAMEDATA "Package the GameData directory into the Android APK assets" ON)
set(RA_ANDROID_SDK_ROOT "" CACHE PATH "Android SDK root used for Android packaging helper discovery")
set(RA_ANDROID_PLATFORM_ROOT "" CACHE PATH "Android SDK platform root containing android.jar for Android packaging")
set(RA_ANDROID_MIN_SDK_VERSION "26" CACHE STRING "Minimum Android SDK level for the native Android build")
set(RA_ANDROID_TARGET_SDK_VERSION "34" CACHE STRING "Target Android SDK level for the native Android build")

set(RA_ICON_DIR "${CMAKE_SOURCE_DIR}/resources/icons")
set(RA_WINDOWS_ICON_FILE "${RA_ICON_DIR}/redalert.ico")
set(RA_WINDOW_ICON_FILE "${RA_ICON_DIR}/redalert-window-icon.png")
set(RA_ANDROID_ICON_RES_DIR "${RA_ICON_DIR}/android")
set(RA_WEB_ICON_DIR "${CMAKE_SOURCE_DIR}/web/icons")
set(RA_WEB_MANIFEST_FILE "${CMAKE_SOURCE_DIR}/web/site.webmanifest")
set(RA_WINDOWS_RESOURCE_FILE "${CMAKE_SOURCE_DIR}/resources/windows/redalert.rc")

if(EMSCRIPTEN)
    set(RA_ENABLE_UDP OFF CACHE BOOL "Enable UDP multiplayer transport" FORCE)
    set(RA_ENABLE_TCP OFF CACHE BOOL "Enable TCP multiplayer transport" FORCE)
    set(RA_ENABLE_WOL ON CACHE BOOL "Enable WebSocket (WOL replacement) multiplayer transport" FORCE)
endif()

if(ANDROID)
    set(ra_android_sdk_root "")
    if(NOT "${RA_ANDROID_SDK_ROOT}" STREQUAL "")
        set(ra_android_sdk_root "${RA_ANDROID_SDK_ROOT}")
    elseif(DEFINED SDL_ANDROID_HOME AND NOT "${SDL_ANDROID_HOME}" STREQUAL "")
        set(ra_android_sdk_root "${SDL_ANDROID_HOME}")
    elseif(DEFINED ANDROID_HOME AND NOT "${ANDROID_HOME}" STREQUAL "")
        set(ra_android_sdk_root "${ANDROID_HOME}")
    elseif(DEFINED ANDROID_SDK_ROOT AND NOT "${ANDROID_SDK_ROOT}" STREQUAL "")
        set(ra_android_sdk_root "${ANDROID_SDK_ROOT}")
    elseif(DEFINED ENV{ANDROID_HOME} AND NOT "$ENV{ANDROID_HOME}" STREQUAL "")
        set(ra_android_sdk_root "$ENV{ANDROID_HOME}")
    elseif(DEFINED ENV{ANDROID_SDK_ROOT} AND NOT "$ENV{ANDROID_SDK_ROOT}" STREQUAL "")
        set(ra_android_sdk_root "$ENV{ANDROID_SDK_ROOT}")
    endif()

    if(NOT "${ra_android_sdk_root}" STREQUAL "" AND
       (NOT DEFINED SDL_ANDROID_HOME OR NOT "${SDL_ANDROID_HOME}" STREQUAL "${ra_android_sdk_root}"))
        set(SDL_ANDROID_HOME "${ra_android_sdk_root}" CACHE PATH "Path to the Android SDK root" FORCE)
    endif()

    set(ra_android_platform_root "")
    if(NOT "${RA_ANDROID_PLATFORM_ROOT}" STREQUAL "")
        set(ra_android_platform_root "${RA_ANDROID_PLATFORM_ROOT}")
    elseif(NOT DEFINED SDL_ANDROID_PLATFORM_ROOT OR "${SDL_ANDROID_PLATFORM_ROOT}" STREQUAL "")
        set(ra_android_platform_root "")
        set(ra_android_platform_version -1)
        if(NOT "${ra_android_sdk_root}" STREQUAL "" AND IS_DIRECTORY "${ra_android_sdk_root}/platforms")
            file(GLOB ra_android_platform_candidates LIST_DIRECTORIES true "${ra_android_sdk_root}/platforms/android-*")
            foreach(ra_android_platform_candidate IN LISTS ra_android_platform_candidates)
                get_filename_component(ra_android_platform_name "${ra_android_platform_candidate}" NAME)
                if(ra_android_platform_name MATCHES "^android-([0-9]+)(\\.[0-9]+)?$" AND EXISTS "${ra_android_platform_candidate}/android.jar")
                    if(CMAKE_MATCH_1 GREATER ra_android_platform_version)
                        set(ra_android_platform_version "${CMAKE_MATCH_1}")
                        set(ra_android_platform_root "${ra_android_platform_candidate}")
                    endif()
                endif()
            endforeach()
        endif()
    else()
        set(ra_android_platform_root "${SDL_ANDROID_PLATFORM_ROOT}")
    endif()

    if(NOT "${ra_android_platform_root}" STREQUAL "")
        get_filename_component(ra_android_platform_name "${ra_android_platform_root}" NAME)
        set(ra_android_sdl_platform_root "${ra_android_platform_root}")
        if(ra_android_platform_name MATCHES "^android-([0-9]+)\\.[0-9]+$")
            set(ra_android_sdl_platform_root "${CMAKE_BINARY_DIR}/android-sdk-platform/android-${CMAKE_MATCH_1}")
            file(MAKE_DIRECTORY "${ra_android_sdl_platform_root}")
            file(COPY_FILE
                "${ra_android_platform_root}/android.jar"
                "${ra_android_sdl_platform_root}/android.jar"
                ONLY_IF_DIFFERENT
            )
        endif()
        set(SDL_ANDROID_PLATFORM_ROOT "${ra_android_sdl_platform_root}" CACHE PATH "Path of Android platform" FORCE)
    endif()
endif()

if(ANDROID)
    set(SDL_SHARED ON CACHE BOOL "" FORCE)
    set(SDL_STATIC OFF CACHE BOOL "" FORCE)
    set(SDL_ANDROID_JAR ON CACHE BOOL "" FORCE)
else()
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
endif()
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

if(RA_ENABLE_TCP)
    list(APPEND RA_COMMON_DEFINITIONS RA_ENABLE_TCP=1)
else()
    list(APPEND RA_COMMON_DEFINITIONS RA_ENABLE_TCP=0)
endif()

if(RA_ENABLE_WOL)
    list(APPEND RA_COMMON_DEFINITIONS RA_ENABLE_WOL=1)
else()
    list(APPEND RA_COMMON_DEFINITIONS RA_ENABLE_WOL=0)
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
list(FILTER RA_GAME_SOURCES EXCLUDE REGEX "/SOCKETS_(WINDOWS|LINUX)\.CPP$")
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

if(WIN32)
    list(APPEND RA_GAME_SOURCES "${RA_WINDOWS_RESOURCE_FILE}")
endif()

if(WIN32)
    list(APPEND RA_GAME_SOURCES "${CMAKE_SOURCE_DIR}/CODE/SOCKETS_WINDOWS.CPP")
else()
    list(APPEND RA_GAME_SOURCES "${CMAKE_SOURCE_DIR}/CODE/SOCKETS_LINUX.CPP")
endif()

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

if(EMSCRIPTEN OR (ANDROID AND RA_ANDROID_PACKAGE_GAMEDATA))
    file(GLOB_RECURSE RA_FILTERED_GAMEDATA_ENTRIES
        RELATIVE "${CMAKE_SOURCE_DIR}/GameData"
        "${CMAKE_SOURCE_DIR}/GameData/*"
    )
    set(RA_FILTERED_GAMEDATA_FILES "")
    foreach(ra_gamedata_entry IN LISTS RA_FILTERED_GAMEDATA_ENTRIES)
        if(IS_DIRECTORY "${CMAKE_SOURCE_DIR}/GameData/${ra_gamedata_entry}")
            continue()
        endif()
        if(ra_gamedata_entry MATCHES "^(redalert|redalert-asan)(\\.exe)?$")
            continue()
        endif()
        string(REPLACE "\\" "/" ra_gamedata_entry_normalized "${ra_gamedata_entry}")
        if(ra_gamedata_entry_normalized MATCHES "^resources/icons/")
            continue()
        endif()
        list(APPEND RA_FILTERED_GAMEDATA_FILES "${ra_gamedata_entry_normalized}")
    endforeach()
    list(SORT RA_FILTERED_GAMEDATA_FILES)
endif()

if(EMSCRIPTEN)
    set(RA_EMSCRIPTEN_EFFECTIVE_PACKAGE_GAMEDATA ${RA_EMSCRIPTEN_PACKAGE_GAMEDATA})
    if(RA_EMSCRIPTEN_LAZY_FETCH_GAMEDATA AND RA_EMSCRIPTEN_PACKAGE_GAMEDATA)
        message(STATUS "RA_EMSCRIPTEN_LAZY_FETCH_GAMEDATA is ON; skipping monolithic GameData preloading")
        set(RA_EMSCRIPTEN_EFFECTIVE_PACKAGE_GAMEDATA OFF)
    endif()

    if(RA_EMSCRIPTEN_LAZY_FETCH_GAMEDATA)
        list(JOIN RA_FILTERED_GAMEDATA_FILES "\n" RA_EMSCRIPTEN_GAMEDATA_MANIFEST_CONTENT)
        set(RA_EMSCRIPTEN_GAMEDATA_MANIFEST "${CMAKE_BINARY_DIR}/ra-assets-manifest.txt")
        if(RA_EMSCRIPTEN_GAMEDATA_MANIFEST_CONTENT)
            file(WRITE "${RA_EMSCRIPTEN_GAMEDATA_MANIFEST}" "${RA_EMSCRIPTEN_GAMEDATA_MANIFEST_CONTENT}\n")
        else()
            file(WRITE "${RA_EMSCRIPTEN_GAMEDATA_MANIFEST}" "")
        endif()
    endif()
endif()

if(ANDROID)
    set(RA_ANDROID_ASSET_STAGE_DIR "${CMAKE_BINARY_DIR}/android-assets")
    set(RA_ANDROID_STAGED_ASSETS "")
    if(RA_ANDROID_PACKAGE_GAMEDATA)
        foreach(ra_android_asset IN LISTS RA_FILTERED_GAMEDATA_FILES)
            set(ra_android_asset_source "${CMAKE_SOURCE_DIR}/GameData/${ra_android_asset}")
            set(ra_android_asset_destination "${RA_ANDROID_ASSET_STAGE_DIR}/${ra_android_asset}")
            get_filename_component(ra_android_asset_destination_directory "${ra_android_asset_destination}" DIRECTORY)
            add_custom_command(
                OUTPUT "${ra_android_asset_destination}"
                COMMAND ${CMAKE_COMMAND} -E make_directory "${ra_android_asset_destination_directory}"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ra_android_asset_source}" "${ra_android_asset_destination}"
                DEPENDS "${ra_android_asset_source}"
                VERBATIM
            )
            list(APPEND RA_ANDROID_STAGED_ASSETS "${ra_android_asset_destination}")
        endforeach()
    endif()
    add_custom_target(redalert-android-assets DEPENDS ${RA_ANDROID_STAGED_ASSETS})
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

if(ANDROID)
    add_library(redalert SHARED ${RA_GAME_SOURCES})
else()
    add_executable(redalert ${RA_GAME_SOURCES})
endif()

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
        target_compile_options(${target} PRIVATE /w /MP)
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

if(TARGET SDL3::SDL3)
    set(RA_SDL_TARGET SDL3::SDL3)
elseif(TARGET SDL3::SDL3-static)
    set(RA_SDL_TARGET SDL3::SDL3-static)
elseif(TARGET SDL3-static)
    set(RA_SDL_TARGET SDL3-static)
else()
    message(FATAL_ERROR "Bundled SDL3 target was not created")
endif()

if(ANDROID)
    if(TARGET SDL3::SDL3-shared)
        set(RA_ANDROID_SDL_SHARED_TARGET SDL3::SDL3-shared)
    elseif(TARGET SDL3-shared)
        set(RA_ANDROID_SDL_SHARED_TARGET SDL3-shared)
    else()
        message(FATAL_ERROR "Android builds require the bundled SDL3 shared target")
    endif()
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
    set(RA_EMSCRIPTEN_SHELL_FILE "${CMAKE_SOURCE_DIR}/web/shell.html")
    target_link_options(redalert PRIVATE
        "-sASYNCIFY"
        "-sASYNCIFY_IMPORTS=ra_set_web_soft_keyboard_active"
        "-sASYNCIFY_STACK_SIZE=1048576"
        "-Wl,--export=emscripten_stack_get_end"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sINITIAL_MEMORY=268435456"
        "-sSTACK_SIZE=8388608"
        "-sMAX_WEBGL_VERSION=2"
        "-sMIN_WEBGL_VERSION=1"
        "-sENVIRONMENT=web"
        "--shell-file" "${RA_EMSCRIPTEN_SHELL_FILE}"
    )
    if(RA_EMSCRIPTEN_DIAGNOSTIC)
        message(STATUS "Emscripten diagnostic build enabled (ASSERTIONS=2, SAFE_HEAP=1, symbol names)")
        target_compile_options(redalert PRIVATE
            "-gsource-map"
            "-fno-omit-frame-pointer"
        )
        target_link_options(redalert PRIVATE
            "-gsource-map"
            "--profiling-funcs"
            "-sASSERTIONS=2"
            "-sSAFE_HEAP=1"
            "-sSTACK_OVERFLOW_CHECK=2"
            "-sASYNCIFY_ADVISE=1"
        )
    endif()
    set_property(TARGET redalert APPEND PROPERTY LINK_DEPENDS "${RA_EMSCRIPTEN_SHELL_FILE}")
    if(RA_ENABLE_WOL)
        target_link_options(redalert PRIVATE "-lwebsocket.js")
    endif()
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
    add_custom_command(TARGET redalert POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:redalert>/icons"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${RA_WEB_ICON_DIR}/favicon-16.png"
            "${RA_WEB_ICON_DIR}/favicon-32.png"
            "${RA_WEB_ICON_DIR}/favicon-256.png"
            "${RA_WEB_ICON_DIR}/icon-192.png"
            "${RA_WEB_ICON_DIR}/apple-touch-icon.png"
            "$<TARGET_FILE_DIR:redalert>/icons"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${RA_WEB_MANIFEST_FILE}"
            "$<TARGET_FILE_DIR:redalert>"
        VERBATIM
    )
elseif(ANDROID)
    find_package(Java REQUIRED)
    if(NOT TARGET SDL3::Jar OR NOT TARGET SDL3-jar)
        message(FATAL_ERROR "Bundled SDL3 Android Java support was not created; check Java and Android SDK setup")
    endif()
    set(RA_ANDROID_SDL_JAR "$<TARGET_PROPERTY:SDL3-jar,OUTPUT>")

    list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/extern/SDL3/cmake/android")
    find_package(SdlAndroidPlatform MODULE REQUIRED)
    find_package(SdlAndroid MODULE REQUIRED)
    include(SdlAndroidFunctions)

    set(RA_ANDROID_PACKAGE "com.ea.redalert")
    set(RA_ANDROID_ACTIVITY "RedAlertActivity")
    set(RA_ANDROID_DATA_SPLIT_NAME "ra_data")
    set(RA_ANDROID_GENERATED_SRC_FOLDER "${CMAKE_CURRENT_BINARY_DIR}/android/redalert-src")
    set(RA_ANDROID_GENERATED_MANIFEST "${RA_ANDROID_GENERATED_SRC_FOLDER}/AndroidManifest.xml")
    set(RA_ANDROID_GENERATED_DATA_MANIFEST "${RA_ANDROID_GENERATED_SRC_FOLDER}/AndroidManifest-data.xml")
    string(REPLACE "." "/" RA_ANDROID_JAVA_PACKAGE_PATH "${RA_ANDROID_PACKAGE}")
    set(RA_ANDROID_JAVA_OUTPUT_DIR "${RA_ANDROID_GENERATED_SRC_FOLDER}/${RA_ANDROID_JAVA_PACKAGE_PATH}")
    file(MAKE_DIRECTORY "${RA_ANDROID_JAVA_OUTPUT_DIR}")

    configure_file(
        "${CMAKE_SOURCE_DIR}/android/cmake/AndroidManifest.xml.cmake"
        "${RA_ANDROID_GENERATED_MANIFEST}"
        @ONLY
    )
    configure_file(
        "${CMAKE_SOURCE_DIR}/android/cmake/AndroidDataManifest.xml.cmake"
        "${RA_ANDROID_GENERATED_DATA_MANIFEST}"
        @ONLY
    )
    configure_file(
        "${CMAKE_SOURCE_DIR}/android/cmake/RedAlertActivity.java.cmake"
        "${RA_ANDROID_JAVA_OUTPUT_DIR}/${RA_ANDROID_ACTIVITY}.java"
        @ONLY
    )

    file(GLOB_RECURSE RA_ANDROID_SDL_RESOURCE_FILES LIST_DIRECTORIES false
        "${CMAKE_SOURCE_DIR}/extern/SDL3/android-project/app/src/main/res/*"
    )
    list(FILTER RA_ANDROID_SDL_RESOURCE_FILES EXCLUDE REGEX "/mipmap-[^/]+/ic_launcher\\.png$")
    list(SORT RA_ANDROID_SDL_RESOURCE_FILES)

    sdl_android_compile_resources(redalert-android-resources
        RESFOLDER "${RA_ANDROID_ICON_RES_DIR}"
        RESOURCES
            ${RA_ANDROID_SDL_RESOURCE_FILES}
    )

    sdl_android_link_resources(redalert-apk-linked
        MANIFEST "${RA_ANDROID_GENERATED_MANIFEST}"
        PACKAGE "${RA_ANDROID_PACKAGE}"
        RES_TARGETS redalert-android-resources
        MIN_SDK_VERSION "${RA_ANDROID_MIN_SDK_VERSION}"
        TARGET_SDK_VERSION "${RA_ANDROID_TARGET_SDK_VERSION}"
    )

    set(CMAKE_JAVA_COMPILE_FLAGS "-encoding;utf-8")
    set(RA_ANDROID_CLASSES_DIR "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/redalert-java.dir/classes")
    if(NOT IS_DIRECTORY "${RA_ANDROID_CLASSES_DIR}")
        execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory "${RA_ANDROID_CLASSES_DIR}")
    endif()
    set(RA_ANDROID_OUT_JAR "${CMAKE_CURRENT_BINARY_DIR}/redalert.jar")
    add_custom_command(
        OUTPUT "${RA_ANDROID_OUT_JAR}"
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${RA_ANDROID_CLASSES_DIR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${RA_ANDROID_CLASSES_DIR}"
        COMMAND ${Java_JAVAC_EXECUTABLE}
            -source 1.8 -target 1.8
            -bootclasspath "${RA_ANDROID_SDL_JAR}"
            "${RA_ANDROID_JAVA_OUTPUT_DIR}/${RA_ANDROID_ACTIVITY}.java"
            $<TARGET_PROPERTY:redalert-apk-linked,JAVA_R>
            -cp "${RA_ANDROID_SDL_JAR}:${SDL_ANDROID_PLATFORM_ANDROID_JAR}"
            -d "${RA_ANDROID_CLASSES_DIR}"
        COMMAND ${Java_JAR_EXECUTABLE} cf "${RA_ANDROID_OUT_JAR}" -C "${RA_ANDROID_CLASSES_DIR}" .
        DEPENDS
            $<TARGET_PROPERTY:redalert-apk-linked,OUTPUTS>
            SDL3-jar
            "${RA_ANDROID_JAVA_OUTPUT_DIR}/${RA_ANDROID_ACTIVITY}.java"
        VERBATIM
    )
    add_custom_target(redalert-jar DEPENDS "${RA_ANDROID_OUT_JAR}")
    set_property(TARGET redalert-jar PROPERTY OUTPUT "${RA_ANDROID_OUT_JAR}")

    set(RA_ANDROID_DEX_WORKDIR "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/redalert-dex.dir")
    if(NOT IS_DIRECTORY "${RA_ANDROID_DEX_WORKDIR}")
        execute_process(COMMAND "${CMAKE_COMMAND}" -E make_directory "${RA_ANDROID_DEX_WORKDIR}")
    endif()
    set(RA_ANDROID_CLASSES_DEX "${RA_ANDROID_DEX_WORKDIR}/classes.dex")
    add_custom_command(
        OUTPUT "${RA_ANDROID_CLASSES_DEX}"
        COMMAND SdlAndroid::d8
            $<TARGET_PROPERTY:redalert-jar,OUTPUT>
            "${RA_ANDROID_SDL_JAR}"
            --lib "${SDL_ANDROID_PLATFORM_ANDROID_JAR}"
            --output "${RA_ANDROID_DEX_WORKDIR}"
        DEPENDS $<TARGET_PROPERTY:redalert-jar,OUTPUT> SDL3-jar
        VERBATIM
    )
    add_custom_target(redalert-dex DEPENDS "${RA_ANDROID_CLASSES_DEX}")
    set_property(TARGET redalert-dex PROPERTY OUTPUT "${RA_ANDROID_CLASSES_DEX}")

    set(RA_ANDROID_CODE_APK_WORKDIR "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/redalert-code-apk-contents.dir")
    set(RA_ANDROID_CODE_UNALIGNED_APK "${CMAKE_CURRENT_BINARY_DIR}/intermediates/redalert-code-unaligned.apk")
    file(MAKE_DIRECTORY "${RA_ANDROID_CODE_APK_WORKDIR}")
    add_custom_command(
        OUTPUT "${RA_ANDROID_CODE_UNALIGNED_APK}"
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${RA_ANDROID_CODE_APK_WORKDIR}/lib"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${RA_ANDROID_CODE_APK_WORKDIR}/lib/${ANDROID_ABI}"
        COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_PROPERTY:redalert-apk-linked,OUTPUT>" "${RA_ANDROID_CODE_UNALIGNED_APK}"
        COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_PROPERTY:redalert-dex,OUTPUT>" "${RA_ANDROID_CODE_APK_WORKDIR}/classes.dex"
        COMMAND SdlAndroid::zip -u -q -j "${RA_ANDROID_CODE_UNALIGNED_APK}" classes.dex
        COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${RA_ANDROID_SDL_SHARED_TARGET}>" "${RA_ANDROID_CODE_APK_WORKDIR}/lib/${ANDROID_ABI}/$<TARGET_FILE_NAME:${RA_ANDROID_SDL_SHARED_TARGET}>"
        COMMAND SdlAndroid::zip -u -q "${RA_ANDROID_CODE_UNALIGNED_APK}" "lib/${ANDROID_ABI}/$<TARGET_FILE_NAME:${RA_ANDROID_SDL_SHARED_TARGET}>"
        COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:redalert>" "${RA_ANDROID_CODE_APK_WORKDIR}/lib/${ANDROID_ABI}/$<TARGET_FILE_NAME:redalert>"
        COMMAND SdlAndroid::zip -u -q "${RA_ANDROID_CODE_UNALIGNED_APK}" "lib/${ANDROID_ABI}/$<TARGET_FILE_NAME:redalert>"
        DEPENDS
            redalert
            redalert-apk-linked
            redalert-dex
            ${RA_ANDROID_SDL_SHARED_TARGET}
        WORKING_DIRECTORY "${RA_ANDROID_CODE_APK_WORKDIR}"
        COMMAND_EXPAND_LISTS
        VERBATIM
    )
    add_custom_target(redalert-code-unaligned-apk DEPENDS "${RA_ANDROID_CODE_UNALIGNED_APK}")
    set_property(TARGET redalert-code-unaligned-apk PROPERTY OUTPUT "${RA_ANDROID_CODE_UNALIGNED_APK}")

    if(RA_ANDROID_PACKAGE_GAMEDATA)
        sdl_android_link_resources(redalert-data-apk-linked
            MANIFEST "${RA_ANDROID_GENERATED_DATA_MANIFEST}"
            PACKAGE "${RA_ANDROID_PACKAGE}"
            MIN_SDK_VERSION "${RA_ANDROID_MIN_SDK_VERSION}"
            TARGET_SDK_VERSION "${RA_ANDROID_TARGET_SDK_VERSION}"
        )

        set(RA_ANDROID_TOPLEVEL_MIX_ASSETS "")
        foreach(RA_ANDROID_FILTERED_GAMEDATA_FILE IN LISTS RA_FILTERED_GAMEDATA_FILES)
            if(RA_ANDROID_FILTERED_GAMEDATA_FILE MATCHES "^[^/]+\\.MIX$")
                list(APPEND RA_ANDROID_TOPLEVEL_MIX_ASSETS "${RA_ANDROID_FILTERED_GAMEDATA_FILE}")
            endif()
        endforeach()
        set(RA_ANDROID_TOPLEVEL_MIX_ARCHIVE_PATHS "")
        foreach(RA_ANDROID_TOPLEVEL_MIX_ASSET IN LISTS RA_ANDROID_TOPLEVEL_MIX_ASSETS)
            list(APPEND RA_ANDROID_TOPLEVEL_MIX_ARCHIVE_PATHS "assets/${RA_ANDROID_TOPLEVEL_MIX_ASSET}")
        endforeach()

        set(RA_ANDROID_DATA_APK_WORKDIR "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/redalert-data-apk-contents.dir")
        set(RA_ANDROID_DATA_UNALIGNED_APK "${CMAKE_CURRENT_BINARY_DIR}/intermediates/redalert-data-unaligned.apk")
        file(MAKE_DIRECTORY "${RA_ANDROID_DATA_APK_WORKDIR}")
        add_custom_command(
            OUTPUT "${RA_ANDROID_DATA_UNALIGNED_APK}"
            COMMAND ${CMAKE_COMMAND} -E remove_directory "${RA_ANDROID_DATA_APK_WORKDIR}/assets"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${RA_ANDROID_DATA_APK_WORKDIR}/assets"
            COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_PROPERTY:redalert-data-apk-linked,OUTPUT>" "${RA_ANDROID_DATA_UNALIGNED_APK}"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${RA_ANDROID_ASSET_STAGE_DIR}" "${RA_ANDROID_DATA_APK_WORKDIR}/assets"
            COMMAND SdlAndroid::zip -u -r -q "${RA_ANDROID_DATA_UNALIGNED_APK}" assets
            COMMAND SdlAndroid::zip -d -q "${RA_ANDROID_DATA_UNALIGNED_APK}" ${RA_ANDROID_TOPLEVEL_MIX_ARCHIVE_PATHS}
            COMMAND SdlAndroid::zip -0 -q "${RA_ANDROID_DATA_UNALIGNED_APK}" ${RA_ANDROID_TOPLEVEL_MIX_ARCHIVE_PATHS}
            DEPENDS
                redalert-data-apk-linked
                redalert-android-assets
            WORKING_DIRECTORY "${RA_ANDROID_DATA_APK_WORKDIR}"
            COMMAND_EXPAND_LISTS
            VERBATIM
        )
        add_custom_target(redalert-data-unaligned-apk DEPENDS "${RA_ANDROID_DATA_UNALIGNED_APK}")
        set_property(TARGET redalert-data-unaligned-apk PROPERTY OUTPUT "${RA_ANDROID_DATA_UNALIGNED_APK}")
    endif()

    sdl_create_android_debug_keystore(redalert-debug-keystore)
    sdl_apk_align(redalert-code-aligned-apk redalert-code-unaligned-apk
        OUTDIR "${CMAKE_CURRENT_BINARY_DIR}/intermediates"
        NAME "redalert-code.aligned.apk"
    )
    sdl_apk_sign(redalert-code-apk redalert-code-aligned-apk
        KEYSTORE redalert-debug-keystore
        OUTPUT "redalert-code.apk"
    )
    if(RA_ANDROID_PACKAGE_GAMEDATA)
        sdl_apk_align(redalert-data-aligned-apk redalert-data-unaligned-apk
            OUTDIR "${CMAKE_CURRENT_BINARY_DIR}/intermediates"
            NAME "redalert-data.aligned.apk"
        )
        sdl_apk_sign(redalert-data-apk redalert-data-aligned-apk
            KEYSTORE redalert-debug-keystore
            OUTPUT "redalert-data.apk"
        )
        add_custom_target(redalert-apk DEPENDS redalert-code-apk redalert-data-apk)
    else()
        add_custom_target(redalert-apk DEPENDS redalert-code-apk)
    endif()

    if(TARGET SdlAndroid::adb)
        add_custom_target(install-redalert-apk
            COMMAND "${ADB_BIN}" install-multiple -r
                "$<TARGET_PROPERTY:redalert-code-apk,OUTPUT>"
                "$<$<BOOL:${RA_ANDROID_PACKAGE_GAMEDATA}>:$<TARGET_PROPERTY:redalert-data-apk,OUTPUT>>"
            DEPENDS redalert-apk
            VERBATIM
        )
        add_custom_target(install-redalert-code-apk
            COMMAND "${ADB_BIN}" install-multiple -r -p
                "$<TARGET_PROPERTY:redalert-code-apk,OUTPUT>"
            DEPENDS redalert-code-apk
            VERBATIM
        )
        if(RA_ANDROID_PACKAGE_GAMEDATA)
            add_custom_target(install-redalert-data-apk
                COMMAND "${ADB_BIN}" install-multiple -r -p
                    "$<TARGET_PROPERTY:redalert-data-apk,OUTPUT>"
                DEPENDS redalert-data-apk
                VERBATIM
            )
        endif()
        add_custom_target(start-redalert
            COMMAND "${ADB_BIN}" shell am start-activity -S "${RA_ANDROID_PACKAGE}/.${RA_ANDROID_ACTIVITY}"
            VERBATIM
        )
        add_custom_target(build-install-start-redalert
            COMMAND "${ADB_BIN}" install-multiple -r
                "$<TARGET_PROPERTY:redalert-code-apk,OUTPUT>"
                "$<$<BOOL:${RA_ANDROID_PACKAGE_GAMEDATA}>:$<TARGET_PROPERTY:redalert-data-apk,OUTPUT>>"
            COMMAND "${ADB_BIN}" shell am start-activity -S "${RA_ANDROID_PACKAGE}/.${RA_ANDROID_ACTIVITY}"
            DEPENDS redalert-apk
            VERBATIM
        )
    endif()
else()
    add_custom_command(TARGET redalert POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:redalert>/resources/icons"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${RA_WINDOW_ICON_FILE}"
            "$<TARGET_FILE_DIR:redalert>/resources/icons"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:redalert>
            "${CMAKE_SOURCE_DIR}/GameData/${RA_RUNTIME_COPY_NAME}${CMAKE_EXECUTABLE_SUFFIX}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/GameData/resources/icons"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${RA_WINDOW_ICON_FILE}"
            "${CMAKE_SOURCE_DIR}/GameData/resources/icons"
        VERBATIM
    )
endif()
