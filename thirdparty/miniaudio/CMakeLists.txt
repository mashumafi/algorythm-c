include(FetchContent)
FetchContent_Declare(
    miniaudio
    GIT_REPOSITORY https://github.com/mackron/miniaudio.git
    GIT_TAG 0.11.23
    GIT_SHALLOW TRUE
)

FetchContent_GetProperties(miniaudio)
if(NOT miniaudio_POPULATED)
    FetchContent_Populate(miniaudio)
endif()

add_library(miniaudio INTERFACE)
target_include_directories(miniaudio INTERFACE "${miniaudio_SOURCE_DIR}")
