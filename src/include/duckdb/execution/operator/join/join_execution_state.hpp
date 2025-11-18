//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/execution/operator/join/join_execution_state.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/common.hpp"
#include "duckdb/common/chrono.hpp"

namespace duckdb {

//! JoinExecutionState tracks actual cardinalities during join execution
//! for making adaptive switching decisions
struct JoinExecutionState {
	//! Number of rows seen on the build side
	idx_t build_rows_seen = 0;
	//! Number of rows seen on the probe side
	idx_t probe_rows_seen = 0;

	//! Time spent building (in microseconds)
	uint64_t build_time_us = 0;
	//! Time spent probing (in microseconds)
	uint64_t probe_time_us = 0;

	//! Size of the hash table in bytes
	idx_t hash_table_size = 0;
	//! Number of hash collisions observed
	idx_t collision_count = 0;
	//! Total number of hash lookups performed
	idx_t total_lookups = 0;

	//! Whether the switch decision has been evaluated
	bool switch_evaluated = false;
	//! Whether a switch actually occurred
	bool switch_occurred = false;

	//! Calculate collision rate (0.0 to 1.0)
	double GetCollisionRate() const {
		if (total_lookups == 0) {
			return 0.0;
		}
		return static_cast<double>(collision_count) / static_cast<double>(total_lookups);
	}

	//! Get build time in milliseconds
	double GetBuildTimeMs() const {
		return static_cast<double>(build_time_us) / 1000.0;
	}

	//! Get probe time in milliseconds
	double GetProbeTimeMs() const {
		return static_cast<double>(probe_time_us) / 1000.0;
	}

	//! Reset the state for reuse
	void Reset() {
		build_rows_seen = 0;
		probe_rows_seen = 0;
		build_time_us = 0;
		probe_time_us = 0;
		hash_table_size = 0;
		collision_count = 0;
		total_lookups = 0;
		switch_evaluated = false;
		switch_occurred = false;
	}
};

} // namespace duckdb
