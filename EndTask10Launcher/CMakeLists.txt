add_executable(EndTask10Launcher main.cpp)
target_link_libraries(EndTask10Launcher kernel32.lib ntdll.lib)
set_target_properties(EndTask10Launcher PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
