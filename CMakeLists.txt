cmake_minimum_required (VERSION 3.11)
project(simple_parallel_analyzer)

add_definitions( -DLOGIC2 )

# enable generation of compile_commands.json, helpful for IDEs to locate include files.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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

add_analyzer_plugin(simple_parallel_analyzer SOURCES ${SOURCES})
