include(FetchContent)

FetchContent_Declare(qlementine
    GIT_REPOSITORY https://github.com/oclero/qlementine.git
    GIT_TAG        v1.4.2
    GIT_SHALLOW    TRUE
)
set(QLEMENTINE_SANDBOX  OFF CACHE BOOL "" FORCE)
set(QLEMENTINE_SHOWCASE OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(qlementine)
