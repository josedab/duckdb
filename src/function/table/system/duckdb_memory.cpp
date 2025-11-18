#include "duckdb/function/table/system_functions.hpp"
#include "duckdb/storage/buffer_manager.hpp"
#include "duckdb/common/memory_manager.hpp"
#include "duckdb/main/database.hpp"

namespace duckdb {

struct DuckDBMemoryData : public GlobalTableFunctionState {
	DuckDBMemoryData() : offset(0) {
	}

	vector<MemoryInformation> entries;
	MemoryUsageReport unified_report;
	idx_t offset;
};

static unique_ptr<FunctionData> DuckDBMemoryBind(ClientContext &context, TableFunctionBindInput &input,
                                                 vector<LogicalType> &return_types, vector<string> &names) {
	names.emplace_back("tag");
	return_types.emplace_back(LogicalType::VARCHAR);

	names.emplace_back("memory_usage_bytes");
	return_types.emplace_back(LogicalType::BIGINT);

	names.emplace_back("temporary_storage_bytes");
	return_types.emplace_back(LogicalType::BIGINT);

	names.emplace_back("peak_memory_bytes");
	return_types.emplace_back(LogicalType::BIGINT);

	names.emplace_back("limit_bytes");
	return_types.emplace_back(LogicalType::BIGINT);

	return nullptr;
}

unique_ptr<GlobalTableFunctionState> DuckDBMemoryInit(ClientContext &context, TableFunctionInitInput &input) {
	auto result = make_uniq<DuckDBMemoryData>();
	result->entries = BufferManager::GetBufferManager(context).GetMemoryUsageInfo();
	result->unified_report = UnifiedMemoryManager::Get(context).GetUsageReport();
	return std::move(result);
}

int64_t ClampReportedMemory(idx_t memory_usage) {
	if (memory_usage > static_cast<idx_t>(NumericLimits<int64_t>::Maximum())) {
		return 0;
	}
	return UnsafeNumericCast<int64_t>(memory_usage);
}

void DuckDBMemoryFunction(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
	auto &data = data_p.global_state->Cast<DuckDBMemoryData>();
	if (data.offset >= data.entries.size()) {
		// finished returning values
		return;
	}
	// start returning values
	// either fill up the chunk or return all the remaining columns
	idx_t count = 0;
	while (data.offset < data.entries.size() && count < STANDARD_VECTOR_SIZE) {
		auto &entry = data.entries[data.offset];
		idx_t tag_idx = static_cast<idx_t>(entry.tag);
		// return values:
		idx_t col = 0;
		// tag, VARCHAR
		output.SetValue(col++, count, EnumUtil::ToString(entry.tag));
		// memory_usage_bytes, BIGINT
		output.SetValue(col++, count, Value::BIGINT(ClampReportedMemory(entry.size)));
		// temporary_storage_bytes, BIGINT
		output.SetValue(col++, count, Value::BIGINT(ClampReportedMemory(entry.evicted_data)));
		// peak_memory_bytes, BIGINT - from unified memory manager
		idx_t peak = tag_idx < MEMORY_TAG_COUNT ? data.unified_report.peak_by_category[tag_idx] : 0;
		output.SetValue(col++, count, Value::BIGINT(ClampReportedMemory(peak)));
		// limit_bytes, BIGINT - global limit (0 = no limit)
		output.SetValue(col++, count, Value::BIGINT(ClampReportedMemory(data.unified_report.limit)));
		data.offset++;
		count++;
	}
	output.SetCardinality(count);
}

void DuckDBMemoryFun::RegisterFunction(BuiltinFunctions &set) {
	set.AddFunction(TableFunction("duckdb_memory", {}, DuckDBMemoryFunction, DuckDBMemoryBind, DuckDBMemoryInit));
}

} // namespace duckdb
