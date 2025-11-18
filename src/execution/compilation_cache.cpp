#include "duckdb/execution/compilation_cache.hpp"

#include "duckdb/main/client_context.hpp"
#include "duckdb/main/config.hpp"
#include "duckdb/main/database.hpp"
#include "duckdb/planner/expression/list.hpp"

namespace duckdb {

//===--------------------------------------------------------------------===//
// CompilationCache
//===--------------------------------------------------------------------===//
CompilationCache::CompilationCache(ClientContext &context)
    : context(context), current_size(0), access_counter(0), hit_count(0), miss_count(0) {
	// Default max cache size: 100MB
	max_cache_size = 100 * 1024 * 1024;
}

CompilationCache::~CompilationCache() {
}

shared_ptr<CompiledExpression> CompilationCache::GetOrCompile(const Expression &expr, ExpressionCompiler &compiler) {
	hash_t hash = HashExpression(expr);

	// Try to get from cache first
	auto cached = Get(hash);
	if (cached) {
		return cached;
	}

	// Not in cache, compile the expression
	auto compiled = compiler.Compile(expr);
	if (!compiled) {
		return nullptr;
	}

	// Convert to shared_ptr and add to cache
	auto shared = std::make_shared<CompiledExpression>(std::move(*compiled));
	Put(hash, shared);

	return shared;
}

shared_ptr<CompiledExpression> CompilationCache::Get(hash_t hash) {
	lock_guard<mutex> lock(cache_lock);

	auto it = cache.find(hash);
	if (it == cache.end()) {
		miss_count++;
		return nullptr;
	}

	hit_count++;
	TouchEntry(hash);
	return it->second.compiled_expression;
}

void CompilationCache::Put(hash_t hash, shared_ptr<CompiledExpression> compiled) {
	lock_guard<mutex> lock(cache_lock);

	// Check if already in cache
	auto it = cache.find(hash);
	if (it != cache.end()) {
		// Update existing entry
		TouchEntry(hash);
		return;
	}

	// Estimate size of the compiled expression
	idx_t entry_size = EstimateSize(*compiled);

	// Evict entries if necessary to make room
	while (current_size + entry_size > max_cache_size && !cache.empty()) {
		// Evict the least recently used entry
		if (lru_list.empty()) {
			break;
		}
		hash_t lru_hash = lru_list.back();
		lru_list.pop_back();
		lru_map.erase(lru_hash);

		auto cache_it = cache.find(lru_hash);
		if (cache_it != cache.end()) {
			current_size -= cache_it->second.size_bytes;
			cache.erase(cache_it);
		}
	}

	// Add new entry
	CompilationCacheEntry entry;
	entry.compiled_expression = std::move(compiled);
	entry.last_access = access_counter++;
	entry.size_bytes = entry_size;

	cache[hash] = std::move(entry);
	current_size += entry_size;

	// Add to LRU list
	lru_list.push_front(hash);
	lru_map[hash] = lru_list.begin();
}

void CompilationCache::Evict(idx_t target_size) {
	lock_guard<mutex> lock(cache_lock);

	while (current_size > target_size && !cache.empty()) {
		if (lru_list.empty()) {
			break;
		}

		hash_t lru_hash = lru_list.back();
		lru_list.pop_back();
		lru_map.erase(lru_hash);

		auto it = cache.find(lru_hash);
		if (it != cache.end()) {
			current_size -= it->second.size_bytes;
			cache.erase(it);
		}
	}
}

void CompilationCache::Clear() {
	lock_guard<mutex> lock(cache_lock);

	cache.clear();
	lru_list.clear();
	lru_map.clear();
	current_size = 0;
}

idx_t CompilationCache::GetCacheSize() const {
	lock_guard<mutex> lock(cache_lock);
	return current_size;
}

idx_t CompilationCache::GetEntryCount() const {
	lock_guard<mutex> lock(cache_lock);
	return cache.size();
}

idx_t CompilationCache::GetHitCount() const {
	lock_guard<mutex> lock(cache_lock);
	return hit_count;
}

idx_t CompilationCache::GetMissCount() const {
	lock_guard<mutex> lock(cache_lock);
	return miss_count;
}

double CompilationCache::GetHitRate() const {
	lock_guard<mutex> lock(cache_lock);
	idx_t total = hit_count + miss_count;
	if (total == 0) {
		return 0.0;
	}
	return static_cast<double>(hit_count) / static_cast<double>(total);
}

idx_t CompilationCache::GetMaxCacheSize() const {
	lock_guard<mutex> lock(cache_lock);
	return max_cache_size;
}

void CompilationCache::SetMaxCacheSize(idx_t size) {
	lock_guard<mutex> lock(cache_lock);
	max_cache_size = size;
}

hash_t CompilationCache::HashExpression(const Expression &expr) const {
	// Delegate to a simpler hash implementation
	hash_t hash = 0;

	// Hash expression class
	hash = CombineHash(hash, Hash<uint8_t>(static_cast<uint8_t>(expr.GetExpressionClass())));

	// Hash return type
	hash = CombineHash(hash, expr.return_type.Hash());

	// Recursively hash children based on expression type
	switch (expr.GetExpressionClass()) {
	case ExpressionClass::BOUND_CONSTANT: {
		auto &constant = expr.Cast<BoundConstantExpression>();
		hash = CombineHash(hash, constant.value.Hash());
		break;
	}

	case ExpressionClass::BOUND_REF: {
		auto &ref = expr.Cast<BoundReferenceExpression>();
		hash = CombineHash(hash, Hash<idx_t>(ref.index));
		break;
	}

	case ExpressionClass::BOUND_COMPARISON: {
		auto &comparison = expr.Cast<BoundComparisonExpression>();
		hash = CombineHash(hash, Hash<uint8_t>(static_cast<uint8_t>(comparison.type)));
		hash = CombineHash(hash, HashExpression(*comparison.left));
		hash = CombineHash(hash, HashExpression(*comparison.right));
		break;
	}

	case ExpressionClass::BOUND_CONJUNCTION: {
		auto &conjunction = expr.Cast<BoundConjunctionExpression>();
		hash = CombineHash(hash, Hash<uint8_t>(static_cast<uint8_t>(conjunction.type)));
		for (auto &child : conjunction.children) {
			hash = CombineHash(hash, HashExpression(*child));
		}
		break;
	}

	case ExpressionClass::BOUND_FUNCTION: {
		auto &func = expr.Cast<BoundFunctionExpression>();
		hash = CombineHash(hash, Hash<string>(func.function.name));
		for (auto &child : func.children) {
			hash = CombineHash(hash, HashExpression(*child));
		}
		break;
	}

	case ExpressionClass::BOUND_CAST: {
		auto &cast = expr.Cast<BoundCastExpression>();
		hash = CombineHash(hash, HashExpression(*cast.child));
		break;
	}

	case ExpressionClass::BOUND_OPERATOR: {
		auto &op = expr.Cast<BoundOperatorExpression>();
		hash = CombineHash(hash, Hash<uint8_t>(static_cast<uint8_t>(op.type)));
		for (auto &child : op.children) {
			hash = CombineHash(hash, HashExpression(*child));
		}
		break;
	}

	default:
		break;
	}

	return hash;
}

idx_t CompilationCache::EstimateSize(const CompiledExpression &compiled) const {
	// Estimate the memory usage of a compiled expression
	// This is a rough estimate - actual size depends on the generated code

	// Base overhead for the CompiledExpression object
	idx_t size = sizeof(CompiledExpression);

	// Estimate for compiled code (typically 1-10KB per expression)
	// Using a conservative estimate of 4KB per expression
	size += 4096;

	return size;
}

void CompilationCache::TouchEntry(hash_t hash) {
	// Move entry to front of LRU list
	auto lru_it = lru_map.find(hash);
	if (lru_it != lru_map.end()) {
		lru_list.erase(lru_it->second);
		lru_list.push_front(hash);
		lru_map[hash] = lru_list.begin();
	}

	// Update access counter for the entry
	auto cache_it = cache.find(hash);
	if (cache_it != cache.end()) {
		cache_it->second.last_access = access_counter++;
	}
}

//===--------------------------------------------------------------------===//
// GlobalCompilationCache
//===--------------------------------------------------------------------===//
GlobalCompilationCache::GlobalCompilationCache() {
}

GlobalCompilationCache &GlobalCompilationCache::Get(DatabaseInstance &db) {
	return db.GetObjectCache().GetOrCreate<GlobalCompilationCache>("jit_compilation_cache");
}

shared_ptr<CompiledExpression> GlobalCompilationCache::GetOrCompile(const Expression &expr, ExpressionCompiler &compiler,
                                                                    ClientContext &context) {
	lock_guard<mutex> lock(init_lock);

	// Lazily initialize the cache
	if (!cache) {
		cache = make_uniq<CompilationCache>(context);
	}

	return cache->GetOrCompile(expr, compiler);
}

void GlobalCompilationCache::Clear() {
	lock_guard<mutex> lock(init_lock);
	if (cache) {
		cache->Clear();
	}
}

idx_t GlobalCompilationCache::GetTotalEntries() const {
	lock_guard<mutex> lock(init_lock);
	if (!cache) {
		return 0;
	}
	return cache->GetEntryCount();
}

idx_t GlobalCompilationCache::GetTotalSizeBytes() const {
	lock_guard<mutex> lock(init_lock);
	if (!cache) {
		return 0;
	}
	return cache->GetCacheSize();
}

} // namespace duckdb
