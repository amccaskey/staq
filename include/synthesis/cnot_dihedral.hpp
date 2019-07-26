/*-------------------------------------------------------------------------------------------------
| This file is distributed under the MIT License.
| See accompanying file /LICENSE for details.
| Author(s): Matthew Amy
*------------------------------------------------------------------------------------------------*/

#pragma once

#include "mapping/device.hpp"
#include "synthesis/linear_reversible.hpp"

#include <tweedledum/utils/angle.hpp>

#include <vector>
#include <list>
#include <variant>

namespace synthewareQ {
namespace synthesis {

  namespace td = tweedledum;
  using phase_term = std::pair<std::vector<bool>, td::angle>;
  using cx_dihedral = std::variant<std::pair<size_t, size_t>, std::pair<td::angle, size_t> >;

  struct partition {
    std::optional<size_t> target;
    std::set<size_t>      remaining_indices;
    std::list<phase_term> terms;
  };

  void adjust_vectors(size_t ctrl, size_t tgt, std::list<partition>& stack) {
    for (auto& part : stack) {
      for (auto& [vec, angle] : part.terms) {
        vec[ctrl] = vec[ctrl] ^ vec[tgt];
      }
    }
  }

  size_t find_best_split(const std::list<phase_term>& terms, const std::set<size_t>& indices) {
    size_t max = -1;
    size_t max_i = -1;
    for (auto i : indices) {
      auto num_zeros = 0;
      auto num_ones = 0;

      for (auto& [vec, angle] : terms) {
        if (vec[i]) num_ones++;
        else num_zeros++;
      }

      if (max_i == -1 || num_zeros > max || num_ones > max) {
        max = num_zeros > num_ones ? num_zeros : num_ones;
        max_i = i;
      }
    }

    return max_i;
  }

  std::pair<std::list<phase_term>, std::list<phase_term> > split(std::list<phase_term>& terms, size_t i) {
    std::list<phase_term> zeros;
    std::list<phase_term> ones;

    while(!terms.empty()) {
      if (terms.front().first[i]) ones.splice(ones.end(), terms, terms.begin());
      else zeros.splice(zeros.end(), terms, terms.begin());
    }

    return std::make_pair(zeros, ones);
  }

  std::list<cx_dihedral> gray_synth(const std::list<phase_term>& f, linear_op<bool> A) {
    // Initialize
    std::list<cx_dihedral> ret;
    std::list<partition> stack;

    std::set<size_t> indices;
    for (auto i = 0; i < A.size(); i++) indices.insert(i);

    stack.push_front({std::nullopt, indices, f});

    while (!stack.empty()) {
      auto part = stack.front();
      stack.pop_front();

      switch (part.terms.size()) {
      case 0: continue;
      case 1: {
        auto& [vec, angle] = part.terms.front();

        if (part.target) {
          auto tgt = *(part.target);
          for (auto ctrl = 0; ctrl < vec.size(); ctrl++) {
            if (ctrl != tgt && vec[ctrl]) {
              ret.push_back(std::make_pair((size_t)ctrl, (size_t)tgt));

              // Adjust remaining vectors & output function
              adjust_vectors(ctrl, tgt, stack);
              for (auto i = 0; i < A.size(); i++) {
                A[i][ctrl] = A[i][ctrl] ^ A[i][tgt];
              }
            }
          }

          ret.push_back(std::make_pair(angle, tgt));
        } // Else can only be the all-zero vector

        break;
      }
      default: {
        // Divide into the zeros and ones of some row
        auto i = find_best_split(part.terms, part.remaining_indices);
        auto [zeros, ones] = split(part.terms, i);

        // Remove i from the remaining indices
        part.remaining_indices.erase(i);

        // Add the new partitions on the stack
        if (part.target) {
          stack.push_front({part.target, part.remaining_indices, ones});
        } else {
          stack.push_front({i, part.remaining_indices, ones});
        }
        stack.push_front({part.target, part.remaining_indices, zeros});
        break;
      }
      }

    }

    // Synthesize the overall linear transformation


    return ret;
  }


}
}