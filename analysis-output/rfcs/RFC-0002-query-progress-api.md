# RFC-0002: Query Progress API

**Status:** Draft
**Author:** Claude Code Analysis
**Created:** November 18, 2025
**Commit Reference:** `52a07d06eafb63b60ed6275a494b85f93be4b986`

---

## Summary

Provide a programmatic API for monitoring query execution progress, enabling applications to display progress bars, estimated completion times, and cancel long-running queries responsively.

---

## Motivation

DuckDB has a CLI progress bar (`PRAGMA enable_progress_bar`), but there's no programmatic API for:
- Getting progress percentage
- Estimating remaining time
- Monitoring per-operator progress
- Integrating with application UIs

### Use Cases

1. **Interactive applications**: Show progress in GUI/web apps
2. **Notebook environments**: Progress bars in Jupyter
3. **ETL pipelines**: Monitor batch job progress
4. **Timeout management**: Cancel based on projected completion

---

## Detailed Design

### 1. Progress Info Structure

```cpp
// src/include/duckdb/main/query_progress.hpp
struct QueryProgress {
    // Overall progress
    double percentage;              // 0.0 - 100.0
    idx_t rows_processed;
    idx_t total_rows_estimated;

    // Timing
    double elapsed_seconds;
    double estimated_remaining_seconds;

    // Current operation
    string current_operator;
    idx_t current_operator_progress;

    // Status
    QueryProgressStatus status;     // RUNNING, FINISHED, ERROR
};

enum class QueryProgressStatus {
    RUNNING,
    FINISHED,
    ERROR,
    CANCELLED
};
```

### 2. Progress Callback API

```cpp
// src/include/duckdb/main/client_context.hpp
class ClientContext {
public:
    // Set progress callback
    using ProgressCallback = function<void(QueryProgress)>;
    void SetProgressCallback(ProgressCallback callback);

    // Set callback interval (default: 100ms)
    void SetProgressInterval(idx_t milliseconds);

    // Get current progress synchronously
    QueryProgress GetQueryProgress();
};
```

### 3. Python API

```python
# duckdb/connection.py
class Connection:
    def set_progress_callback(self, callback, interval_ms=100):
        """Set a function to be called with progress updates."""
        pass

    def query_with_progress(self, query):
        """Execute query and yield progress updates."""
        pass

# Usage example
def on_progress(progress):
    print(f"{progress.percentage:.1f}% - {progress.current_operator}")

con.set_progress_callback(on_progress)
con.execute("SELECT * FROM big_table")

# Generator style
for progress in con.query_with_progress("SELECT * FROM big_table"):
    if progress.status == 'FINISHED':
        result = progress.result
    else:
        update_ui(progress.percentage)
```

### 4. C++ Integration

```cpp
// Usage in C++
DuckDB db;
Connection con(db);

con.context->SetProgressCallback([](QueryProgress progress) {
    cout << progress.percentage << "% complete" << endl;
});

con.Query("SELECT * FROM big_table");
```

### 5. Progress Tracking Implementation

Integrate with executor to track actual progress:

```cpp
// src/execution/executor.cpp
void Executor::Execute(DataChunk &result) {
    // Track rows processed
    progress_state.rows_processed += result.size();

    // Update percentage based on cardinality estimates
    progress_state.percentage = CalculateProgress();

    // Call callback if interval elapsed
    if (ShouldCallback()) {
        InvokeCallback(progress_state);
    }
}

double Executor::CalculateProgress() {
    // Use cardinality estimates from optimizer
    if (total_estimated > 0) {
        return (double)rows_processed / total_estimated * 100.0;
    }
    // Fallback to operator-based progress
    return CalculateOperatorProgress();
}
```

### 6. Cancellation Support

Enable cancellation based on progress:

```cpp
// Cancellation from callback
con.context->SetProgressCallback([&con](QueryProgress p) {
    if (p.estimated_remaining_seconds > 300) {  // > 5 minutes
        con.Interrupt();  // Cancel query
    }
});
```

---

## Example Usage

### Interactive Progress Bar

```python
from tqdm import tqdm
import duckdb

con = duckdb.connect()

# Create progress bar
pbar = None

def on_progress(progress):
    global pbar
    if pbar is None:
        pbar = tqdm(total=100, desc="Query")
    pbar.n = progress.percentage
    pbar.refresh()

con.set_progress_callback(on_progress)
result = con.execute("SELECT * FROM read_parquet('large.parquet')").fetchdf()
pbar.close()
```

### Jupyter Notebook

```python
from IPython.display import display, HTML
import duckdb

con = duckdb.connect()
progress_widget = display(HTML(""), display_id=True)

def on_progress(progress):
    bar_width = int(progress.percentage / 2)
    html = f"""
    <div style="width: 300px; border: 1px solid #ccc;">
        <div style="width: {progress.percentage}%; background: #4CAF50; height: 20px;"></div>
    </div>
    {progress.percentage:.1f}% - {progress.current_operator}
    """
    progress_widget.update(HTML(html))

con.set_progress_callback(on_progress)
con.execute("SELECT * FROM large_table")
```

### Timeout Management

```python
import duckdb
import time

con = duckdb.connect()

def on_progress(progress):
    if progress.estimated_remaining_seconds > 60:
        # Log warning
        print(f"Query will take {progress.estimated_remaining_seconds}s")

    if progress.elapsed_seconds > 120:
        # Cancel if taking too long
        con.interrupt()

con.set_progress_callback(on_progress)
try:
    con.execute("SELECT * FROM huge_table")
except duckdb.InterruptException:
    print("Query cancelled due to timeout")
```

---

## Implementation Plan

### Phase 1: Core Progress Tracking (Day 1-2)
- Add `QueryProgress` structure
- Implement progress calculation in executor
- Unit tests

### Phase 2: Callback API (Day 2-3)
- Add callback registration to `ClientContext`
- Implement interval-based invocation
- Thread safety

### Phase 3: Language Bindings (Day 3-4)
- Python API
- Node.js API (if applicable)
- Documentation

---

## Backwards Compatibility

This is a purely additive feature:
- No changes to existing APIs
- Progress bar setting continues to work
- Default behavior unchanged (no callbacks)

---

## Alternatives Considered

### Alternative 1: Polling API Only
```cpp
while (!finished) {
    auto progress = con.GetProgress();
    sleep(100ms);
}
```
- Pro: Simpler implementation
- Con: Constant polling overhead, less responsive

### Alternative 2: Progress Events via SQL
```sql
LISTEN query_progress;
```
- Pro: Works with any client
- Con: Requires notification infrastructure

**Decision:** Callback API provides best balance of responsiveness and simplicity.

---

## Open Questions

1. **Accuracy vs. overhead**: How often to update estimates? (Proposed: 100ms)
2. **Cardinality unknowns**: How to show progress without estimates? (Proposed: operator count)
3. **Multi-statement**: Progress per statement or total? (Proposed: per statement)

---

## Success Criteria

- [ ] Progress available for queries > 1 second
- [ ] Percentage accuracy within 20% of actual
- [ ] < 1% overhead for progress tracking
- [ ] Python and C++ APIs implemented
- [ ] Documentation with examples

---

## Effort Estimation

**Total: 4 developer-days**
- Core tracking: 2 days
- Callback API: 1 day
- Python bindings: 1 day

**Risk: Low** - Additive feature with clear implementation path.
