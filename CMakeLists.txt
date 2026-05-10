cmake_minimum_required(VERSION 3.20)
project(EndTask10 C CXX)

# Set C and C++ standards
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build for x64 only
if(NOT MSVC_PLATFORM_TOOLSET)
    set(CMAKE_GENERATOR_PLATFORM x64)
endif()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc")

# Set Windows API version (Windows 10 minimum)
add_compile_definitions(_WIN32_WINNT=0x0A00)
add_compile_definitions(WINVER=0x0A00)
add_compile_definitions(UNICODE _UNICODE)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# Add subdirectories
add_subdirectory(third_party/MinHook)
add_subdirectory(EndTask10Launcher)
add_subdirectory(EndTask10Hook)
add_subdirectory(EndTask10Helper)
