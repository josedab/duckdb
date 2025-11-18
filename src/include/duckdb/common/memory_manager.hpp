//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/common/memory_manager.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/atomic.hpp"
#include "duckdb/common/common.hpp"
#include "duckdb/common/enums/memory_tag.hpp"
#include "duckdb/common/mutex.hpp"
#include "duckdb/common/types.hpp"
#include "duckdb/common/vector.hpp"

#include <array>
#include <functional>

namespace duckdb {
class ClientContext;
class DatabaseInstance;

//! Memory usage report containing detailed breakdown by category
struct MemoryUsageReport {
	//! Memory usage by category/tag
	std::array<idx_t, MEMORY_TAG_COUNT> by_category;
	//! Peak memory usage by category/tag
	std::array<idx_t, MEMORY_TAG_COUNT> peak_by_category;
	//! Total current memory usage
	idx_t total;
	//! Peak total memory usage
	idx_t peak;
	//! Memory limit (0 = no limit)
	idx_t limit;
	//! Map of query ID to memory usage
	unordered_map<string, idx_t> by_query;
	//! Top memory consumers
	vector<pair<string, idx_t>> top_allocations;
};

//! Callback type for memory pressure notifications
using memory_pressure_callback_t = std::function<void()>;

//! Category-specific memory usage tracking
struct CategoryUsage {
	//! Current memory usage for this category
	atomic<idx_t> current;
	//! Peak memory usage for this category
	atomic<idx_t> peak;
	//! Memory limit for this category (0 = no limit)
	idx_t limit;

	CategoryUsage() : current(0), peak(0), limit(0) {
	}
};

//! UnifiedMemoryManager provides centralized memory tracking and coordination
//! across all DuckDB allocation systems (Allocator, BufferManager, ArenaAllocator)
class UnifiedMemoryManager {
public:
	explicit UnifiedMemoryManager(idx_t memory_limit = 0);
	~UnifiedMemoryManager();

	//! Allocate memory with tracking
	data_ptr_t Allocate(idx_t size, MemoryTag tag);

	//! Deallocate memory with tracking
	void Deallocate(data_ptr_t ptr, idx_t size, MemoryTag tag);

	//! Reallocate memory with tracking
	data_ptr_t Reallocate(data_ptr_t ptr, idx_t old_size, idx_t new_size, MemoryTag tag);

	//! Try to allocate without throwing - returns nullptr on failure
	data_ptr_t TryAllocate(idx_t size, MemoryTag tag);

	//! Handle memory pressure - attempt to free memory
	void OnMemoryPressure();

	//! Get current memory usage for a category
	idx_t GetUsedMemory(MemoryTag tag) const;

	//! Get total memory usage across all categories
	idx_t GetTotalUsedMemory() const;

	//! Get peak memory usage for a category
	idx_t GetPeakMemory(MemoryTag tag) const;

	//! Get total peak memory usage
	idx_t GetTotalPeakMemory() const;

	//! Get detailed memory usage report
	MemoryUsageReport GetUsageReport() const;

	//! Set global memory limit
	void SetMemoryLimit(idx_t limit);

	//! Get current memory limit
	idx_t GetMemoryLimit() const;

	//! Set category-specific memory limit
	void SetCategoryLimit(MemoryTag tag, idx_t limit);

	//! Get category-specific memory limit
	idx_t GetCategoryLimit(MemoryTag tag) const;

	//! Register a callback to be called on memory pressure
	void RegisterPressureCallback(memory_pressure_callback_t callback);

	//! Clear all pressure callbacks
	void ClearPressureCallbacks();

	//! Reset tracking (useful for testing)
	void Reset();

	//! Track allocation without actually allocating (for external allocations)
	void TrackAllocation(idx_t size, MemoryTag tag);

	//! Untrack allocation (for external deallocations)
	void UntrackAllocation(idx_t size, MemoryTag tag);

	//! Check if allocation would exceed limits
	bool WouldExceedLimit(idx_t size, MemoryTag tag) const;

	//! Get the singleton instance for a database
	static UnifiedMemoryManager &Get(DatabaseInstance &db);

	//! Get the singleton instance from client context
	static UnifiedMemoryManager &Get(ClientContext &context);

private:
	//! Check if allocation is within limits
	bool CheckLimits(idx_t size, MemoryTag tag) const;

	//! Update peak memory usage
	void UpdatePeak(MemoryTag tag);

	//! Category-specific usage tracking
	std::array<CategoryUsage, MEMORY_TAG_COUNT> category_usage;

	//! Total memory usage across all categories
	atomic<idx_t> total_usage;

	//! Peak total memory usage
	atomic<idx_t> peak_total_usage;

	//! Global memory limit (0 = no limit)
	idx_t total_limit;

	//! Mutex for pressure callbacks
	mutex callback_mutex;

	//! Registered memory pressure callbacks
	vector<memory_pressure_callback_t> pressure_callbacks;
};

} // namespace duckdb
