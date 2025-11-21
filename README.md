<!--
Copyright (c) 2025 Environmental Systems Research Institute, Inc.
SPDX-License-Identifier: Apache-2.0
-->

# link-what-you-include

> Maintain a coherent build system target model.

This is a tool that can be used to check that a C++ build system has correctly
specified the dependencies between targets based on what files the source code
includes. It is analogous to
[Include What You Use (IWYU)](https://github.com/include-what-you-use/include-what-you-use)
but targeting inter-library dependencies rather than inter-file dependencies.

A core organizing principle of modern build systems is the target model, which
is a graph of interconnected targets representing libraries, executables, and
other artifacts. Each target specifies the requirements needed to build itself
and also the requirements needed to be used by another target. When performing
a build, the usage requirements of targets are propagated down the graph to
dependent targets. This target model is a concise and powerful way to specify
the relationships between the components but it can be difficult to maintain
it's accuracy over the lifetime of a project. For example it is easy to make a
change to the source that removes the last remaining dependency on a library
but neglects to remove that dependency from the target model. The compile
commands will then have unnecessary include directories, and the link command
will list an unnecessary library but the build will succeed. Similarly, it is
easy to start using an external library that one of your existing dependencies
uses in it's interface and neglect to add that library to the target model.
Since the include directories and libraries are already present in the build
commands because of the transitive dependency, again the build will still
succeed. Over time, mistakes like this accumulate and the effectiveness of the
target model is diminished.

Both of the common mistakes mentioned above can be caught by
link-what-you-include. The tool works by reading a JSON file containing target
information acquired from the build system, scanning the source code for each
target, mapping the set of included headers to the set of dependent libraries,
and comparing these dependencies to what is reported in the JSON file.
Currently, the JSON file is in a bespoke format and can only be generated for a
compatible cmake based buildsytem with the provided cmake module. The eventual
goal is to use [CPS](https://github.com/cps-org/cps) files instead.

### Status

This tool is under development and may not be ready for production use. It is
being used internally against itself and in one reasonably large project at
Esri.

### How to build

The following dependencies must be found by cmake

- [catch2](https://github.com/catchorg/Catch2). Only needed if BUILD_TESTING is
  enabled.
- [fmt](https://github.com/fmtlib/fmt). To be removed by a future update to
  C++23.
- [libclang](https://github.com/llvm/llvm-project/releases/tag/llvmorg-18.1.8).
  Must be version 18.
- [simdjson](https://github.com/simdjson/simdjson).
- [tl-expected](https://github.com/TartanLlama/expected). To be removed by a future update to C++23.

[Conan](https://conan.io/) can be used to get all the dependencies except for
`libclang`. It should be possible to manually install the dependencies or use
system packages and have cmake find them in the usual way but this workflow is
not tested. For Ubuntu, `libclang` can be installed with

```
$ sudo apt install libclang-18-dev
$ export Clang_ROOT=/usr/lib/llvm-18
```

Then run conan to install the rest of the dependencies and build as usual.

```
$ conan install . -s build_type=Debug --build=missing -of build
$ cmake -GNinja -S. -Bbuild \
    -DCMAKE_TOOLCHAIN_FILE=./build/conan_build/generators/conan_toolchain.cmake \
    -DCMAKE_BUILD_TYPE=Debug
$ cmake --build build
```

### How to use with a cmake based build system

The tool works best if targets define
[`INTERFACE_HEADER_SETS`](https://cmake.org/cmake/help/latest/prop_tgt/INTERFACE_HEADER_SETS.html)
and
[`VERIFY_INTERFACE_HEADER_SETS`](https://cmake.org/cmake/help/latest/prop_tgt/VERIFY_INTERFACE_HEADER_SETS.html#prop_tgt:VERIFY_INTERFACE_HEADER_SETS)
is used. The former makes it easy to associate an included header to its
corresponding target and the latter makes it easy to scan interface headers with
the correct preprocessor flags. If a target does not define the
`INTERFACE_HEADER_SETS` property, included headers will still be associated if
the header is located in one of its
[`INTERFACE_INCLUDE_DIRECTORIES`](https://cmake.org/cmake/help/latest/prop_tgt/INTERFACE_INCLUDE_DIRECTORIES.html).
Since multiple targets could use the same include directory, one or more
include prefix strings can be provided to disambiguate.

Update your cmake logic to include
[link_what_you_include.cmake](cmake/link_what_you_include.cmake) and call
`link_what_you_include(target ...)` for every target you want to participate in
the verification process. Configure the build system with a single-config
generator and set the CMAKE_EXPORT_COMPILE_COMMANDS cache variable. Then run
the lwyi executable and point it at the configured build directory.

```
$ lwyi -d /path/to/the/build/dir
```

### Contributing

Esri welcomes contributions from anyone and everyone. Please see our
[guidelines for contributing](https://github.com/esri/contributing).

### License

Copyright 2025 Esri

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

> http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

A copy of the license is available in the repository's
[LICENSE.txt](./LICENSE.txt) file.
