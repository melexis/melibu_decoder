# Saleae MeLiBu decoder



## Getting started

The following documentation describes how to build this analyzer locally. For more detailed information about the Analyzer SDK, debugging, CI builds, and more, check out the readme in the Sample Analyzer repository.

https://github.com/saleae/SampleAnalyzer

## Prerequisites

### Windows

Dependencies:

- Visual Studio 2017 (or newer) with C++
- CMake 3.13+
- git

**Visual Studio 2017**

_Note - newer versions of Visual Studio should be fine._

Setup options:

- Programming Languages > Visual C++ > select all sub-components.

Note - if CMake has any problems with the MSVC compiler, it's likely a component is missing.

**CMake**

Download and install the latest CMake release here.
https://cmake.org/download/

**git**

Git is required for CMake to automatically download the AnalyzerSDK, which is a dependency of this project. Git can be downloaded here: https://git-scm.com/

### MacOS

Dependencies:

- XCode with command line tools
- CMake 3.13+
- git

Installing command line tools after XCode is installed:

```
xcode-select --install
```

Then open XCode, open Preferences from the main menu, go to locations, and select the only option under 'Command line tools'.

Installing CMake on MacOS:

1. Download the binary distribution for MacOS, `cmake-*-Darwin-x86_64.dmg`
2. Install the usual way by dragging into applications.
3. Open a terminal and run the following:

```
/Applications/CMake.app/Contents/bin/cmake-gui --install
```

_Note: Errors may occur if older versions of CMake are installed._

### Linux

Dependencies:

- CMake 3.13+
- gcc 5+
- git

Misc dependencies:

```
sudo apt-get install build-essential
```

## Building your Analyzer

### Windows

```bat
mkdir build
cd build
cmake .. -A x64
cmake --build .
:: built analyzer will be located at MeLiBu_low_level\build\Analyzers\Debug\MELIBUAnalyzer.dll
```

### MacOS

```bash
mkdir build
cd build
cmake ..
cmake --build .
# built analyzer will be located at MeLiBu_low_level/build/Analyzers/libMELIBUAnalyzer.so
```

### Linux

```bash
mkdir build
cd build
cmake ..
cmake --build .
# built analyzer will be located at MeLiBu_low_level/build/Analyzers/libMELIBUAnalyzer.so
```

## Importing analyzer

To import this analyzer in the Logic app, go to Edit->Settings and in Preferences, Custom Low Level Analyzers browse folder where the mentioned dll/so file is.

For more information on how to use MeLiBu low level analyzer with Logic app check Saleae MeLiBu plugin user manual.docx.