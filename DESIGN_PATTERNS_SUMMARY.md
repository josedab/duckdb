# DuckDB Performance Design Patterns - Quick Reference

## Core Files to Study

### Execution Engine
- `/src/include/duckdb/execution/executor.hpp` - Query executor with pipeline management
- `/src/execution/expression_executor.cpp` - Vectorized expression evaluation (200+ lines of key logic)
- `/src/include/duckdb/execution/physical_operator.hpp` - Base operator class with Source/Operator/Sink interfaces
- `/src/execution/physical_operator.cpp` - Operator execution logic and parallelism estimation

### Vectorized Execution
- `/src/include/duckdb/common/vector_size.hpp` - Vector size configuration (DEFAULT_STANDARD_VECTOR_SIZE = 2048)
- `/src/include/duckdb/common/types/vector.hpp` - Vector type definitions and UnifiedVectorFormat
- `/src/include/duckdb/common/types/data_chunk.hpp` - DataChunk: batch container for vectorized processing
- `/src/include/duckdb/common/types/selection_vector.hpp` - Selection vectors for zero-copy filtering

### Storage Engine
- `/src/include/duckdb/storage/data_table.hpp` - Main table interface (150+ lines of public API)
- `/src/include/duckdb/storage/table/row_group.hpp` - Row group organization (~65K rows per group)
- `/src/include/duckdb/storage/table/column_segment.hpp` - Column segment with compression
- `/src/storage/table/column_data.cpp` - Column data implementation

### Compression
- `/src/storage/compression/` - 14+ compression techniques:
  - `dictionary_compression.cpp` - Categorical/low-cardinality
  - `bitpacking.cpp` - Numeric range encoding
  - `rle.cpp` - Run-length encoding
  - `fsst.cpp` - String compression
  - `zstd.cpp` - General-purpose
  - `chimp.cpp` - Time-series optimized

### Query Optimization
- `/src/optimizer/filter_pushdown.cpp` - Push predicates to data sources
- `/src/optimizer/late_materialization.cpp` - Defer column materialization
- `/src/optimizer/column_lifetime_analyzer.cpp` - Eliminate unused columns early
- `/src/optimizer/compressed_materialization.cpp` - Keep data compressed
- `/src/optimizer/filter_combiner.cpp` - Combine multiple filters

### Parallelism
- `/src/include/duckdb/parallel/pipeline.hpp` - Pipeline DAG with work-stealing
- `/src/include/duckdb/parallel/pipeline_executor.hpp` - Per-thread pipeline execution
- `/src/parallel/executor.cpp` - Task scheduling and coordination

### Memory Management
- `/src/include/duckdb/common/allocator.hpp` - Pluggable allocator interface
- `/src/include/duckdb/storage/buffer_manager.hpp` - Buffer pooling and eviction
- `/src/storage/buffer_manager.cpp` - Memory pressure handling

### Join Operations
- `/src/include/duckdb/execution/join_hashtable.hpp` - Hash join with linear probing
- `/src/execution/join_hashtable.cpp` - Join implementation (1000+ lines)
- `/src/execution/operator/join/physical_hash_join.cpp` - Hash join operator

---

## Key Performance Metrics

### Vector Processing
- **Vector Size**: 2,048 elements (optimal cache efficiency)
- **Cache Target**: 16-64 KB per vector type
- **SIMD Alignment**: Power-of-2 for efficient vectorization

### Storage Organization
- **Row Group Size**: ~65,536 rows
- **Column Segment**: ~64 KB chunks
- **Compression Ratio**: 2-10x typical (depends on data)

### Parallelism
- **Batch Chunks**: 50 DataChunks per task before rescheduling
- **Thread Count Estimation**: `max(cardinality / (row_group_size * 2), 1)`
- **Pipeline DAG**: Dependency-based execution

### Memory
- **Memory Tagging**: Per-component tracking
- **Eviction**: LRU with temporary file spill
- **Prefetch**: Block prefetching support

---

## Critical Design Decisions Explained

### Why 2048-Element Vectors?
1. **Cache Efficiency**: ~16-64 KB typical, fits L1/L2 cache
2. **SIMD-Friendly**: Aligned, contiguous memory for auto-vectorization
3. **Batching Tradeoff**: Optimal between overhead and parallelism

### Why Column Storage?
1. **Compression**: Type-homogeneous data compresses better
2. **Selective Scanning**: Load only needed columns
3. **Late Materialization**: Keep data narrow through pipeline
4. **SIMD Operations**: Type-uniform data easier to vectorize

### Why Selection Vectors?
1. **Zero-Copy Filtering**: No memory copy during filtering
2. **Composable Filters**: Stack multiple filters on same data
3. **Memory Efficiency**: Single indirection per access
4. **Cache Friendly**: Original data remains hot

### Why Lazy Evaluation?
1. **Bandwidth**: Process compressed data directly
2. **Memory**: Avoid materialization of intermediate results
3. **CPU**: Decompression happens only for filtered results
4. **Storage**: Dictionary encoding preserved across pipeline

---

## Optimization Pipeline Order (Critical!)

1. **Filter Pushdown** - Move predicates down early
2. **Late Materialization** - Delay column widening
3. **Column Lifetime Analysis** - Drop unused columns
4. **Compressed Materialization** - Keep data compressed
5. **Common Subplan Optimization** - Materialize shared subplans once
6. **Expression Heuristics** - Reorder cheap filters first
7. **CSE Optimization** - Eliminate common subexpressions
8. **Filter Combination** - Merge predicates for better pushdown
9. **Join Elimination** - Remove unnecessary joins
10. **CTE Inlining** - Inline CTEs when beneficial
11. **Join Order** - Optimize join order
12. **Index Utilization** - Use available indexes

---

## Operator Interface Pattern

All operators implement (optionally):

```cpp
// As a data source
SourceResultType GetData(ExecutionContext &ctx, DataChunk &chunk);
bool IsSource() const;
bool ParallelSource() const;

// As a transformation
OperatorResultType Execute(ExecutionContext &ctx, DataChunk &input, 
                          DataChunk &output, GlobalOperatorState &gs);
bool ParallelOperator() const;

// As an aggregation/sink
SinkResultType Sink(ExecutionContext &ctx, DataChunk &chunk);
SinkCombineResultType Combine(ExecutionContext &ctx);
bool ParallelSink() const;
```

---

## Quick Start: Understanding Performance

### To Understand Vectorized Execution:
1. Read `vector_size.hpp` - Understand batch size
2. Read `vector.hpp` (lines 31-76) - Understand UnifiedVectorFormat
3. Read `expression_executor.cpp` - See vectorized eval

### To Understand Storage:
1. Read `row_group.hpp` - Understand hierarchy
2. Read `column_segment.hpp` - Understand compression
3. Explore `/src/storage/compression/` - See algorithms

### To Understand Optimization:
1. Read `filter_pushdown.cpp` - Filter movement
2. Read `column_lifetime_analyzer.cpp` - Column elimination
3. Read any operator in `/src/execution/operator/` - See transformations

### To Understand Parallelism:
1. Read `pipeline.hpp` - Understand DAG structure
2. Read `physical_operator.cpp` lines 56-83 - Understand thread estimation
3. Read `executor.hpp` - Understand task scheduling

---

## Performance Gains From Each Pattern

| Pattern | Benefit | Typical Gain |
|---------|---------|--------------|
| Vectorization | Amortize overhead, SIMD | 5-50x vs row-at-a-time |
| Column Storage | Selective load, compression | 2-10x storage, 5x scan speed |
| Selection Vectors | Zero-copy filtering | 2-5x filter speed |
| Lazy Decompression | Avoid materialization | 30-70% bandwidth savings |
| Pipeline Parallelism | Multicore utilization | 4-32x on 8-32 cores |
| Filter Pushdown | Early termination | 10-1000x on selective queries |
| Compression | Disk I/O reduction | 2-10x I/O speedup |
| Join Hash Table | Cache efficiency | 2-4x join speed vs naive |

---

## Key Insight

DuckDB's performance comes not from a single technique, but from **synergistic integration**:

1. **Vectorized execution** enables batching
2. **Column storage** makes selective access efficient
3. **Selection vectors** enable zero-copy filtering
4. **Lazy evaluation** keeps data compressed
5. **Compression** reduces memory footprint
6. **Pipeline parallelism** utilizes all cores
7. **Optimizer passes** reduce unnecessary work
8. **Segment statistics** enable aggressive pruning

Each layer builds on lower layers, creating multiplicative effects.

