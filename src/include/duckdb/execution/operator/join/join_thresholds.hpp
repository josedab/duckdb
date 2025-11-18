//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/execution/operator/join/join_thresholds.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/common.hpp"

namespace duckdb {

//! JoinThresholds defines when adaptive join switching should occur
struct JoinThresholds {
	//! Switch to nested loop if build side smaller than this
	idx_t nested_loop_threshold = 128;

	//! Switch to merge join if collision rate exceeds this (0.0 to 1.0)
	double high_collision_threshold = 0.3;

	//! Only switch if improvement exceeds this multiplier
	double switch_threshold = 1.5;

	//! Minimum rows before evaluating switch
	idx_t min_rows_for_evaluation = 1000;

	//! Cost constants for estimation
	static constexpr double HASH_COST = 1.0;
	static constexpr double LOOKUP_COST = 1.5;
	static constexpr double COMPARE_COST = 0.5;
	static constexpr double COLLISION_PENALTY = 2.0;
};

} // namespace duckdb
