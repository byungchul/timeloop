/* Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "mapping/nest.hpp"

namespace analysis
{

class NestAnalysis
{
 private:
  // Cached copy of loop nest under evaluation (used for speedup).
  loop::Nest cached_nest;
  
  // Properties of the nest being analyzed (copied over during construction).
  std::vector<uint64_t> storage_tiling_boundaries_;

  // Live state.
  std::vector<analysis::LoopState> nest_state_;
  std::vector<int> indices_;
  std::uint64_t num_epochs_;
  
  // Identifies the spatial element
  // whose working set is currently being computed.
  // Dynamically updated by recursive calls.
  std::uint64_t spatial_id_;
  
  tiling::CompoundTileNest working_sets_;
  tiling::BodyInfo body_info_;

  // Memoization structures to accelerate IndexToProblemPoint()
  std::vector<problem::PerProblemDimension<std::uint64_t>>
      per_level_dim_scales_;  // level * dim
  problem::ProblemPoint cur_transform_;
  std::vector<problem::ProblemPoint> mold_low_;
  std::vector<problem::ProblemPoint> mold_high_;

  // per-level properties.
  std::vector<uint64_t> num_spatial_elems_;
  std::vector<uint64_t> spatial_fanouts_;

  // used to accelerate to IndexToProblemPoint computation
  // relevant only for master spatial levels.
  std::vector<uint64_t> horizontal_sizes_;
  std::vector<uint64_t> vertical_sizes_;

  // records if a level corresponds to the starting
  // point of a new storage tile.
  std::vector<bool> storage_boundary_level_;
  
  // any level which is at the transition point from temporal to
  // spatial nests is a master spatial level.
  // there should be one such level between each set of
  // consecutive physical storage levels.
  std::vector<bool> master_spatial_level_;
  
  // true if the spatial elements at a given master spatial
  // level are connected by on-chip links.
  std::vector<bool> linked_spatial_level_;

  bool working_sets_computed_ = false;

  problem::WorkloadConfig* workload_config_ = nullptr;

  // Internal helper methods.
  void ComputeWorkingSets();

  void InitializeNestProperties();
  void InitNumSpatialElems();
  void InitStorageBoundaries();
  void InitSpatialFanouts();
  void InitPerLevelDimScales();

  void InitializeLiveState();
  void CollectWorkingSets();

  problem::ProblemPoint IndexToProblemPoint_(const std::vector<int>& indices) const;
  
  problem::OperationSpace ComputeWorkingSetsRecursive_(
    std::vector<analysis::LoopState>::reverse_iterator cur, bool skip_delta = false);

  void ComputeTemporalWorkingSet(std::vector<analysis::LoopState>::reverse_iterator cur,
                                 problem::OperationSpace& point_set,
                                 analysis::ElementState& cur_state);
  void ComputeSpatialWorkingSet(std::vector<analysis::LoopState>::reverse_iterator cur,
                                problem::OperationSpace& point_set);

  void FillSpatialDeltas(std::vector<analysis::LoopState>::reverse_iterator cur,
                         problem::OperationSpace& point_set,
                         std::vector<problem::OperationSpace>& spatial_deltas,
                         std::vector<bool>& valid_delta,
                         std::uint64_t base_index,
                         int depth = 0);

  void ComputeAccurateMulticastedAccesses(
      std::vector<analysis::LoopState>::reverse_iterator cur,
      const std::vector<problem::OperationSpace>& spatial_deltas,
      std::vector<problem::PerDataSpace<bool>>&
      unaccounted_delta,
      problem::PerDataSpace<std::vector<std::uint64_t>>& accesses,
      problem::PerDataSpace<std::vector<std::uint64_t>>& scatter_factors,
      problem::PerDataSpace<std::vector<std::uint64_t>>& cumulative_hops
    );

  void ComputeApproxMulticastedAccesses(
      std::vector<analysis::LoopState>::reverse_iterator cur,
      const std::vector<problem::OperationSpace>& spatial_deltas);

  void ComputeNetworkLinkTransfers(
      std::vector<analysis::LoopState>::reverse_iterator cur,
      const std::vector<problem::OperationSpace>& cur_spatial_deltas,
      std::vector<problem::PerDataSpace<bool>>&
      unaccounted_delta,
      problem::PerDataSpace<std::uint64_t>& link_transfers);
  
 public:  
  // API
  NestAnalysis();
  void Init(problem::WorkloadConfig* wc, const loop::Nest* nest);
  void Reset();
 
  std::vector<problem::PerDataSpace<std::size_t>> GetWorkingSetSizes_LTW() const;

  problem::PerDataSpace<std::vector<tiling::TileInfo>> GetWorkingSets();
  tiling::BodyInfo GetBodyInfo();

  // Serialization.
  friend class boost::serialization::access;

  template <class Archive>
  void serialize(Archive& ar, const unsigned int version=0) 
  {
    if(version == 0)
    {
      ar& BOOST_SERIALIZATION_NVP(nest_state_);
      ar& boost::serialization::make_nvp("work_sets_",boost::serialization::make_array(working_sets_.data(),working_sets_.size()));
      ar& BOOST_SERIALIZATION_NVP(working_sets_computed_);
      // ar& BOOST_SERIALIZATION_NVP(compute_cycles_);
    }
  }

  friend std::ostream& operator << (std::ostream& out, const NestAnalysis& n);  
};

} // namespace analysis
