# EndTask10Hook - DLL injected into explorer.exe

add_library(EndTask10Hook SHARED
    dllmain.cpp
    explorer_hooks.cpp
    explorer_hooks.h
    logging.cpp
    logging.h
)

target_link_libraries(EndTask10Hook
    minhook
    kernel32.lib
    user32.lib
    shell32.lib
    shlwapi.lib
    comctl32.lib
    ole32.lib
    oleaut32.lib
    oleacc.lib
    UIAutomationCore.lib
)

# Set DLL properties
set_target_properties(EndTask10Hook PROPERTIES
    PREFIX ""
    SUFFIX ".dll"
)
