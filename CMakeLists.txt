cmake_minimum_required (VERSION 3.11)
project(simple_parallel_analyzer)

# custom CMake Modules are located in the cmake directory.
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

include(ExternalAnalyzerSDK)

set(SOURCES 
src/SimpleParallelAnalyzer.cpp
src/SimpleParallelAnalyzer.h
src/SimpleParallelAnalyzerResults.cpp
src/SimpleParallelAnalyzerResults.h
src/SimpleParallelAnalyzerSettings.cpp
src/SimpleParallelAnalyzerSettings.h
src/SimpleParallelSimulationDataGenerator.cpp
src/SimpleParallelSimulationDataGenerator.h
)

add_library(simple_parallel_analyzer SHARED ${SOURCES})
target_link_libraries(simple_parallel_analyzer PRIVATE Saleae::AnalyzerSDK)

if(MSVC)
    install(TARGETS simple_parallel_analyzer RUNTIME DESTINATION "Analyzers")
else()
    install(TARGETS simple_parallel_analyzer LIBRARY DESTINATION "Analyzers")
endif()

set_post_build_destination(simple_parallel_analyzer)
