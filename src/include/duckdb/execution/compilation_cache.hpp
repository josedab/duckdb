//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/execution/compilation_cache.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/common.hpp"
#include "duckdb/common/mutex.hpp"
#include "duckdb/common/unordered_map.hpp"
#include "duckdb/execution/expression_compiler.hpp"

#include <list>

namespace duckdb {

class ClientContext;

//! CompilationCacheEntry represents an entry in the compilation cache
struct CompilationCacheEntry {
	//! The compiled expression
	shared_ptr<CompiledExpression> compiled_expression;
	//! Last access time (for LRU eviction)
	idx_t last_access;
	//! Size in bytes (estimated)
	idx_t size_bytes;
};

//! CompilationCache manages compiled expressions for reuse across queries.
//! It uses an LRU eviction policy to manage memory usage.
class CompilationCache {
public:
	explicit CompilationCache(ClientContext &context);
	~CompilationCache();

	//! Get a compiled expression from cache, or compile and cache if not present
	//! @param expr The expression to get or compile
	//! @param compiler The compiler to use if compilation is needed
	//! @return The compiled expression, or nullptr if compilation failed
	shared_ptr<CompiledExpression> GetOrCompile(const Expression &expr, ExpressionCompiler &compiler);

	//! Try to get a compiled expression from cache
	//! @param hash The expression hash to look up
	//! @return The compiled expression if found, nullptr otherwise
	shared_ptr<CompiledExpression> Get(hash_t hash);

	//! Add a compiled expression to the cache
	//! @param hash The expression hash
	//! @param compiled The compiled expression
	void Put(hash_t hash, shared_ptr<CompiledExpression> compiled);

	//! Evict entries until cache is under target size
	//! @param target_size Target size in bytes
	void Evict(idx_t target_size);

	//! Clear the entire cache
	void Clear();

	//! Get current cache size in bytes
	idx_t GetCacheSize() const;

	//! Get number of entries in cache
	idx_t GetEntryCount() const;

	//! Get cache hit count
	idx_t GetHitCount() const;

	//! Get cache miss count
	idx_t GetMissCount() const;

	//! Get cache hit rate (0.0 to 1.0)
	double GetHitRate() const;

	//! Get the maximum cache size in bytes
	idx_t GetMaxCacheSize() const;

	//! Set the maximum cache size in bytes
	void SetMaxCacheSize(idx_t size);

private:
	//! Hash an expression tree
	hash_t HashExpression(const Expression &expr) const;

	//! Estimate the size of a compiled expression in bytes
	idx_t EstimateSize(const CompiledExpression &compiled) const;

	//! Update LRU order for an entry
	void TouchEntry(hash_t hash);

private:
	//! Client context
	ClientContext &context;
	//! Cache storage
	unordered_map<hash_t, CompilationCacheEntry> cache;
	//! LRU list (most recently used at front)
	std::list<hash_t> lru_list;
	//! Map from hash to LRU list position
	unordered_map<hash_t, std::list<hash_t>::iterator> lru_map;
	//! Current cache size in bytes
	idx_t current_size;
	//! Maximum cache size in bytes
	idx_t max_cache_size;
	//! Access counter for LRU
	idx_t access_counter;
	//! Cache statistics
	idx_t hit_count;
	idx_t miss_count;
	//! Mutex for thread safety
	mutable mutex cache_lock;
};

//! Global compilation cache manager for database-wide caching
class GlobalCompilationCache {
public:
	static GlobalCompilationCache &Get(DatabaseInstance &db);

	//! Get or compile an expression
	shared_ptr<CompiledExpression> GetOrCompile(const Expression &expr, ExpressionCompiler &compiler,
	                                            ClientContext &context);

	//! Clear all cached expressions
	void Clear();

	//! Get cache statistics
	idx_t GetTotalEntries() const;
	idx_t GetTotalSizeBytes() const;

private:
	GlobalCompilationCache();

	//! Database-wide compilation cache
	unique_ptr<CompilationCache> cache;
	//! Mutex for initialization
	mutex init_lock;
};

} // namespace duckdb
