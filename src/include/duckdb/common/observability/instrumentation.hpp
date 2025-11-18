//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/common/observability/instrumentation.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/common.hpp"
#include "duckdb/common/chrono.hpp"

namespace duckdb {

class DatabaseInstance;
class ClientContext;
class MetricsRegistry;

//! Helper class for instrumenting DuckDB operations
class Instrumentation {
public:
	//! Update query metrics after query execution
	static void RecordQueryExecution(DatabaseInstance &db, double duration_seconds, bool success);

	//! Update memory metrics
	static void UpdateMemoryMetrics(DatabaseInstance &db, idx_t memory_usage, idx_t buffer_pool_size,
	                                idx_t buffer_pool_used);

	//! Update connection count
	static void UpdateConnectionCount(DatabaseInstance &db, int64_t delta);

	//! Record block I/O
	static void RecordBlockRead(DatabaseInstance &db);
	static void RecordBlockWrite(DatabaseInstance &db);

	//! Record transaction events
	static void RecordTransactionCommit(DatabaseInstance &db);
	static void RecordTransactionRollback(DatabaseInstance &db);

	//! Record checkpoint
	static void RecordCheckpoint(DatabaseInstance &db, double duration_seconds);

	//! Update database size
	static void UpdateDatabaseSize(DatabaseInstance &db, idx_t size_bytes);
};

//! RAII helper for timing query execution
class QueryTimer {
public:
	QueryTimer(DatabaseInstance &db);
	~QueryTimer();

	//! Mark the query as failed
	void SetFailed() {
		success = false;
	}

private:
	DatabaseInstance &db;
	std::chrono::time_point<std::chrono::high_resolution_clock> start_time;
	bool success;
};

} // namespace duckdb
