# Copyright (c) 2025 Environmental Systems Research Institute, Inc.
# SPDX-License-Identifier: Apache-2.0

set -ex

root_dir="$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )"/.. &> /dev/null && pwd )"
work_dir="$root_dir/build/simdjson"
install_dir="$root_dir/3rdparty"

mkdir -p "$work_dir"
cd "$work_dir"

if [ ! -d "$work_dir/src" ]; then
  version="5.0.5"
  curl -LO "https://github.com/simdjson/simdjson/archive/refs/tags/v${version}.tar.gz"
  tar -xf "v${version}.tar.gz"
  mv "simdjson-${version}" "$work_dir/src"
fi

cmake --fresh \
      -G Ninja \
      -S "$work_dir/src" -B "$work_dir/build" \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX="$install_dir"
cmake --build "$work_dir/build"
cmake --install "$work_dir/build"
