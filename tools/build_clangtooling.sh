# Copyright (c) 2025 Environmental Systems Research Institute, Inc.
# SPDX-License-Identifier: Apache-2.0

# This script downloads, builds, and installs clang in the build/llvm directory

set -ex

root_dir="$PWD/build/llvm"
src_dir="$root_dir/src"
build_dir="$root_dir/build"

if [ -d "$root_dir" ]; then
  echo "Error: this build script expects $root_dir to not exist"
  exit 1
fi
mkdir -p "$root_dir"

mkdir "$src_dir"
cd "$src_dir"
version="18.1.8"
curl -LO "https://github.com/llvm/llvm-project/releases/download/llvmorg-${version}/clang-${version}.src.tar.xz"
curl -LO "https://github.com/llvm/llvm-project/releases/download/llvmorg-${version}/cmake-${version}.src.tar.xz"
curl -LO "https://github.com/llvm/llvm-project/releases/download/llvmorg-${version}/llvm-${version}.src.tar.xz"
export MSYS=winsymlinks:nativestrict # needed for windows
tar -xf "clang-${version}.src.tar.xz"
tar -xf "cmake-${version}.src.tar.xz"
tar -xf "llvm-${version}.src.tar.xz"
mv "clang-${version}.src" "$src_dir/clang"
mv "cmake-${version}.src" "$src_dir/cmake"
mv "llvm-${version}.src" "$src_dir/llvm"

cd "$root_dir"
cmake -G Ninja \
      -S "$src_dir/llvm" -B "$build_dir" \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX="$root_dir" \
      -DLLVM_ENABLE_PROJECTS=clang \
      -DLLVM_INCLUDE_TESTS=OFF \
      -DLLVM_INCLUDE_BENCHMARKS=OFF
cmake --build "$build_dir"
cmake --install "$build_dir"

# TODO: figure out how to only build and install the libraries
#cmake --build "$build_dir" -t \
#  install-clang-headers \
#  install-clang-libraries \
#  install-clang-cmake-exports
