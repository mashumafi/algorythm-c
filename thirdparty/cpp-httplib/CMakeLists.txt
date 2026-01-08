include(FetchContent)
FetchContent_Declare(
    cpp-httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.30.0
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(cpp-httplib)
