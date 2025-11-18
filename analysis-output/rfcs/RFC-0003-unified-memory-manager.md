# RFC-0003: Unified Memory Manager

**Status:** Draft
**Author:** Claude Code Analysis
**Created:** November 18, 2025
**Commit Reference:** `52a07d06eafb63b60ed6275a494b85f93be4b986`

---

## Summary

Consolidate DuckDB's multiple memory allocation systems (Allocator, BufferManager, ArenaAllocator) into a unified memory manager with consistent tracking, limits, and observability.

---

## Motivation

DuckDB currently has multiple allocation systems:

1. **Allocator**: General-purpose allocator wrapper
2. **BufferManager**: Buffer pool with LRU eviction
3. **ArenaAllocator**: Bump allocator for temporary data
4. **Direct malloc**: Some code paths use malloc directly

This fragmentation causes issues:

### Problems

1. **Inconsistent memory tracking**: Total usage unclear
2. **Limit enforcement**: Memory limits only apply to some allocations
3. **Debugging difficulty**: Can't trace which component uses memory
4. **Spillover coordination**: BufferManager doesn't know about other allocations

### Example Issue

```cpp
// Query uses 4GB in buffer pool + 2GB in arena allocators
// Memory limit set to 5GB
// BufferManager sees 4GB, thinks it's fine
// System actually uses 6GB, OOM possible
```

---

## Detailed Design

### 1. Unified Memory Manager Interface

```cpp
// src/include/duckdb/common/memory/memory_manager.hpp
class MemoryManager {
public:
    // Allocation categories
    enum class Category {
        BUFFER_POOL,      // BufferManager blocks
        OPERATOR_STATE,   // PhysicalOperator state
        HASH_TABLE,       // Hash join, aggregation
        SORT_DATA,        // Sort buffers
        TEMPORARY,        // Arena allocations
        OTHER
    };

    // Allocate with tracking
    data_ptr_t Allocate(idx_t size, Category category);
    void Deallocate(data_ptr_t ptr, idx_t size, Category category);

    // Reallocation
    data_ptr_t Reallocate(data_ptr_t ptr, idx_t old_size, idx_t new_size,
                          Category category);

    // Memory pressure handling
    bool TryAllocate(idx_t size, Category category);
    void OnMemoryPressure();

    // Tracking
    idx_t GetUsedMemory(Category category);
    idx_t GetTotalUsedMemory();
    MemoryUsageReport GetUsageReport();

    // Limits
    void SetMemoryLimit(idx_t limit);
    void SetCategoryLimit(Category category, idx_t limit);
};
```

### 2. Memory Usage Tracking

```cpp
// src/common/memory/memory_manager.cpp
struct CategoryUsage {
    atomic<idx_t> current;
    atomic<idx_t> peak;
    idx_t limit;  // 0 = no limit
};

class MemoryManager {
private:
    array<CategoryUsage, NUM_CATEGORIES> category_usage;
    atomic<idx_t> total_usage;
    idx_t total_limit;

public:
    data_ptr_t Allocate(idx_t size, Category category) {
        // Check limits
        if (!CheckLimits(size, category)) {
            // Try to free memory
            OnMemoryPressure();
            if (!CheckLimits(size, category)) {
                throw OutOfMemoryException("...");
            }
        }

        // Allocate
        auto ptr = malloc(size);

        // Track
        category_usage[category].current += size;
        total_usage += size;
        UpdatePeak(category);

        return ptr;
    }
};
```

### 3. Memory Pressure Response

Coordinate spillover across components:

```cpp
void MemoryManager::OnMemoryPressure() {
    // 1. Evict from buffer pool (LRU)
    buffer_manager->Evict(target_bytes);

    // 2. Spill hash tables to disk
    for (auto &ht : active_hash_tables) {
        ht->SpillPartitions();
    }

    // 3. Spill sort buffers
    for (auto &sort : active_sorts) {
        sort->SpillRuns();
    }

    // 4. Notify operators of pressure
    for (auto &callback : pressure_callbacks) {
        callback();
    }
}
```

### 4. Arena Integration

Make arenas use the unified manager:

```cpp
// src/common/allocator.cpp
class ArenaAllocator {
    MemoryManager &memory_manager;

public:
    data_ptr_t Allocate(idx_t size) {
        // Use unified manager with TEMPORARY category
        return memory_manager.Allocate(size, Category::TEMPORARY);
    }

    ~ArenaAllocator() {
        // Deallocate all at once
        memory_manager.Deallocate(base, total_size, Category::TEMPORARY);
    }
};
```

### 5. Usage Reporting

Provide detailed memory breakdown:

```cpp
struct MemoryUsageReport {
    map<Category, idx_t> by_category;
    idx_t total;
    idx_t peak;
    idx_t limit;

    // Per-query breakdown
    map<string, idx_t> by_query;

    // Top consumers
    vector<pair<string, idx_t>> top_allocations;
};

// SQL interface
// SELECT * FROM duckdb_memory();
```

---

## Example Usage

### Memory Limit Enforcement

```sql
-- Set global limit
SET memory_limit = '4GB';

-- Query that would exceed limit
SELECT * FROM large_table GROUP BY complex_key;
-- Automatically spills to disk when approaching limit
```

### Memory Monitoring

```sql
-- View memory breakdown
SELECT * FROM duckdb_memory();
-- category        | current_bytes | peak_bytes | limit_bytes
-- BUFFER_POOL     | 2147483648    | 3221225472 | 4294967296
-- HASH_TABLE      | 536870912     | 1073741824 | 0
-- SORT_DATA       | 268435456     | 536870912  | 0
-- TEMPORARY       | 134217728     | 268435456  | 0
-- Total           | 3087007744    | 5100273664 | 4294967296
```

### Per-Query Tracking

```cpp
// Enable query-level tracking
context.EnableMemoryTracking();

auto result = con.Query("SELECT * FROM t GROUP BY x");

auto report = context.GetMemoryReport();
cout << "Query peak memory: " << report.peak << endl;
```

---

## Implementation Plan

### Phase 1: Core Manager (Week 1)
- Implement `MemoryManager` class
- Basic allocation tracking
- Category support
- Unit tests

### Phase 2: Integration (Week 2)
- Integrate with BufferManager
- Integrate with ArenaAllocator
- Update hash table allocations
- Update sort allocations

### Phase 3: Pressure Handling (Week 3)
- Implement coordinated spillover
- Add pressure callbacks
- Test with memory limits

### Phase 4: Observability (Week 3)
- Add SQL functions
- Implement reporting
- Documentation

---

## Backwards Compatibility

### API Changes
- New `MemoryManager` class added
- Existing `Allocator` class becomes wrapper
- `BufferManager` delegates to `MemoryManager`

### Behavior Changes
- Memory limits now apply to all allocations (stricter)
- Spillover may occur earlier (more coordinated)

### Migration
```cpp
// Old code
auto ptr = Allocator::Allocate(size);

// New code (wrapper maintains compatibility)
auto ptr = Allocator::Allocate(size);  // Works unchanged

// Or use explicit category
auto ptr = memory_manager.Allocate(size, Category::HASH_TABLE);
```

---

## Alternatives Considered

### Alternative 1: Keep Separate Systems
- Pro: No migration needed
- Con: Problems remain unsolved

### Alternative 2: Single malloc Replacement
- Pro: Complete tracking
- Con: Performance overhead, complexity

### Alternative 3: OS-Level Tracking
- Pro: Accurate
- Con: Not portable, no category info

**Decision:** Unified manager provides best balance of tracking and control.

---

## Open Questions

1. **Jemalloc integration**: Use jemalloc's tracking? (Proposed: Support both)
2. **Thread-local caching**: Keep TLS caches for performance? (Proposed: Yes)
3. **Granularity**: Track individual allocations or just totals? (Proposed: Totals + sampling)

---

## Success Criteria

- [ ] All allocations tracked through manager
- [ ] Memory limit enforced across categories
- [ ] < 3% overhead vs. current allocation
- [ ] `duckdb_memory()` function available
- [ ] Coordinated spillover working
- [ ] Documentation complete

---

## Effort Estimation

**Total: 3 weeks (15 developer-days)**
- Core manager: 5 days
- Integration: 5 days
- Pressure handling: 3 days
- Observability: 2 days

**Risk: Medium** - Wide-ranging changes require careful testing.
