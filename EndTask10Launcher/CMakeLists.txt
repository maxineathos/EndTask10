# EndTask10Launcher - DLL Injector executable

add_executable(EndTask10Launcher
    main.cpp
)

target_link_libraries(EndTask10Launcher
    kernel32.lib
    ntdll.lib
)

# Set subsystem to Windows (console for now, can change to WINDOWS_GUI later)
set_target_properties(EndTask10Launcher PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
)
