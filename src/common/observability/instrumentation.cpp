#include "duckdb/common/observability/instrumentation.hpp"
#include "duckdb/common/observability/metrics_registry.hpp"
#include "duckdb/main/database.hpp"
#include "duckdb/main/config.hpp"

namespace duckdb {

void Instrumentation::RecordQueryExecution(DatabaseInstance &db, double duration_seconds, bool success) {
	if (!db.config.metrics_registry) {
		return;
	}

	auto &registry = *db.config.metrics_registry;

	// Increment query counter
	auto *queries_counter = registry.GetCounter("duckdb_queries_total");
	if (queries_counter) {
		queries_counter->Inc();
	}

	// Record duration
	auto *duration_histogram = registry.GetHistogram("duckdb_query_duration_seconds");
	if (duration_histogram) {
		duration_histogram->Observe(duration_seconds);
	}

	// Record errors
	if (!success) {
		auto *errors_counter = registry.GetCounter("duckdb_query_errors_total");
		if (errors_counter) {
			errors_counter->Inc();
		}
	}
}

void Instrumentation::UpdateMemoryMetrics(DatabaseInstance &db, idx_t memory_usage, idx_t buffer_pool_size,
                                          idx_t buffer_pool_used) {
	if (!db.config.metrics_registry) {
		return;
	}

	auto &registry = *db.config.metrics_registry;

	auto *memory_gauge = registry.GetGauge("duckdb_memory_usage_bytes");
	if (memory_gauge) {
		memory_gauge->Set(static_cast<double>(memory_usage));
	}

	auto *pool_size_gauge = registry.GetGauge("duckdb_buffer_pool_size_bytes");
	if (pool_size_gauge) {
		pool_size_gauge->Set(static_cast<double>(buffer_pool_size));
	}

	auto *pool_used_gauge = registry.GetGauge("duckdb_buffer_pool_used_bytes");
	if (pool_used_gauge) {
		pool_used_gauge->Set(static_cast<double>(buffer_pool_used));
	}
}

void Instrumentation::UpdateConnectionCount(DatabaseInstance &db, int64_t delta) {
	if (!db.config.metrics_registry) {
		return;
	}

	auto &registry = *db.config.metrics_registry;

	auto *connections_gauge = registry.GetGauge("duckdb_connections_active");
	if (connections_gauge) {
		if (delta > 0) {
			connections_gauge->Inc(static_cast<double>(delta));
		} else {
			connections_gauge->Dec(static_cast<double>(-delta));
		}
	}
}

void Instrumentation::RecordBlockRead(DatabaseInstance &db) {
	if (!db.config.metrics_registry) {
		return;
	}

	auto &registry = *db.config.metrics_registry;

	auto *blocks_counter = registry.GetCounter("duckdb_blocks_read_total");
	if (blocks_counter) {
		blocks_counter->Inc();
	}
}

void Instrumentation::RecordBlockWrite(DatabaseInstance &db) {
	if (!db.config.metrics_registry) {
		return;
	}

	auto &registry = *db.config.metrics_registry;

	auto *blocks_counter = registry.GetCounter("duckdb_blocks_written_total");
	if (blocks_counter) {
		blocks_counter->Inc();
	}
}

void Instrumentation::RecordTransactionCommit(DatabaseInstance &db) {
	if (!db.config.metrics_registry) {
		return;
	}

	auto &registry = *db.config.metrics_registry;

	auto *tx_counter = registry.GetCounter("duckdb_transactions_total");
	if (tx_counter) {
		tx_counter->Inc();
	}

	auto *commit_counter = registry.GetCounter("duckdb_transactions_committed_total");
	if (commit_counter) {
		commit_counter->Inc();
	}
}

void Instrumentation::RecordTransactionRollback(DatabaseInstance &db) {
	if (!db.config.metrics_registry) {
		return;
	}

	auto &registry = *db.config.metrics_registry;

	auto *tx_counter = registry.GetCounter("duckdb_transactions_total");
	if (tx_counter) {
		tx_counter->Inc();
	}

	auto *rollback_counter = registry.GetCounter("duckdb_transactions_rolled_back_total");
	if (rollback_counter) {
		rollback_counter->Inc();
	}
}

void Instrumentation::RecordCheckpoint(DatabaseInstance &db, double duration_seconds) {
	if (!db.config.metrics_registry) {
		return;
	}

	auto &registry = *db.config.metrics_registry;

	auto *checkpoint_counter = registry.GetCounter("duckdb_checkpoints_total");
	if (checkpoint_counter) {
		checkpoint_counter->Inc();
	}

	auto *duration_histogram = registry.GetHistogram("duckdb_checkpoint_duration_seconds");
	if (duration_histogram) {
		duration_histogram->Observe(duration_seconds);
	}
}

void Instrumentation::UpdateDatabaseSize(DatabaseInstance &db, idx_t size_bytes) {
	if (!db.config.metrics_registry) {
		return;
	}

	auto &registry = *db.config.metrics_registry;

	auto *size_gauge = registry.GetGauge("duckdb_database_size_bytes");
	if (size_gauge) {
		size_gauge->Set(static_cast<double>(size_bytes));
	}
}

//===----------------------------------------------------------------------===//
// QueryTimer
//===----------------------------------------------------------------------===//

QueryTimer::QueryTimer(DatabaseInstance &db) : db(db), success(true) {
	start_time = std::chrono::high_resolution_clock::now();
}

QueryTimer::~QueryTimer() {
	auto end_time = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration<double>(end_time - start_time);
	Instrumentation::RecordQueryExecution(db, duration.count(), success);
}

} // namespace duckdb
