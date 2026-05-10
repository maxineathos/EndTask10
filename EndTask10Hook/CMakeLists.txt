add_library(EndTask10Hook SHARED dllmain.cpp logging.cpp logging.h)
target_link_libraries(EndTask10Hook
    kernel32.lib user32.lib shell32.lib shlwapi.lib ole32.lib oleacc.lib UIAutomationCore.lib)
set_target_properties(EndTask10Hook PROPERTIES PREFIX "" SUFFIX ".dll")
