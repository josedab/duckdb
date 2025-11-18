#include "duckdb/common/observability/metrics_registry.hpp"

namespace duckdb {

//! Register all built-in DuckDB metrics
void RegisterBuiltinMetrics(MetricsRegistry &registry) {
	// Query metrics
	registry.RegisterCounter("duckdb_queries_total", "Total number of queries executed");

	registry.RegisterHistogram("duckdb_query_duration_seconds", "Query execution duration in seconds",
	                           {0.001, 0.01, 0.1, 1.0, 10.0, 60.0, 300.0});

	registry.RegisterCounter("duckdb_query_errors_total", "Total number of query errors");

	// Memory metrics
	registry.RegisterGauge("duckdb_memory_usage_bytes", "Current memory usage in bytes");

	registry.RegisterGauge("duckdb_buffer_pool_size_bytes", "Buffer pool size in bytes");

	registry.RegisterGauge("duckdb_buffer_pool_used_bytes", "Buffer pool used memory in bytes");

	// Storage metrics
	registry.RegisterGauge("duckdb_database_size_bytes", "Database file size in bytes");

	registry.RegisterCounter("duckdb_blocks_read_total", "Total blocks read from disk");

	registry.RegisterCounter("duckdb_blocks_written_total", "Total blocks written to disk");

	// Connection metrics
	registry.RegisterGauge("duckdb_connections_active", "Number of active connections");

	// Transaction metrics
	registry.RegisterCounter("duckdb_transactions_total", "Total number of transactions");

	registry.RegisterCounter("duckdb_transactions_committed_total", "Total number of committed transactions");

	registry.RegisterCounter("duckdb_transactions_rolled_back_total", "Total number of rolled back transactions");

	// Checkpoint metrics
	registry.RegisterCounter("duckdb_checkpoints_total", "Total number of checkpoints");

	registry.RegisterHistogram("duckdb_checkpoint_duration_seconds", "Checkpoint duration in seconds",
	                           {0.1, 1.0, 10.0, 60.0, 300.0});
}

} // namespace duckdb
