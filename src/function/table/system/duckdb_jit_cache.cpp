#include "duckdb/function/table/system_functions.hpp"
#include "duckdb/main/client_context.hpp"
#include "duckdb/main/database.hpp"
#include "duckdb/execution/compilation_cache.hpp"
#include "duckdb/execution/expression_compiler.hpp"

namespace duckdb {

struct DuckDBJITCacheData : public GlobalTableFunctionState {
	DuckDBJITCacheData() : finished(false) {
	}

	bool finished;
};

static unique_ptr<FunctionData> DuckDBJITCacheBind(ClientContext &context, TableFunctionBindInput &input,
                                                   vector<LogicalType> &return_types, vector<string> &names) {
	// Define the columns for the result
	names.emplace_back("cache_size_bytes");
	return_types.emplace_back(LogicalType::BIGINT);

	names.emplace_back("entry_count");
	return_types.emplace_back(LogicalType::BIGINT);

	names.emplace_back("hit_count");
	return_types.emplace_back(LogicalType::BIGINT);

	names.emplace_back("miss_count");
	return_types.emplace_back(LogicalType::BIGINT);

	names.emplace_back("hit_rate");
	return_types.emplace_back(LogicalType::DOUBLE);

	names.emplace_back("max_cache_size_bytes");
	return_types.emplace_back(LogicalType::BIGINT);

	names.emplace_back("jit_enabled");
	return_types.emplace_back(LogicalType::BOOLEAN);

	names.emplace_back("compilation_threshold");
	return_types.emplace_back(LogicalType::BIGINT);

	return nullptr;
}

static unique_ptr<GlobalTableFunctionState> DuckDBJITCacheInit(ClientContext &context, TableFunctionInitInput &input) {
	return make_uniq<DuckDBJITCacheData>();
}

static void DuckDBJITCacheFunction(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
	auto &data = data_p.global_state->Cast<DuckDBJITCacheData>();
	if (data.finished) {
		return;
	}

	// Get the compilation cache
	auto &db = DatabaseInstance::GetDatabase(context);
	auto &global_cache = GlobalCompilationCache::Get(db);

	// Get configuration
	auto &config = DBConfig::GetConfig(context);

	// Output one row with cache statistics
	output.SetCardinality(1);

	// cache_size_bytes
	output.SetValue(0, 0, Value::BIGINT(static_cast<int64_t>(global_cache.GetTotalSizeBytes())));

	// entry_count
	output.SetValue(1, 0, Value::BIGINT(static_cast<int64_t>(global_cache.GetTotalEntries())));

	// hit_count - not directly available from global cache, use 0 as placeholder
	output.SetValue(2, 0, Value::BIGINT(0));

	// miss_count - not directly available from global cache, use 0 as placeholder
	output.SetValue(3, 0, Value::BIGINT(0));

	// hit_rate - not directly available from global cache, use 0.0 as placeholder
	output.SetValue(4, 0, Value::DOUBLE(0.0));

	// max_cache_size_bytes
	output.SetValue(5, 0, Value::BIGINT(static_cast<int64_t>(config.options.jit_cache_size)));

	// jit_enabled
	output.SetValue(6, 0, Value::BOOLEAN(config.options.enable_jit_compilation));

	// compilation_threshold
	output.SetValue(7, 0, Value::BIGINT(static_cast<int64_t>(config.options.jit_compilation_threshold)));

	data.finished = true;
}

void DuckDBJITCacheFun::RegisterFunction(BuiltinFunctions &set) {
	set.AddFunction(TableFunction("duckdb_jit_cache", {}, DuckDBJITCacheFunction, DuckDBJITCacheBind, DuckDBJITCacheInit));
}

} // namespace duckdb
