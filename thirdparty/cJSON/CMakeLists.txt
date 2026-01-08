include(FetchContent)
FetchContent_Declare(
    cJSON
    GIT_REPOSITORY https://github.com/DaveGamble/cJSON.git
    GIT_TAG v1.7.19
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(cJSON)

# Provide an interface target exposing cJSON headers for consumers.
FetchContent_GetProperties(cJSON)
if(cJSON_SOURCE_DIR)
    add_library(cjson_headers INTERFACE)
    target_include_directories(cjson_headers INTERFACE "${cJSON_SOURCE_DIR}")
endif()
