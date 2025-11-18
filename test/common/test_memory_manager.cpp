#include "catch.hpp"
#include "duckdb/common/memory_manager.hpp"

#include <atomic>

using namespace duckdb;
using namespace std;

TEST_CASE("Basic memory manager allocation tracking", "[memory_manager]") {
	UnifiedMemoryManager manager;

	// Initial state should be zero
	REQUIRE(manager.GetTotalUsedMemory() == 0);
	REQUIRE(manager.GetTotalPeakMemory() == 0);

	// Allocate some memory
	auto ptr = manager.Allocate(1024, MemoryTag::HASH_TABLE);
	REQUIRE(ptr != nullptr);
	REQUIRE(manager.GetTotalUsedMemory() == 1024);
	REQUIRE(manager.GetUsedMemory(MemoryTag::HASH_TABLE) == 1024);
	REQUIRE(manager.GetTotalPeakMemory() == 1024);

	// Allocate more in a different category
	auto ptr2 = manager.Allocate(2048, MemoryTag::ORDER_BY);
	REQUIRE(ptr2 != nullptr);
	REQUIRE(manager.GetTotalUsedMemory() == 3072);
	REQUIRE(manager.GetUsedMemory(MemoryTag::ORDER_BY) == 2048);
	REQUIRE(manager.GetTotalPeakMemory() == 3072);

	// Deallocate first allocation
	manager.Deallocate(ptr, 1024, MemoryTag::HASH_TABLE);
	REQUIRE(manager.GetTotalUsedMemory() == 2048);
	REQUIRE(manager.GetUsedMemory(MemoryTag::HASH_TABLE) == 0);
	// Peak should remain at 3072
	REQUIRE(manager.GetTotalPeakMemory() == 3072);

	// Deallocate second allocation
	manager.Deallocate(ptr2, 2048, MemoryTag::ORDER_BY);
	REQUIRE(manager.GetTotalUsedMemory() == 0);
	REQUIRE(manager.GetUsedMemory(MemoryTag::ORDER_BY) == 0);
}

TEST_CASE("Memory manager reallocation tracking", "[memory_manager]") {
	UnifiedMemoryManager manager;

	// Initial allocation
	auto ptr = manager.Allocate(1024, MemoryTag::COLUMN_DATA);
	REQUIRE(manager.GetTotalUsedMemory() == 1024);

	// Grow allocation
	ptr = manager.Reallocate(ptr, 1024, 4096, MemoryTag::COLUMN_DATA);
	REQUIRE(ptr != nullptr);
	REQUIRE(manager.GetTotalUsedMemory() == 4096);
	REQUIRE(manager.GetUsedMemory(MemoryTag::COLUMN_DATA) == 4096);

	// Shrink allocation
	ptr = manager.Reallocate(ptr, 4096, 512, MemoryTag::COLUMN_DATA);
	REQUIRE(manager.GetTotalUsedMemory() == 512);
	REQUIRE(manager.GetUsedMemory(MemoryTag::COLUMN_DATA) == 512);
	// Peak should remain at 4096
	REQUIRE(manager.GetPeakMemory(MemoryTag::COLUMN_DATA) == 4096);

	manager.Deallocate(ptr, 512, MemoryTag::COLUMN_DATA);
}

TEST_CASE("Memory manager memory limits", "[memory_manager]") {
	// Create manager with 10KB limit
	UnifiedMemoryManager manager(10240);

	REQUIRE(manager.GetMemoryLimit() == 10240);

	// Allocate within limit
	auto ptr = manager.Allocate(5120, MemoryTag::HASH_TABLE);
	REQUIRE(ptr != nullptr);
	REQUIRE(manager.GetTotalUsedMemory() == 5120);

	// Check if allocation would exceed limit
	REQUIRE(manager.WouldExceedLimit(6000, MemoryTag::ORDER_BY) == true);
	REQUIRE(manager.WouldExceedLimit(5000, MemoryTag::ORDER_BY) == false);

	// TryAllocate should return nullptr when exceeding limit
	auto ptr2 = manager.TryAllocate(6000, MemoryTag::ORDER_BY);
	REQUIRE(ptr2 == nullptr);

	// TryAllocate within limit should succeed
	ptr2 = manager.TryAllocate(4000, MemoryTag::ORDER_BY);
	REQUIRE(ptr2 != nullptr);
	REQUIRE(manager.GetTotalUsedMemory() == 9120);

	manager.Deallocate(ptr, 5120, MemoryTag::HASH_TABLE);
	manager.Deallocate(ptr2, 4000, MemoryTag::ORDER_BY);
}

TEST_CASE("Memory manager category limits", "[memory_manager]") {
	UnifiedMemoryManager manager;

	// Set category-specific limit
	manager.SetCategoryLimit(MemoryTag::HASH_TABLE, 2048);
	REQUIRE(manager.GetCategoryLimit(MemoryTag::HASH_TABLE) == 2048);

	// Allocate within category limit
	auto ptr = manager.Allocate(1024, MemoryTag::HASH_TABLE);
	REQUIRE(ptr != nullptr);

	// Check if would exceed category limit
	REQUIRE(manager.WouldExceedLimit(1500, MemoryTag::HASH_TABLE) == true);

	// Other categories should not be affected
	auto ptr2 = manager.Allocate(4096, MemoryTag::ORDER_BY);
	REQUIRE(ptr2 != nullptr);

	manager.Deallocate(ptr, 1024, MemoryTag::HASH_TABLE);
	manager.Deallocate(ptr2, 4096, MemoryTag::ORDER_BY);
}

TEST_CASE("Memory manager usage report", "[memory_manager]") {
	UnifiedMemoryManager manager(8192);

	auto ptr1 = manager.Allocate(1024, MemoryTag::HASH_TABLE);
	auto ptr2 = manager.Allocate(2048, MemoryTag::ORDER_BY);
	auto ptr3 = manager.Allocate(512, MemoryTag::COLUMN_DATA);

	auto report = manager.GetUsageReport();
	REQUIRE(report.total == 3584);
	REQUIRE(report.limit == 8192);
	REQUIRE(report.by_category[static_cast<idx_t>(MemoryTag::HASH_TABLE)] == 1024);
	REQUIRE(report.by_category[static_cast<idx_t>(MemoryTag::ORDER_BY)] == 2048);
	REQUIRE(report.by_category[static_cast<idx_t>(MemoryTag::COLUMN_DATA)] == 512);

	// Deallocate and verify peak is preserved
	manager.Deallocate(ptr1, 1024, MemoryTag::HASH_TABLE);
	manager.Deallocate(ptr2, 2048, MemoryTag::ORDER_BY);

	report = manager.GetUsageReport();
	REQUIRE(report.total == 512);
	REQUIRE(report.peak == 3584);
	REQUIRE(report.peak_by_category[static_cast<idx_t>(MemoryTag::HASH_TABLE)] == 1024);
	REQUIRE(report.peak_by_category[static_cast<idx_t>(MemoryTag::ORDER_BY)] == 2048);

	manager.Deallocate(ptr3, 512, MemoryTag::COLUMN_DATA);
}

TEST_CASE("Memory manager track/untrack allocation", "[memory_manager]") {
	UnifiedMemoryManager manager;

	// Track an external allocation
	manager.TrackAllocation(4096, MemoryTag::EXTENSION);
	REQUIRE(manager.GetTotalUsedMemory() == 4096);
	REQUIRE(manager.GetUsedMemory(MemoryTag::EXTENSION) == 4096);

	// Untrack the allocation
	manager.UntrackAllocation(4096, MemoryTag::EXTENSION);
	REQUIRE(manager.GetTotalUsedMemory() == 0);
	REQUIRE(manager.GetUsedMemory(MemoryTag::EXTENSION) == 0);
}

TEST_CASE("Memory manager pressure callbacks", "[memory_manager]") {
	UnifiedMemoryManager manager(1024);

	atomic<int> callback_count(0);

	// Register a pressure callback
	manager.RegisterPressureCallback([&callback_count]() {
		callback_count++;
	});

	// Trigger memory pressure by setting limit lower than current usage
	auto ptr = manager.Allocate(512, MemoryTag::HASH_TABLE);
	manager.SetMemoryLimit(256); // Should trigger pressure callback

	REQUIRE(callback_count > 0);

	manager.ClearPressureCallbacks();
	manager.Deallocate(ptr, 512, MemoryTag::HASH_TABLE);
}

TEST_CASE("Memory manager reset", "[memory_manager]") {
	UnifiedMemoryManager manager;

	// Allocate some memory
	auto ptr = manager.Allocate(1024, MemoryTag::HASH_TABLE);
	REQUIRE(manager.GetTotalUsedMemory() == 1024);

	manager.Deallocate(ptr, 1024, MemoryTag::HASH_TABLE);

	// Reset should clear all tracking
	manager.Reset();
	REQUIRE(manager.GetTotalUsedMemory() == 0);
	REQUIRE(manager.GetTotalPeakMemory() == 0);
	REQUIRE(manager.GetMemoryLimit() == 0);
}

TEST_CASE("Memory manager multiple categories", "[memory_manager]") {
	UnifiedMemoryManager manager;

	// Allocate across all categories
	vector<pair<data_ptr_t, MemoryTag>> allocations;

	allocations.push_back({manager.Allocate(100, MemoryTag::BASE_TABLE), MemoryTag::BASE_TABLE});
	allocations.push_back({manager.Allocate(200, MemoryTag::HASH_TABLE), MemoryTag::HASH_TABLE});
	allocations.push_back({manager.Allocate(300, MemoryTag::PARQUET_READER), MemoryTag::PARQUET_READER});
	allocations.push_back({manager.Allocate(400, MemoryTag::CSV_READER), MemoryTag::CSV_READER});
	allocations.push_back({manager.Allocate(500, MemoryTag::ORDER_BY), MemoryTag::ORDER_BY});

	REQUIRE(manager.GetTotalUsedMemory() == 1500);
	REQUIRE(manager.GetUsedMemory(MemoryTag::BASE_TABLE) == 100);
	REQUIRE(manager.GetUsedMemory(MemoryTag::HASH_TABLE) == 200);
	REQUIRE(manager.GetUsedMemory(MemoryTag::PARQUET_READER) == 300);
	REQUIRE(manager.GetUsedMemory(MemoryTag::CSV_READER) == 400);
	REQUIRE(manager.GetUsedMemory(MemoryTag::ORDER_BY) == 500);

	// Clean up
	for (auto &alloc : allocations) {
		idx_t size = 0;
		switch (alloc.second) {
		case MemoryTag::BASE_TABLE: size = 100; break;
		case MemoryTag::HASH_TABLE: size = 200; break;
		case MemoryTag::PARQUET_READER: size = 300; break;
		case MemoryTag::CSV_READER: size = 400; break;
		case MemoryTag::ORDER_BY: size = 500; break;
		default: break;
		}
		manager.Deallocate(alloc.first, size, alloc.second);
	}

	REQUIRE(manager.GetTotalUsedMemory() == 0);
}
