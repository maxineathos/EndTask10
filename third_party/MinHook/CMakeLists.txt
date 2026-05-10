add_library(minhook STATIC
    "src/buffer.c"
    "src/hook.c"
    "src/trampoline.c"
    "src/hde/hde64.c"
)

target_include_directories(minhook PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_include_directories(minhook PRIVATE
    "src"
    "src/hde"
)

set_target_properties(minhook PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)
