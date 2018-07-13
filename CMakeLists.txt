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

add_analyzer_plugin(simple_parallel_analyzer SOURCES ${SOURCES})
