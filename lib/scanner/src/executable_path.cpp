// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <src/executable_path.hpp>

#include <array>
#include <cstddef>
#include <filesystem>

#ifdef WIN32
#include <Windows.h>
#elif __APPLE__
#include <mach-o/dyld.h>
#else
#include <sys/types.h>
#include <unistd.h>
#endif

namespace scanner
{
auto executable_path() -> std::filesystem::path
{
#ifdef WIN32
  constexpr DWORD max_path_length = 4096;
  std::array<char, max_path_length> exe_path{};
  const DWORD bytes = GetModuleFileNameA(nullptr, exe_path.data(), max_path_length);
  if ((bytes == 0) || (bytes == max_path_length))
  {
    return {};
  }
  return {exe_path.data()};
#elif __APPLE__
  constexpr uint32_t max_path_length = 4096;
  std::array<char, max_path_length> exe_path{};
  uint32_t path_length = max_path_length;
  const ssize_t bytes = _NSGetExecutablePath(exe_path.data(), &path_length);
  if (bytes < 0)
  {
    return {};
  }
  return {exe_path.data()};
#else
  constexpr size_t max_path_length = 4096;
  std::array<char, max_path_length> exe_path{};
  const ssize_t bytes = readlink("/proc/self/exe", exe_path.data(), max_path_length);
  if (bytes < 0 || bytes == max_path_length)
  {
    return {};
  }
  return {exe_path.data()};
#endif
}
} // namespace scanner

