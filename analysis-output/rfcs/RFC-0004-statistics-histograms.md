# RFC-0004: Statistics Histograms for Improved Cardinality Estimation

**Status:** Draft
**Author:** Claude Code Analysis
**Created:** November 18, 2025
**Commit Reference:** `52a07d06eafb63b60ed6275a494b85f93be4b986`

---

## Summary

Enhance DuckDB's statistics system with equi-height histograms and distinct count estimators to improve query optimizer cardinality estimates, leading to better query plans.

---

## Motivation

DuckDB currently maintains basic statistics (min, max, null count) for columns. These are insufficient for accurate cardinality estimation:

### Current Limitations

```sql
-- Column: age, min=0, max=100
-- Actual distribution: 80% between 20-40

SELECT * FROM users WHERE age > 50;
-- Estimated selectivity: 50% (uniform assumption)
-- Actual selectivity: 10%
```

### Impact

Poor estimates lead to:
1. Wrong join order (10-1000x slower)
2. Wrong join algorithm (hash vs. merge)
3. Wrong parallelism decisions
4. Memory allocation misses

---

## Detailed Design

### 1. Histogram Structure

```cpp
// src/include/duckdb/storage/statistics/histogram.hpp
struct EquiHeightHistogram {
    // Bucket boundaries
    vector<Value> bounds;
    // Each bucket contains approximately same number of rows
    idx_t rows_per_bucket;
    // Number of buckets
    idx_t num_buckets;
    // Total distinct count estimate
    idx_t distinct_count;

    // Estimate selectivity for predicate
    double EstimateSelectivity(ExpressionType op, const Value &constant);
};
```

### 2. Histogram Collection

Collect histograms during analysis:

```cpp
// src/storage/statistics/histogram_collector.cpp
class HistogramCollector {
public:
    HistogramCollector(idx_t num_buckets = 100);

    void AddValue(const Value &value);
    EquiHeightHistogram Build();

private:
    // Use reservoir sampling for large data
    ReservoirSample sample;
    HyperLogLog distinct_counter;
};

// Integrate with ANALYZE
void AnalyzeTable(DataTable &table) {
    for (auto &column : table.columns) {
        HistogramCollector collector;
        for (auto &chunk : ScanColumn(column)) {
            for (idx_t i = 0; i < chunk.size(); i++) {
                collector.AddValue(chunk.GetValue(i));
            }
        }
        column.statistics.histogram = collector.Build();
    }
}
```

### 3. Selectivity Estimation

Use histograms for predicate selectivity:

```cpp
// src/optimizer/statistics/selectivity_estimator.cpp
double EquiHeightHistogram::EstimateSelectivity(
    ExpressionType op, const Value &constant) {

    switch (op) {
        case ExpressionType::COMPARE_EQUAL: {
            // Find bucket containing value
            auto bucket = FindBucket(constant);
            // Assume uniform within bucket
            return 1.0 / distinct_count;
        }

        case ExpressionType::COMPARE_LESSTHAN: {
            // Count buckets below constant
            idx_t below = CountBucketsBelow(constant);
            // Partial bucket contribution
            double partial = PartialBucketFraction(constant);
            return (below + partial) / num_buckets;
        }

        case ExpressionType::COMPARE_BETWEEN: {
            // Range selectivity
            return EstimateLessThan(high) - EstimateLessThan(low);
        }
    }
}
```

### 4. Multi-Column Statistics

Track correlations between columns:

```cpp
// src/include/duckdb/storage/statistics/multi_column_stats.hpp
struct MultiColumnStatistics {
    // Joint distinct count
    idx_t joint_distinct;

    // Functional dependency indicator
    double dependency_strength;

    // Sample of value pairs
    vector<pair<Value, Value>> sample_pairs;
};
```

### 5. Statistics Propagation

Propagate estimates through query plan:

```cpp
// src/optimizer/statistics_propagator.cpp
void StatisticsPropagator::PropagateFilter(
    LogicalFilter &filter, Statistics &input_stats) {

    for (auto &expr : filter.expressions) {
        if (IsSimplePredicate(expr)) {
            auto &col_stats = input_stats.GetColumnStats(expr.column);
            double selectivity = col_stats.histogram.EstimateSelectivity(
                expr.type, expr.constant);
            output_stats.cardinality = input_stats.cardinality * selectivity;
        }
    }
}
```

---

## Example Usage

### Analyzing Tables

```sql
-- Collect histograms
ANALYZE users;

-- View statistics
SELECT * FROM duckdb_statistics('users');
-- column | min | max | distinct | histogram_buckets
-- age    | 0   | 100 | 50       | [0,18,25,30,35,40,50,65,80,100]
-- city   | ... | ... | 500      | ...
```

### Improved Estimates

```sql
-- Before histograms
EXPLAIN SELECT * FROM users WHERE age > 50;
-- Estimated rows: 50000 (50% of 100000)

-- After histograms
EXPLAIN SELECT * FROM users WHERE age > 50;
-- Estimated rows: 10000 (based on actual distribution)
```

### Join Order Impact

```sql
-- Complex query with multiple joins
SELECT *
FROM orders o
JOIN users u ON o.user_id = u.id
JOIN products p ON o.product_id = p.id
WHERE u.age > 50
  AND p.category = 'Electronics';

-- With histograms: Optimal join order based on actual selectivities
-- Without histograms: Potentially 10x slower due to wrong order
```

---

## Implementation Plan

### Phase 1: Basic Histograms (Week 1)
- Implement `EquiHeightHistogram` structure
- Add histogram collection in ANALYZE
- Basic selectivity estimation
- Unit tests

### Phase 2: Integration (Week 2)
- Integrate with statistics propagator
- Update optimizer to use histograms
- Storage serialization

### Phase 3: Advanced Features (Week 3)
- Multi-column statistics
- Correlation detection
- Automatic re-analysis triggers

### Phase 4: Testing and Tuning (Week 4)
- Benchmark on TPC-H/TPC-DS
- Tune bucket counts
- Documentation

---

## Backwards Compatibility

### Storage Format
- New statistics fields added
- Old databases work (no histograms, use defaults)
- Migration on first ANALYZE

### Behavior Changes
- Query plans may change (usually better)
- ANALYZE takes longer (histogram construction)

### Configuration

```sql
-- Control histogram collection
SET histogram_buckets = 100;  -- Default
SET enable_histogram_stats = true;

-- Force re-analysis
ANALYZE table RECOMPUTE;
```

---

## Alternatives Considered

### Alternative 1: Sampling-Based Estimation
- Pro: Lower storage overhead
- Con: Less accurate for skewed data

### Alternative 2: Exact Distinct Counts
- Pro: Perfect accuracy
- Con: O(n) storage, expensive to maintain

### Alternative 3: Machine Learning Estimator
- Pro: Can learn complex patterns
- Con: Training overhead, less interpretable

**Decision:** Equi-height histograms provide good accuracy/overhead tradeoff.

---

## Open Questions

1. **Bucket count**: Fixed or adaptive? (Proposed: Default 100, adaptive for large tables)
2. **Update policy**: When to rebuild? (Proposed: On significant data change or explicit ANALYZE)
3. **String histograms**: How to bucket strings? (Proposed: Prefix-based bucketing)

---

## Success Criteria

- [ ] 20% reduction in cardinality estimation error (geometric mean)
- [ ] ANALYZE completes in < 2x current time
- [ ] Storage overhead < 10% of column data
- [ ] TPC-H plan quality improvement measurable
- [ ] Documentation complete

---

## Effort Estimation

**Total: 4 weeks (20 developer-days)**
- Basic histograms: 5 days
- Integration: 5 days
- Advanced features: 5 days
- Testing/tuning: 5 days

**Risk: Low** - Well-understood technique with clear implementation path.
