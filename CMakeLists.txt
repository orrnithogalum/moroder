cmake_minimum_required(VERSION 3.16)

project(ytmusic)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.13.0
)

FetchContent_MakeAvailable(spdlog)

set(SOURCES
    src/services/yt_music.cpp

    src/main.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME}
    PRIVATE
    spdlog::spdlog
)

target_compile_definitions(${PROJECT_NAME}
    PRIVATE YTM_DEV_PYTHON_PATH="${CMAKE_SOURCE_DIR}/python/main.py"
)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)

install(DIRECTORY python/ DESTINATION share/ytmusic/python)