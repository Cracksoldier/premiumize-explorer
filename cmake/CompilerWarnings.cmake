add_library(pmex_warnings INTERFACE)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(pmex_warnings INTERFACE
        -Wall -Wextra -Wpedantic
        -Wno-unused-parameter
    )
    if(PMEX_ENABLE_ASAN)
        target_compile_options(pmex_warnings INTERFACE -fsanitize=address,undefined)
        target_link_options(pmex_warnings INTERFACE    -fsanitize=address,undefined)
    endif()
endif()
