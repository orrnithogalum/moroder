cmake_minimum_required(VERSION 3.16)

project(moroder)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(DISCORD_SDK_ROOT "${CMAKE_SOURCE_DIR}/lib/discord_social_sdk")
set(DISCORD_SDK_LIB_DIR "${DISCORD_SDK_ROOT}/lib/release")
set(DISCORD_SDK_BIN_DIR "${DISCORD_SDK_ROOT}/bin/release")
set(DISCORD_SDK_INCLUDE_DIR "${DISCORD_SDK_ROOT}/include")

set(DISCORD_LIB_PATH "${DISCORD_SDK_LIB_DIR}/libdiscord_partner_sdk.so")
set(DISCORD_SHARED_LIB "${DISCORD_SDK_LIB_DIR}/libdiscord_partner_sdk.so")

include(FetchContent)

FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.13.0
)
FetchContent_MakeAvailable(spdlog)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.12.0
)
FetchContent_MakeAvailable(nlohmann_json)

FetchContent_Declare(
    ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/ftxui.git
    GIT_TAG v6.1.9
)
FetchContent_MakeAvailable(ftxui)

FetchContent_Declare(
    ftxui-image-view
    GIT_REPOSITORY https://github.com/orrnithogalum/ftxui-image-view-async.git
    GIT_TAG main
)
FetchContent_MakeAvailable(ftxui-image-view)

FetchContent_Declare(
    sdbus-cpp
    GIT_REPOSITORY https://github.com/Kistler-Group/sdbus-cpp.git
    GIT_TAG v2.2.1
)
FetchContent_MakeAvailable(sdbus-cpp)

FetchContent_Declare(
    ftxui-grid-container
    GIT_REPOSITORY https://github.com/mingsheng13/grid-container-ftxui
    GIT_TAG master
)
FetchContent_MakeAvailable(ftxui-grid-container)

set(SOURCES
    src/services/mpris.cpp
    src/services/mpv.cpp
    src/services/music.cpp
    src/services/player.cpp
    src/services/social.cpp

    src/ui/components/carousel.cpp
    src/ui/components/grid.cpp
    src/ui/components/search_bar.cpp
    src/ui/components/sidebar.cpp

    src/main.cpp
)

# Embed default config
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/gen)

set(GENERATED_HEADER_NAME defaults)
set(DEFAULT_CONF ${CMAKE_CURRENT_SOURCE_DIR}/res/${PROJECT_NAME}.default.conf)
set(GENERATED_HEADER ${CMAKE_CURRENT_BINARY_DIR}/gen/${GENERATED_HEADER_NAME}.hpp)

add_custom_command(
    OUTPUT ${GENERATED_HEADER}

    COMMAND xxd -i -n ${PROJECT_NAME}_default ${DEFAULT_CONF} > ${GENERATED_HEADER}
    COMMAND sed -i -E "s/^(unsigned )/inline \\1/" ${GENERATED_HEADER}

    DEPENDS ${DEFAULT_CONF}
    COMMENT "Embedding default config into ${GENERATED_HEADER}"
    VERBATIM
)

add_custom_target(generate_${PROJECT_NAME}_conf_header ALL DEPENDS ${GENERATED_HEADER})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_executable(${PROJECT_NAME} ${SOURCES})
add_dependencies(${PROJECT_NAME} generate_${PROJECT_NAME}_conf_header)

target_include_directories(${PROJECT_NAME} PRIVATE ${DISCORD_SDK_INCLUDE_DIR})

target_link_libraries(${PROJECT_NAME}
    PRIVATE

    spdlog::spdlog
    nlohmann_json::nlohmann_json
    sdbus-c++

    ftxui::screen
    ftxui::dom
    ftxui::component
    ftxui-image-view
    ftxui-grid-container

    mpv

    ${DISCORD_LIB_PATH}
)

set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_INSTALL_RPATH "$ORIGIN")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(${PROJECT_NAME}
        PRIVATE
        MORODER_PYTHON_PATH="${CMAKE_SOURCE_DIR}/python"
        MORODER_ASSET_PATH="${CMAKE_SOURCE_DIR}/assets"
        MORODER_LOG_PATH="${CMAKE_SOURCE_DIR}/logs"

        MORODER_CACHE_PATH=".cache/moroder"
    )
else()
    target_compile_definitions(${PROJECT_NAME}
        PRIVATE
        MORODER_PYTHON_PATH=".local/share/moroder/python"
        MORODER_ASSET_PATH="$.local/share/moroder/assets"
        MORODER_LOG_PATH=".local/state/moroder/logs"

        MORODER_CACHE_PATH=".cache/moroder"
    )
endif()

target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Release>:-O3>
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${DISCORD_SHARED_LIB}"
    $<TARGET_FILE_DIR:${PROJECT_NAME}>
)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
