cmake_minimum_required (VERSION 3.13)

project(MELIBUAnalyzer)

add_definitions( -DLOGIC2 )

# enable generation of compile_commands.json, helpful for IDEs to locate include files.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# custom CMake Modules are located in the cmake directory.
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

include(ExternalAnalyzerSDK)

set(SOURCES 
src/MELIBUAnalyzer.cpp
src/MELIBUAnalyzer.h
src/MELIBUAnalyzerResults.cpp
src/MELIBUAnalyzerResults.h
src/MELIBUAnalyzerSettings.cpp
src/MELIBUAnalyzerSettings.h
src/MELIBUSimulationDataGenerator.cpp
src/MELIBUSimulationDataGenerator.h
src/MELIBUCrc.h
src/MELIBUCrc.cpp
)

add_analyzer_plugin(${PROJECT_NAME} SOURCES ${SOURCES})
