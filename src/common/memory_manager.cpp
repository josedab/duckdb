#include "duckdb/common/memory_manager.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/numeric_utils.hpp"
#include "duckdb/main/client_context.hpp"
#include "duckdb/main/database.hpp"

#include <cstdlib>

namespace duckdb {

UnifiedMemoryManager::UnifiedMemoryManager(idx_t memory_limit)
    : total_usage(0), peak_total_usage(0), total_limit(memory_limit) {
	// Initialize all category usage to zero
	for (idx_t i = 0; i < MEMORY_TAG_COUNT; i++) {
		category_usage[i].current = 0;
		category_usage[i].peak = 0;
		category_usage[i].limit = 0;
	}
}

UnifiedMemoryManager::~UnifiedMemoryManager() {
}

bool UnifiedMemoryManager::CheckLimits(idx_t size, MemoryTag tag) const {
	// Check category limit
	idx_t cat_idx = static_cast<idx_t>(tag);
	if (category_usage[cat_idx].limit > 0) {
		if (category_usage[cat_idx].current + size > category_usage[cat_idx].limit) {
			return false;
		}
	}

	// Check total limit
	if (total_limit > 0) {
		if (total_usage + size > total_limit) {
			return false;
		}
	}

	return true;
}

void UnifiedMemoryManager::UpdatePeak(MemoryTag tag) {
	idx_t cat_idx = static_cast<idx_t>(tag);
	idx_t current = category_usage[cat_idx].current;
	idx_t peak = category_usage[cat_idx].peak;

	// Update category peak if current exceeds it
	while (current > peak) {
		if (category_usage[cat_idx].peak.compare_exchange_weak(peak, current)) {
			break;
		}
		peak = category_usage[cat_idx].peak;
	}

	// Update total peak
	idx_t total = total_usage;
	idx_t total_peak = peak_total_usage;
	while (total > total_peak) {
		if (peak_total_usage.compare_exchange_weak(total_peak, total)) {
			break;
		}
		total_peak = peak_total_usage;
	}
}

data_ptr_t UnifiedMemoryManager::Allocate(idx_t size, MemoryTag tag) {
	// Check limits first
	if (!CheckLimits(size, tag)) {
		// Try to free memory
		OnMemoryPressure();
		if (!CheckLimits(size, tag)) {
			throw OutOfMemoryException("Failed to allocate %llu bytes for %s (limit: %llu, current: %llu)",
			                           size, EnumUtil::ToString(tag).c_str(), total_limit, total_usage.load());
		}
	}

	// Allocate memory
	auto ptr = static_cast<data_ptr_t>(std::malloc(size));
	if (!ptr) {
		throw OutOfMemoryException("Failed to allocate %llu bytes - malloc returned null", size);
	}

	// Track allocation
	TrackAllocation(size, tag);

	return ptr;
}

void UnifiedMemoryManager::Deallocate(data_ptr_t ptr, idx_t size, MemoryTag tag) {
	if (ptr) {
		std::free(ptr);
		UntrackAllocation(size, tag);
	}
}

data_ptr_t UnifiedMemoryManager::Reallocate(data_ptr_t ptr, idx_t old_size, idx_t new_size, MemoryTag tag) {
	if (new_size == old_size) {
		return ptr;
	}

	if (new_size > old_size) {
		// Growing allocation - check limits
		idx_t additional = new_size - old_size;
		if (!CheckLimits(additional, tag)) {
			OnMemoryPressure();
			if (!CheckLimits(additional, tag)) {
				throw OutOfMemoryException("Failed to reallocate from %llu to %llu bytes for %s",
				                           old_size, new_size, EnumUtil::ToString(tag).c_str());
			}
		}
	}

	auto new_ptr = static_cast<data_ptr_t>(std::realloc(ptr, new_size));
	if (!new_ptr && new_size > 0) {
		throw OutOfMemoryException("Failed to reallocate to %llu bytes - realloc returned null", new_size);
	}

	// Update tracking
	idx_t cat_idx = static_cast<idx_t>(tag);
	if (new_size > old_size) {
		idx_t diff = new_size - old_size;
		category_usage[cat_idx].current += diff;
		total_usage += diff;
		UpdatePeak(tag);
	} else {
		idx_t diff = old_size - new_size;
		category_usage[cat_idx].current -= diff;
		total_usage -= diff;
	}

	return new_ptr;
}

data_ptr_t UnifiedMemoryManager::TryAllocate(idx_t size, MemoryTag tag) {
	// Check limits first
	if (!CheckLimits(size, tag)) {
		OnMemoryPressure();
		if (!CheckLimits(size, tag)) {
			return nullptr;
		}
	}

	// Try to allocate
	auto ptr = static_cast<data_ptr_t>(std::malloc(size));
	if (!ptr) {
		return nullptr;
	}

	// Track allocation
	TrackAllocation(size, tag);

	return ptr;
}

void UnifiedMemoryManager::OnMemoryPressure() {
	// Notify all registered callbacks
	lock_guard<mutex> guard(callback_mutex);
	for (auto &callback : pressure_callbacks) {
		callback();
	}
}

idx_t UnifiedMemoryManager::GetUsedMemory(MemoryTag tag) const {
	idx_t cat_idx = static_cast<idx_t>(tag);
	return category_usage[cat_idx].current;
}

idx_t UnifiedMemoryManager::GetTotalUsedMemory() const {
	return total_usage;
}

idx_t UnifiedMemoryManager::GetPeakMemory(MemoryTag tag) const {
	idx_t cat_idx = static_cast<idx_t>(tag);
	return category_usage[cat_idx].peak;
}

idx_t UnifiedMemoryManager::GetTotalPeakMemory() const {
	return peak_total_usage;
}

MemoryUsageReport UnifiedMemoryManager::GetUsageReport() const {
	MemoryUsageReport report;
	report.total = total_usage;
	report.peak = peak_total_usage;
	report.limit = total_limit;

	for (idx_t i = 0; i < MEMORY_TAG_COUNT; i++) {
		report.by_category[i] = category_usage[i].current;
		report.peak_by_category[i] = category_usage[i].peak;
	}

	return report;
}

void UnifiedMemoryManager::SetMemoryLimit(idx_t limit) {
	total_limit = limit;

	// If we're over the new limit, try to free memory
	if (limit > 0 && total_usage > limit) {
		OnMemoryPressure();
	}
}

idx_t UnifiedMemoryManager::GetMemoryLimit() const {
	return total_limit;
}

void UnifiedMemoryManager::SetCategoryLimit(MemoryTag tag, idx_t limit) {
	idx_t cat_idx = static_cast<idx_t>(tag);
	category_usage[cat_idx].limit = limit;

	// If we're over the new limit, try to free memory
	if (limit > 0 && category_usage[cat_idx].current > limit) {
		OnMemoryPressure();
	}
}

idx_t UnifiedMemoryManager::GetCategoryLimit(MemoryTag tag) const {
	idx_t cat_idx = static_cast<idx_t>(tag);
	return category_usage[cat_idx].limit;
}

void UnifiedMemoryManager::RegisterPressureCallback(memory_pressure_callback_t callback) {
	lock_guard<mutex> guard(callback_mutex);
	pressure_callbacks.push_back(std::move(callback));
}

void UnifiedMemoryManager::ClearPressureCallbacks() {
	lock_guard<mutex> guard(callback_mutex);
	pressure_callbacks.clear();
}

void UnifiedMemoryManager::Reset() {
	// Reset all counters
	for (idx_t i = 0; i < MEMORY_TAG_COUNT; i++) {
		category_usage[i].current = 0;
		category_usage[i].peak = 0;
		category_usage[i].limit = 0;
	}
	total_usage = 0;
	peak_total_usage = 0;

	// Clear callbacks
	ClearPressureCallbacks();
}

void UnifiedMemoryManager::TrackAllocation(idx_t size, MemoryTag tag) {
	idx_t cat_idx = static_cast<idx_t>(tag);
	category_usage[cat_idx].current += size;
	total_usage += size;
	UpdatePeak(tag);
}

void UnifiedMemoryManager::UntrackAllocation(idx_t size, MemoryTag tag) {
	idx_t cat_idx = static_cast<idx_t>(tag);
	category_usage[cat_idx].current -= size;
	total_usage -= size;
}

bool UnifiedMemoryManager::WouldExceedLimit(idx_t size, MemoryTag tag) const {
	return !CheckLimits(size, tag);
}

UnifiedMemoryManager &UnifiedMemoryManager::Get(DatabaseInstance &db) {
	return db.GetUnifiedMemoryManager();
}

UnifiedMemoryManager &UnifiedMemoryManager::Get(ClientContext &context) {
	return UnifiedMemoryManager::Get(DatabaseInstance::GetDatabase(context));
}

} // namespace duckdb
