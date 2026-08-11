# Copyright (c) 2025 Environmental Systems Research Institute, Inc.
# SPDX-License-Identifier: Apache-2.0

set -ex

root_dir="$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )"/.. &> /dev/null && pwd )"
work_dir="$root_dir/build/llvm"
install_dir="$root_dir/3rdparty"

mkdir -p "$work_dir"
cd "$work_dir"

if [ ! -d "$work_dir/src" ]; then
  version="18.1.8"
  curl -LO "https://github.com/llvm/llvm-project/releases/download/llvmorg-${version}/clang-${version}.src.tar.xz"
  curl -LO "https://github.com/llvm/llvm-project/releases/download/llvmorg-${version}/cmake-${version}.src.tar.xz"
  curl -LO "https://github.com/llvm/llvm-project/releases/download/llvmorg-${version}/llvm-${version}.src.tar.xz"
  export MSYS=winsymlinks:nativestrict # needed for windows
  tar -xf "clang-${version}.src.tar.xz"
  tar -xf "cmake-${version}.src.tar.xz"
  tar -xf "llvm-${version}.src.tar.xz"
  mkdir src
  mv "clang-${version}.src" "$work_dir/src/clang"
  mv "cmake-${version}.src" "$work_dir/src/cmake"
  mv "llvm-${version}.src" "$work_dir/src/llvm"
  patch -d $work_dir/src -p1 < $root_dir/tools/llvm-18.1.8-gcc-15.patch
fi

cmake --fresh \
      -G Ninja \
      -S "$work_dir/src/llvm" -B "$work_dir/build" \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=OFF \
      -DCMAKE_INSTALL_PREFIX="$install_dir" \
      -DLLVM_ENABLE_PROJECTS=clang \
      -DLLVM_INCLUDE_TESTS=OFF \
      -DLLVM_INCLUDE_BENCHMARKS=OFF
cmake --build "$work_dir/build"
cmake --install "$work_dir/build"

# TODO: figure out how to only build and install the libraries
#cmake --build "$work_dir/build" -t \
#  install-clang-headers \
#  install-clang-libraries \
#  install-clang-cmake-exports
