# Patterns and Practices in DuckDB

*Part 3 of the DuckDB Deep Dive Series*

**Analysis based on commit:** [`52a07d06`](https://github.com/duckdb/duckdb/tree/52a07d06eafb63b60ed6275a494b85f93be4b986)

---

## What You'll Learn

- Design patterns used throughout DuckDB
- Error handling and exception hierarchy
- Memory management approach
- Testing philosophy and practices
- Code organization strategies

---

## Introduction

A codebase of 415,000 lines needs consistent patterns to remain maintainable. Without deliberate structure, codebases of this size become "big balls of mud"—difficult to understand, modify, or extend. DuckDB avoids this fate through thoughtful application of design patterns and consistent practices.

When you're reading DuckDB's code, you'll notice certain patterns appearing repeatedly. This isn't coincidence—it's intentional architecture. The same Visitor pattern that traverses ASTs also traverses logical plans and physical plans. The same Factory pattern that creates functions also creates operators and compression algorithms. This consistency means that once you understand one subsystem, you can transfer that knowledge to others.

In this post, we'll examine the patterns that make DuckDB's code both performant and maintainable—patterns that have proven their worth over millions of lines of production code in databases like PostgreSQL, MySQL, and now DuckDB.

Let's start with the design patterns, then move to error handling, memory management, and testing philosophy.

---

## Design Patterns

### The Visitor Pattern

DuckDB makes extensive use of the Visitor pattern for traversing tree structures—ASTs, logical plans, and physical plans.

```cpp
// src/include/duckdb/planner/logical_operator_visitor.hpp
class LogicalOperatorVisitor {
public:
    virtual void VisitOperator(LogicalOperator &op);
    virtual void VisitExpression(unique_ptr<Expression> *expression);

protected:
    // Override for specific operator types
    virtual void VisitReplace(LogicalAggregate &op);
    virtual void VisitReplace(LogicalFilter &op);
    // ...
};
```

**Why Visitor?**

1. **Separation of concerns**: Algorithms live separately from data structures
2. **Easy extension**: Add new operations without modifying operators
3. **Type safety**: Compiler ensures all cases are handled

**Real usage—Filter Pushdown:**

```cpp
// src/optimizer/filter_pushdown.cpp
class FilterPushdown : public LogicalOperatorVisitor {
    void VisitOperator(LogicalOperator &op) override {
        // Recursively push filters down the tree
        switch (op.type) {
            case LogicalOperatorType::FILTER:
                PushFilters(op);
                break;
            // ...
        }
    }
};
```

### The Factory Pattern

Factories create objects based on type information, essential for DuckDB's extensibility:

```cpp
// Function creation
ScalarFunction::GetFunction(type);

// Operator creation
PhysicalPlanGenerator::CreatePlan(logical_op);

// Compression selection
CompressionFunction::GetFunction(compression_type);
```

**Example—Physical Operator Creation:**

```cpp
// src/execution/physical_plan_generator.cpp
unique_ptr<PhysicalOperator> PhysicalPlanGenerator::CreatePlan(
    LogicalOperator &op) {

    switch (op.type) {
        case LogicalOperatorType::GET:
            return CreateTableScan(op.Cast<LogicalGet>());
        case LogicalOperatorType::FILTER:
            return CreateFilter(op.Cast<LogicalFilter>());
        case LogicalOperatorType::AGGREGATE:
            return CreateAggregate(op.Cast<LogicalAggregate>());
        // ... 50+ operator types
    }
}
```

### The Strategy Pattern

DuckDB uses Strategy for algorithm selection at runtime:

```cpp
// Join algorithm selection
class JoinHashTable;
class JoinMergeSort;

// Compression algorithm selection
class RLECompression;
class DictionaryCompression;
class BitpackingCompression;
```

**Example—Compression Selection:**

```cpp
// src/storage/compression/compression.cpp
unique_ptr<CompressionState> ColumnDataCompression::GetCompressionState(
    CompressionType type) {

    switch (type) {
        case CompressionType::RLE:
            return make_uniq<RLECompressionState>();
        case CompressionType::DICTIONARY:
            return make_uniq<DictionaryCompressionState>();
        case CompressionType::BITPACKING:
            return make_uniq<BitpackingCompressionState>();
        // ...
    }
}
```

The optimizer analyzes data to choose the best compression strategy for each column segment.

---

## Error Handling

DuckDB uses exceptions for error handling with a well-organized hierarchy:

```cpp
// src/include/duckdb/common/exception.hpp
class Exception : public std::exception {
public:
    ExceptionType type;
    string message;
};

// Specific exception types
class CatalogException : public Exception;      // Table not found
class ParserException : public Exception;       // Syntax error
class BinderException : public Exception;       // Semantic error
class InternalException : public Exception;     // Bug in DuckDB
class OutOfMemoryException : public Exception;  // Memory limit
class IOException : public Exception;           // File access
class ConstraintException : public Exception;   // Constraint violation
// ... 30+ exception types
```

### Why Exceptions?

1. **Error propagation**: Errors automatically bubble up the call stack
2. **Separation of concerns**: Error handling separate from business logic
3. **Context preservation**: Stack traces aid debugging

### Exception Formatting

DuckDB provides formatted exception messages with context:

```cpp
throw ParserException("Expected %s but found %s at position %d",
    expected_token, actual_token, position);

throw BinderException("Table '%s' does not exist in schema '%s'",
    table_name, schema_name);
```

### Error Location Tracking

The parser tracks source locations for helpful error messages:

```cpp
// Every AST node has location info
struct ParsedExpression {
    idx_t query_location;  // Position in original query
};

// Enables messages like:
// Error: Column "foo" not found
// SELECT foo FROM bar
//        ^^^
```

---

## Memory Management

Memory management in a database is critical—you're often working with datasets that approach or exceed available RAM. DuckDB carefully manages memory for both performance and safety, using a layered approach that provides fine-grained control while maintaining developer ergonomics.

The memory system has multiple levels: smart pointers for ownership semantics, custom allocators for tracking and limits, a buffer manager for caching and spillover, and arena allocators for temporary data. Each level serves a specific purpose.

### Smart Pointers

The codebase uses smart pointers consistently, eliminating entire classes of memory bugs:

```cpp
// Unique ownership (most common)
unique_ptr<LogicalOperator> plan;

// Shared ownership (when needed)
shared_ptr<DatabaseInstance> db;

// No raw owning pointers
```

### Custom Allocator

DuckDB tracks allocations for memory limits and debugging:

```cpp
// src/include/duckdb/common/allocator.hpp
class Allocator {
public:
    data_ptr_t Allocate(idx_t size);
    void Free(data_ptr_t pointer, idx_t size);

    // Track allocated bytes
    idx_t GetAllocatedBytes();
};
```

### Buffer Manager

Large allocations go through the BufferManager for spillover:

```cpp
// src/storage/buffer/buffer_manager.cpp
class BufferManager {
    // Pin data in memory
    BufferHandle Pin(BlockHandle &handle);

    // Release to allow eviction
    void Unpin(BlockHandle &handle);

    // Evict to disk under memory pressure
    void Evict();
};
```

This enables queries larger than available memory by spilling to disk.

### Memory Arenas

Short-lived allocations use arenas for efficiency:

```cpp
// Allocate from arena (no individual free)
ArenaAllocator arena;
auto ptr = arena.Allocate(size);

// All memory freed when arena is destroyed
// ~ArenaAllocator() frees everything at once
```

Arenas avoid allocation overhead for temporary query state.

---

## Testing Philosophy

Testing is crucial for database correctness—a bug in query execution can silently corrupt results, leading to wrong business decisions downstream. DuckDB takes testing seriously with a comprehensive approach spanning 3,870+ test files, multiple testing frameworks, and continuous fuzzing.

The philosophy is pragmatic: use the simplest testing approach that catches bugs effectively. For most SQL functionality, that means SQLLogicTest files—just SQL and expected results.

### SQLLogicTest

Most tests use the SQLLogicTest format—SQL with expected results:

```sql
# test/sql/aggregate/test_sum.test

statement ok
CREATE TABLE integers(i INTEGER);

statement ok
INSERT INTO integers VALUES (1), (2), (3);

query I
SELECT SUM(i) FROM integers;
----
6

query I
SELECT SUM(i) FROM integers WHERE i > 5;
----
NULL
```

**Benefits:**

1. **Readable**: Tests are just SQL
2. **Portable**: Same format as SQLite's test suite
3. **Complete**: Tests full query execution, not just units

### C++ Unit Tests

API and low-level tests use Catch2:

```cpp
// test/api/test_api.cpp
TEST_CASE("Basic API usage", "[api]") {
    DuckDB db;
    Connection con(db);

    auto result = con.Query("SELECT 42");
    REQUIRE(result->GetValue(0, 0) == 42);
}
```

### Fuzzing

DuckDB employs multiple fuzzing strategies:

```
test/fuzzer/
├── duckfuzz/      # SQL mutation fuzzing
├── sqlsmith/      # Grammar-based generation
├── pedro/         # Differential testing
└── afl/           # Coverage-guided fuzzing
```

The project participates in Google's OSS-Fuzz for continuous fuzzing.

### Test Organization

Tests mirror source structure:

```
test/sql/
├── aggregate/     # Aggregation functions
├── join/          # Join algorithms
├── types/         # Type system
├── function/      # Built-in functions
├── storage/       # Persistence
└── ...            # 70+ categories
```

---

## Code Organization

### File Naming

Consistent naming conventions:

```
src/
├── include/duckdb/
│   └── module/
│       └── feature.hpp       # Public interface
└── module/
    └── feature.cpp           # Implementation
```

### Include Structure

Headers are organized under `src/include/duckdb/`:

```cpp
// Public API
#include "duckdb/main/database.hpp"
#include "duckdb/main/connection.hpp"

// Internal types
#include "duckdb/common/types/vector.hpp"
#include "duckdb/execution/physical_operator.hpp"
```

### Forward Declarations

Headers minimize includes using forward declarations:

```cpp
// Forward declare instead of including
class LogicalOperator;
class PhysicalOperator;

class Optimizer {
    unique_ptr<LogicalOperator> Optimize(unique_ptr<LogicalOperator> plan);
};
```

This reduces compilation times and dependencies.

---

## Configuration Management

### Global Configuration

Database-wide settings in DBConfig:

```cpp
// src/include/duckdb/main/config.hpp
struct DBConfig {
    //! Access mode (READ_ONLY, READ_WRITE)
    AccessMode access_mode;
    //! Maximum memory to use
    idx_t maximum_memory;
    //! Number of threads
    idx_t maximum_threads;
    //! Extension directory
    string extension_directory;
};
```

### Per-Connection Configuration

Connection-specific settings in ClientConfig:

```cpp
struct ClientConfig {
    //! Enable progress bar
    bool enable_progress_bar;
    //! Query timeout
    idx_t query_timeout_ms;
    //! Maximum expression depth
    idx_t max_expression_depth;
};
```

### Setting System

Settings are modified via SQL:

```sql
SET memory_limit = '4GB';
SET threads = 8;
SET enable_progress_bar = true;
```

---

## Logging and Profiling

### Query Profiling

DuckDB provides detailed query profiling:

```sql
PRAGMA enable_profiling;
SELECT * FROM large_table WHERE x > 100;
```

Output includes:
- Operator timing breakdown
- Rows processed per operator
- Memory usage
- Pipeline structure

### Logging Framework

The logging system tracks query execution:

```cpp
// src/logging/log_manager.cpp
class LogManager {
    void Log(LogLevel level, const string &message);
};
```

---

## Key Takeaways

1. **Design patterns provide structure**: Visitor for traversal, Factory for creation, Strategy for algorithm selection

2. **Exceptions with rich context** make debugging easier than error codes

3. **Memory management is layered**: Smart pointers, custom allocator, buffer manager, arenas

4. **SQLLogicTest enables readable, comprehensive tests** that verify full query execution

5. **Code organization mirrors logical structure** with consistent naming and minimal includes

---

## Next Steps

In the next post, we'll explore DuckDB's storage engine—how data is organized on disk, compressed, and retrieved efficiently.

**Continue to Part 4: [Storage Engine Deep Dive](04-storage-engine.md)**

---

## Further Reading

- [Exception Hierarchy](https://github.com/duckdb/duckdb/blob/52a07d06eafb63b60ed6275a494b85f93be4b986/src/include/duckdb/common/exception.hpp)
- [Allocator Implementation](https://github.com/duckdb/duckdb/blob/52a07d06eafb63b60ed6275a494b85f93be4b986/src/include/duckdb/common/allocator.hpp)
- [SQLLogicTest Format](https://www.sqlite.org/sqllogictest/doc/trunk/about.wiki)
- [Contributing Guide](https://github.com/duckdb/duckdb/blob/52a07d06eafb63b60ed6275a494b85f93be4b986/CONTRIBUTING.md)
