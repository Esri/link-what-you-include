// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <src/cluster_diff.hpp>

#include <target_model/target.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <numeric>
#include <set>
#include <vector>

namespace tidy
{
namespace
{
size_t intersection_size(const std::set<target_model::Target>* lhs,
                         const std::set<target_model::Target>* rhs)
{
  if (lhs && !rhs)
  {
    return lhs->size();
  }
  if (!lhs && rhs)
  {
    return rhs->size();
  }
  if (!lhs && !rhs)
  {
    return 0;
  }

  auto first1 = lhs->begin();
  auto first2 = rhs->begin();
  auto last1 = lhs->end();
  auto last2 = rhs->end();
  size_t count = 0;
  while (first1 != last1 && first2 != last2)
  {
    if (*first1 < *first2)
    {
      ++first1;
    }
    else
    {
      if (!(*first2 < *first1))
      {
        first1++;
        ++count;
      }
      ++first2;
    }
  }
  return count;
}

Cluster_diff single_diff(const std::set<target_model::Target>* lhs,
                         const std::set<target_model::Target>* rhs)
{
  Cluster_diff diff;

  if (lhs && !rhs)
  {
    diff.left_only.insert(diff.left_only.end(), lhs->begin(), lhs->end());
  }
  else if (!lhs && rhs)
  {
    diff.right_only.insert(diff.right_only.end(), rhs->begin(), rhs->end());
  }
  else if (lhs && rhs)
  {
    std::ranges::set_difference(*lhs, *rhs, std::back_inserter(diff.left_only));
    std::ranges::set_difference(*rhs, *lhs, std::back_inserter(diff.right_only));
  }

  return diff;
}
} // namespace

std::vector<Cluster_diff> cluster_diff(const std::vector<std::set<target_model::Target>>& lhs,
                                       const std::vector<std::set<target_model::Target>>& rhs)
{
  const size_t result_size = std::max(lhs.size(), rhs.size());

  auto view_lhs = [&](size_t i) -> const std::set<target_model::Target>*
  {
    if (i < lhs.size())
    {
      return &lhs[i];
    }
    return nullptr;
  };
  auto view_rhs = [&](size_t i) -> const std::set<target_model::Target>*
  {
    if (i < rhs.size())
    {
      return &rhs[i];
    }
    return nullptr;
  };

  std::vector<int16_t> intersection_matrix(result_size * result_size);
  for (size_t i = 0; i < result_size; ++i)
  {
    for (size_t j = 0; j < result_size; ++j)
    {
      const size_t size = intersection_size(view_lhs(i), view_rhs(j));
      intersection_matrix[i * result_size + j] = static_cast<int16_t>(
        std::min(size, static_cast<size_t>(std::numeric_limits<int16_t>::max())));
    }
  }

  std::vector<size_t> permutation(result_size);
  std::ranges::iota(permutation, 0);

  int32_t max_score = -1;
  std::vector<size_t> max_permutation;
  do // NOLINT(cppcoreguidelines-avoid-do-while)
  {
    int32_t score = 0;
    for (size_t i = 0; i < permutation.size(); ++i)
    {
      const size_t j = permutation[i];
      score += intersection_matrix[i * result_size + j];
    }
    if (max_score < score)
    {
      max_score = score;
      max_permutation = permutation;
    }
  }
  while (std::ranges::next_permutation(permutation).found);

  std::vector<Cluster_diff> result(result_size);
  for (size_t i = 0; i < result_size; ++i)
  {
    result[i] = single_diff(view_lhs(i), view_rhs(max_permutation[i]));
  }

  return result;
}
} // namespace tidy
