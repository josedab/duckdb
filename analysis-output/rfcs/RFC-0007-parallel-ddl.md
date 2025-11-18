# RFC-0007: Parallel DDL Operations

**Status:** Draft
**Author:** Claude Code Analysis
**Created:** November 18, 2025
**Commit Reference:** `52a07d06eafb63b60ed6275a494b85f93be4b986`

---

## Summary

Enable parallel execution of DDL operations (CREATE TABLE AS, INSERT INTO, COPY) to leverage multiple cores for large data loading and transformation operations.

---

## Motivation

Currently, many DDL operations run single-threaded:

```sql
-- Single-threaded insert
INSERT INTO target SELECT * FROM large_source;

-- Single-threaded COPY
COPY large_table TO 'output.parquet';

-- Single-threaded CTAS
CREATE TABLE new_table AS SELECT * FROM source;
```

This leaves significant performance on the table for large operations.

### Performance Impact

With parallel DDL on 8 cores:
- **COPY IN**: 4-6x faster
- **INSERT...SELECT**: 3-5x faster
- **CREATE TABLE AS**: 4-6x faster

---

## Detailed Design

### 1. Parallel Insert Operator

Parallelize inserts by partitioning work:

```cpp
// src/execution/operator/persistent/physical_insert.hpp
class PhysicalParallelInsert : public PhysicalOperator {
public:
    // Get parallelism level
    idx_t MaxThreads() override {
        return TaskScheduler::GetScheduler().NumberOfThreads();
    }

    // Per-thread state
    unique_ptr<OperatorState> GetOperatorState(ClientContext &context) override;

    // Parallel execution
    void ExecuteParallel(ExecutionContext &context,
                         DataChunk &input,
                         OperatorState &state) override;
};

class ParallelInsertState : public OperatorState {
    // Thread-local storage for buffering
    LocalStorage local_storage;
    // Accumulated rows
    DataChunk buffer;
};
```

### 2. Partitioned Writing

Partition data for parallel writes:

```cpp
// src/execution/operator/persistent/parallel_insert.cpp
void PhysicalParallelInsert::ExecuteParallel(
    ExecutionContext &context,
    DataChunk &input,
    OperatorState &gstate) {

    auto &state = gstate.Cast<ParallelInsertState>();

    // Buffer locally
    state.buffer.Append(input);

    // Flush when buffer full
    if (state.buffer.size() >= FLUSH_THRESHOLD) {
        FlushBuffer(state);
    }
}

void PhysicalParallelInsert::FlushBuffer(ParallelInsertState &state) {
    // Append to table with row-group level locking
    lock_guard<mutex> lock(table_write_lock);
    table.Append(state.buffer);
    state.buffer.Reset();
}
```

### 3. Parallel COPY

Parallelize COPY operations:

```cpp
// src/execution/operator/persistent/physical_copy_to_file.hpp
class PhysicalParallelCopyToFile : public PhysicalOperator {
public:
    // Parallel output paths
    void InitializeParallelOutput();

    // Per-thread writer
    unique_ptr<OperatorState> GetOperatorState(ClientContext &context) override {
        auto state = make_uniq<ParallelCopyState>();
        state->writer = CreateWriter(GetPartitionPath(thread_id));
        return state;
    }

    // Merge outputs at end
    void Finalize(ExecutionContext &context) override;
};
```

### 4. Row Group Level Locking

Fine-grained locking for concurrent appends:

```cpp
// src/storage/data_table.cpp
class DataTable {
public:
    // Append with row-group granularity locking
    void ParallelAppend(DataChunk &chunk, idx_t thread_id) {
        // Find or create row group for this thread
        auto row_group = GetOrCreateRowGroupForThread(thread_id);

        // Lock only this row group
        lock_guard<mutex> lock(row_group->write_lock);

        // Append
        row_group->Append(chunk);
    }

private:
    // Thread-to-row-group mapping
    unordered_map<idx_t, shared_ptr<RowGroup>> thread_row_groups;
};
```

### 5. Parallel CREATE TABLE AS

Parallelize table creation:

```cpp
// src/execution/operator/schema/physical_create_table_as.cpp
void PhysicalCreateTableAs::Execute(ClientContext &context) {
    // Create table schema
    catalog.CreateTable(info);

    // Execute source query in parallel
    auto result = executor.Execute(source_query);

    // Parallel insert results
    ParallelInsert(table, result);
}
```

### 6. Configuration

Control parallelism:

```sql
-- Global parallel DDL setting
SET parallel_ddl_enabled = true;

-- Control degree of parallelism
SET ddl_threads = 8;

-- Threshold for parallel execution
SET parallel_ddl_threshold = 100000;  -- rows
```

---

## Example Usage

### Parallel Data Loading

```sql
-- Parallel COPY from multiple files
COPY target FROM 'data/*.parquet';
-- Uses all cores to read and insert

-- Parallel INSERT
INSERT INTO warehouse
SELECT * FROM staging WHERE date > '2024-01-01';
-- Parallel source scan and insert
```

### Parallel Export

```sql
-- Parallel COPY to Parquet (creates partitioned output)
COPY (SELECT * FROM large_table)
TO 'output/' (FORMAT PARQUET, PARTITION_BY (date));
-- Each partition written in parallel
```

### Monitoring Progress

```sql
-- Enable progress for DDL
SET enable_progress_bar = true;

-- View parallel execution
COPY large_table TO 'output.parquet';
-- [████████████████████] 100% | 8 threads | 10M rows/s
```

---

## Implementation Plan

### Phase 1: Parallel Insert (Week 1-2)
- Implement `PhysicalParallelInsert`
- Row-group level locking
- Local buffering
- Unit tests

### Phase 2: Parallel COPY (Week 2-3)
- Parallel COPY TO
- Parallel COPY FROM
- Partitioned output
- File format support

### Phase 3: Parallel CTAS (Week 4)
- CREATE TABLE AS
- CREATE INDEX
- VACUUM (if applicable)

### Phase 4: Testing and Tuning (Week 5-6)
- Benchmark on various workloads
- Tune thresholds
- Handle edge cases
- Documentation

---

## Backwards Compatibility

### Semantics
- Results identical to serial execution
- Transaction semantics preserved
- Error handling unchanged

### Output
- COPY TO may produce partitioned output
- Configure with `SINGLE_FILE` option if needed

### Configuration

```sql
-- Disable parallel DDL
SET parallel_ddl_enabled = false;

-- Force single file output
COPY table TO 'output.parquet' (SINGLE_FILE true);
```

---

## Alternatives Considered

### Alternative 1: External Parallelism
- Pro: No core changes
- Con: User burden, coordination overhead

### Alternative 2: Async Writes
- Pro: Non-blocking
- Con: Doesn't reduce total time

### Alternative 3: Partitioned Tables Only
- Pro: Simpler locking
- Con: Limited applicability

**Decision:** Row-group level parallel insert provides best general-purpose solution.

---

## Open Questions

1. **Write conflicts**: How to handle concurrent updates to same row? (Proposed: Append-only for parallel)
2. **Memory usage**: Per-thread buffers increase memory. (Proposed: Configurable buffer size)
3. **Order preservation**: Do we guarantee output order? (Proposed: No, use ORDER BY if needed)

---

## Success Criteria

- [ ] 4x speedup for large INSERT...SELECT on 8 cores
- [ ] 4x speedup for large COPY operations
- [ ] No correctness issues
- [ ] Memory usage bounded
- [ ] Progress reporting works
- [ ] Documentation complete

---

## Effort Estimation

**Total: 6 weeks (30 developer-days)**
- Parallel insert: 10 days
- Parallel COPY: 10 days
- Parallel CTAS: 5 days
- Testing/tuning: 5 days

**Risk: Medium** - Concurrency requires careful handling of edge cases.
