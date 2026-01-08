# fetch simpleble

include(FetchContent)
FetchContent_Declare(
    simpleble
    GIT_REPOSITORY https://github.com/simpleble/simpleble.git
    GIT_TAG v0.6.1
    GIT_SHALLOW TRUE
)

FetchContent_GetProperties(simpleble)
if(NOT simpleble_POPULATED)
    FetchContent_Populate(simpleble)
endif()

if(EXISTS "${simpleble_SOURCE_DIR}/simpleble/CMakeLists.txt")
    add_subdirectory("${simpleble_SOURCE_DIR}/simpleble" "${simpleble_BINARY_DIR}/simpleble")
elseif(EXISTS "${simpleble_SOURCE_DIR}/CMakeLists.txt")
    add_subdirectory("${simpleble_SOURCE_DIR}" "${simpleble_BINARY_DIR}")
else()
    message(FATAL_ERROR "simpleble repo missing CMakeLists in root or simpleble/ subdir")
endif()
