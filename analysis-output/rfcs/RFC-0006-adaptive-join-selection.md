# RFC-0006: Adaptive Join Selection

**Status:** Draft
**Author:** Claude Code Analysis
**Created:** November 18, 2025
**Commit Reference:** `52a07d06eafb63b60ed6275a494b85f93be4b986`

---

## Summary

Implement runtime-adaptive join algorithm selection that can switch from hash join to nested loop or merge join based on actual cardinalities observed during execution, rather than relying solely on optimizer estimates.

---

## Motivation

The optimizer selects join algorithms based on cardinality estimates, but these estimates can be wrong:

### Problem Scenarios

**Scenario 1: Overestimated Build Side**
```sql
-- Optimizer estimates: build=1M, probe=10M → Hash Join
-- Actual: build=100, probe=10M → Nested Loop would be better
SELECT * FROM orders o
JOIN rare_statuses s ON o.status = s.code
WHERE s.type = 'SPECIAL';  -- Very selective filter
```

**Scenario 2: Underestimated Probe Side**
```sql
-- Optimizer estimates: build=10K, probe=10K → Hash Join
-- Actual: build=10K, probe=100M → Build side too small for efficiency
SELECT * FROM small_lookup l
JOIN exploded_data e ON l.key = e.key;
```

### Current Behavior

DuckDB makes static join decisions at plan time:
- Hash Join: Default for equi-joins
- Merge Join: For already-sorted inputs
- Nested Loop: For non-equi joins

These decisions can't change based on actual runtime cardinalities.

---

## Detailed Design

### 1. Join Operator Interface

Add adaptive switching capability:

```cpp
// src/include/duckdb/execution/operator/join/physical_join.hpp
class PhysicalJoin : public PhysicalOperator {
public:
    // Check if switch is beneficial
    virtual bool ShouldSwitch(JoinExecutionState &state);

    // Switch to different algorithm
    virtual void SwitchAlgorithm(JoinAlgorithm new_algo);

    // Current algorithm
    JoinAlgorithm current_algorithm;
};

enum class JoinAlgorithm {
    HASH_JOIN,
    NESTED_LOOP,
    MERGE_JOIN,
    INDEX_JOIN
};
```

### 2. Cardinality Monitoring

Track actual cardinalities during execution:

```cpp
// src/execution/operator/join/join_execution_state.hpp
struct JoinExecutionState {
    // Actual counts
    idx_t build_rows_seen;
    idx_t probe_rows_seen;

    // Timing
    double build_time_ms;
    double probe_time_ms;

    // Hash table stats
    idx_t hash_table_size;
    double collision_rate;

    // Decision point
    bool switch_evaluated;
};
```

### 3. Adaptive Hash Join

Implement switching logic:

```cpp
// src/execution/operator/join/physical_hash_join.cpp
bool PhysicalHashJoin::ShouldSwitch(JoinExecutionState &state) {
    // After building hash table, evaluate actual size
    if (state.build_rows_seen < NESTED_LOOP_THRESHOLD) {
        // Build side tiny - nested loop would be faster
        return true;
    }

    // Check hash table efficiency
    if (state.collision_rate > HIGH_COLLISION_THRESHOLD) {
        // Poor hash distribution - consider merge join
        return true;
    }

    // Estimate remaining work
    double hash_cost = EstimateRemainingHashCost(state);
    double merge_cost = EstimateMergeCost(state);

    return merge_cost < hash_cost * SWITCH_THRESHOLD;
}

void PhysicalHashJoin::SwitchAlgorithm(JoinAlgorithm new_algo) {
    switch (new_algo) {
        case JoinAlgorithm::NESTED_LOOP:
            // Convert hash table to materialized vector
            ConvertToMaterialized();
            current_algorithm = new_algo;
            break;

        case JoinAlgorithm::MERGE_JOIN:
            // Sort build side, prepare for merge
            SortBuildSide();
            current_algorithm = new_algo;
            break;
    }
}
```

### 4. Decision Thresholds

Define when to switch:

```cpp
// src/include/duckdb/execution/operator/join/join_thresholds.hpp
struct JoinThresholds {
    // Switch to nested loop if build side smaller than this
    idx_t nested_loop_threshold = 128;

    // Switch to merge if collision rate exceeds this
    double high_collision_threshold = 0.3;

    // Only switch if improvement exceeds this multiplier
    double switch_threshold = 1.5;

    // Minimum rows before evaluating switch
    idx_t min_rows_for_evaluation = 1000;
};
```

### 5. Cost Estimation

Estimate remaining work for different algorithms:

```cpp
double PhysicalHashJoin::EstimateRemainingHashCost(JoinExecutionState &state) {
    // Probe cost: per-row hash + lookup
    double probe_cost = state.probe_rows_seen * (HASH_COST + LOOKUP_COST);

    // Adjust for collision rate
    probe_cost *= (1.0 + state.collision_rate * COLLISION_PENALTY);

    return probe_cost;
}

double PhysicalHashJoin::EstimateMergeCost(JoinExecutionState &state) {
    // Sort cost for build side
    double sort_cost = state.build_rows_seen *
                       log2(state.build_rows_seen) * COMPARE_COST;

    // Merge cost: linear scan
    double merge_cost = (state.build_rows_seen + state.probe_rows_seen) *
                        COMPARE_COST;

    return sort_cost + merge_cost;
}
```

---

## Example Scenarios

### Scenario 1: Tiny Build Side

```sql
SELECT * FROM orders o
JOIN (SELECT DISTINCT status FROM orders WHERE status LIKE 'SPECIAL%') s
ON o.status = s.status;
```

**Execution:**
1. Build hash table: 5 rows
2. Evaluate switch: 5 < 128 threshold
3. Switch to nested loop
4. Probe: Simple array scan for each probe row

**Result:** 10x faster than hash table overhead

### Scenario 2: Poor Hash Distribution

```sql
SELECT * FROM table_a a
JOIN table_b b ON a.skewed_key = b.skewed_key;
```

**Execution:**
1. Build hash table: 10K rows
2. Start probing: 50% collision rate observed
3. Evaluate switch: 0.5 > 0.3 threshold
4. Switch to merge join: Sort build side
5. Merge: Linear scan with sorted data

**Result:** 3x faster than degraded hash join

---

## Implementation Plan

### Phase 1: Infrastructure (Week 1)
- Add `JoinExecutionState` tracking
- Implement cardinality monitoring
- Unit tests for monitoring

### Phase 2: Adaptive Hash Join (Week 2)
- Implement switch decision logic
- Add hash → nested loop switch
- Add hash → merge switch
- Performance testing

### Phase 3: Tuning (Week 3)
- Benchmark threshold values
- Add configuration options
- Profiling support
- Documentation

---

## Backwards Compatibility

### Query Plans
- Plan output will show initial algorithm
- `EXPLAIN ANALYZE` will show any switches

### Performance
- Some queries may run faster (correct switches)
- Minimal overhead for switch evaluation
- Fallback: Disable with setting

### Configuration

```sql
-- Disable adaptive joins
SET adaptive_join_enabled = false;

-- Adjust thresholds
SET adaptive_join_nested_loop_threshold = 256;
```

---

## Alternatives Considered

### Alternative 1: Better Statistics
- Pro: Correct decisions at plan time
- Con: Statistics can still be wrong after filters

### Alternative 2: Multiple Plans
- Pro: No runtime switching
- Con: Memory overhead, can't react to actual data

### Alternative 3: Re-optimization
- Pro: Full optimizer re-run
- Con: High overhead, complexity

**Decision:** Runtime switching provides best reactivity with manageable complexity.

---

## Open Questions

1. **Mid-probe switching**: Allow switching after probe starts? (Proposed: Only at boundary)
2. **Statistics feedback**: Update statistics from actual cardinalities? (Proposed: Yes, optional)
3. **Parallelism**: How to switch parallel hash join? (Proposed: Per-thread decision)

---

## Success Criteria

- [ ] Automatic switch when beneficial
- [ ] < 5% overhead for switch evaluation
- [ ] 2x+ speedup for misestimated joins
- [ ] No regression for correctly estimated joins
- [ ] Profiling shows switch decisions

---

## Effort Estimation

**Total: 3 weeks (15 developer-days)**
- Infrastructure: 5 days
- Switching logic: 5 days
- Tuning and testing: 5 days

**Risk: Medium** - Core execution path changes require careful benchmarking.
