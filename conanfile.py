# Copyright (c) 2025 Environmental Systems Research Institute, Inc.
# SPDX-License-Identifier: Apache-2.0

from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout, CMakeDeps


class lwyiRecipe(ConanFile):
    name = "lwyi"
    version = "0.1"
    package_type = "application"

    # Binary configuration
    settings = "os", "compiler", "build_type", "arch"

    def requirements(self):
        self.requires("catch2/3.15.1")
        self.requires("fmt/11.1.1")
        self.requires("tl-expected/20190710")
        self.requires("simdjson/3.12.3")

    def layout(self):
        cmake_layout(self, generator="Ninja Multi-Config", build_folder="conan_build")

    def generate(self):
        tc = CMakeToolchain(self, generator="Ninja Multi-Config")
        tc.user_presets_path = False
        tc.generate()
        deps = CMakeDeps(self)
        deps.generate()

