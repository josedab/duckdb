//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/execution/operator/join/join_algorithm.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/common.hpp"

namespace duckdb {

//! JoinAlgorithm represents the different join algorithms that can be used
enum class JoinAlgorithm : uint8_t {
	HASH_JOIN = 0,
	NESTED_LOOP = 1,
	MERGE_JOIN = 2,
	INDEX_JOIN = 3
};

//! Convert JoinAlgorithm to string for display/profiling
inline string JoinAlgorithmToString(JoinAlgorithm algorithm) {
	switch (algorithm) {
	case JoinAlgorithm::HASH_JOIN:
		return "HASH_JOIN";
	case JoinAlgorithm::NESTED_LOOP:
		return "NESTED_LOOP";
	case JoinAlgorithm::MERGE_JOIN:
		return "MERGE_JOIN";
	case JoinAlgorithm::INDEX_JOIN:
		return "INDEX_JOIN";
	default:
		return "UNKNOWN";
	}
}

} // namespace duckdb
