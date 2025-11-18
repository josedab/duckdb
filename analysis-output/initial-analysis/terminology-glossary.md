# DuckDB Terminology Glossary

**Commit SHA:** `52a07d06eafb63b60ed6275a494b85f93be4b986`

---

## Core Concepts

### DataChunk
A batch of data containing multiple vectors, typically 2,048 rows. The fundamental unit of data transfer between operators in DuckDB's vectorized execution engine.

```cpp
// Located: src/include/duckdb/common/types/data_chunk.hpp
class DataChunk {
    vector<Vector> data;  // One Vector per column
    idx_t count;          // Number of rows (max 2048)
};
```

### Vector
A single column of data within a DataChunk. Can be stored in various formats (flat, constant, dictionary, sequence) for efficiency.

```cpp
// Located: src/include/duckdb/common/types/vector.hpp
class Vector {
    VectorType vector_type;   // FLAT, CONSTANT, DICTIONARY, etc.
    LogicalType type;         // Data type
    data_ptr_t data;          // Raw data pointer
    ValidityMask validity;    // NULL tracking
};
```

### Pipeline
A sequence of operators that can execute without materialization breaks. Pipelines are the unit of parallel execution in DuckDB.

```cpp
// Located: src/include/duckdb/parallel/pipeline.hpp
class Pipeline {
    PhysicalOperator *source;     // Data producer
    PhysicalOperator *sink;       // Data consumer
    vector<PhysicalOperator*> operators;  // Intermediate ops
};
```

### RowGroup
A horizontal partition of a table, containing ~122,880 rows (configurable). Each RowGroup stores column data in segments.

```cpp
// Located: src/include/duckdb/storage/table/row_group.hpp
class RowGroup {
    vector<shared_ptr<ColumnData>> columns;
    idx_t start;  // Starting row index
    idx_t count;  // Number of rows
};
```

### ColumnSegment
A contiguous piece of column data within a RowGroup. Can be compressed using various algorithms (RLE, Dictionary, Bitpacking, etc.).

---

## Query Processing

### Binder
The semantic analysis phase that resolves table/column names, checks types, and converts parsed SQL into bound expressions.

### LogicalOperator
A node in the logical query plan representing a relational algebra operation (Scan, Filter, Join, Aggregate, etc.).

### PhysicalOperator
A node in the physical query plan representing a specific algorithm implementation (HashJoin vs MergeJoin, etc.).

### ExpressionExecutor
Evaluates expressions over DataChunks using vectorized operations. Handles arithmetic, comparisons, function calls, etc.

### ClientContext
Per-connection state including transaction, configuration, prepared statements, and query progress tracking.

---

## Storage

### BufferManager
Manages memory allocation and disk I/O. Implements buffer pool with LRU eviction for caching database blocks.

### Block
A fixed-size unit of storage (typically 256KB). The granularity for disk I/O and buffer pool management.

### Checkpoint
The process of writing in-memory data to persistent storage. Creates a consistent snapshot of the database.

### WAL (Write-Ahead Log)
Durability mechanism that logs changes before applying them. Enables crash recovery.

### LocalStorage
Per-transaction buffer for uncommitted changes. Implements MVCC isolation without blocking readers.

---

## Types

### LogicalType
DuckDB's type system representation. Includes SQL standard types plus extensions (STRUCT, LIST, MAP, ENUM).

```cpp
// Common types
BOOLEAN, TINYINT, SMALLINT, INTEGER, BIGINT, HUGEINT
FLOAT, DOUBLE, DECIMAL
VARCHAR, BLOB
DATE, TIME, TIMESTAMP, INTERVAL
LIST, STRUCT, MAP, ENUM, UUID
```

### UnifiedVectorFormat
A normalized view of any Vector type that allows uniform access regardless of internal representation.

---

## Optimization

### Filter Pushdown
Moving filter predicates closer to data sources to reduce intermediate result sizes.

### Column Pruning
Eliminating columns that aren't needed for the query result.

### Join Ordering
Determining the optimal sequence for joining multiple tables based on cardinality estimates.

### Late Materialization
Keeping data in compressed/columnar format as long as possible, only materializing rows when necessary.

### Statistics Propagation
Passing cardinality and distribution estimates through the query plan for cost-based optimization.

---

## Execution

### Source Operator
A pipeline operator that produces data (table scan, value generator).

### Sink Operator
A pipeline operator that consumes data and may produce output to a parent pipeline (aggregation, sort, join build).

### Operator State
Per-thread execution state for an operator. Enables parallel execution without sharing mutable state.

### Work Stealing
Load balancing strategy where idle threads steal tasks from busy threads' queues.

---

## Transactions

### MVCC (Multi-Version Concurrency Control)
Concurrency model where each transaction sees a consistent snapshot. Writers don't block readers.

### Transaction ID
Unique identifier for a transaction, used for visibility checks and undo tracking.

### Commit ID
Timestamp assigned when a transaction commits, used to determine which changes are visible.

### Undo Buffer
Storage for old versions of modified data, enabling rollback and MVCC reads.

---

## Functions

### ScalarFunction
A function that operates on individual values (e.g., `UPPER`, `ABS`, `+`).

### AggregateFunction
A function that combines multiple values into one (e.g., `SUM`, `COUNT`, `AVG`).

### TableFunction
A function that produces a table result (e.g., `read_csv`, `generate_series`).

### WindowFunction
An aggregate function with an OVER clause that operates on window frames.

---

## Extensions

### Extension
A dynamically loadable module that adds functions, types, or storage formats to DuckDB.

### Catalog Entry
A metadata object in the database catalog (table, view, function, type, schema).

### Copy Function
A function that handles data import/export (CSV reader, Parquet writer).

---

## Common Abbreviations

| Abbreviation | Meaning |
|--------------|---------|
| `idx_t` | Index type (typically `uint64_t`) |
| `data_ptr_t` | Raw data pointer |
| `sel_t` | Selection vector element |
| `hash_t` | Hash value type |
| `row_t` | Row identifier type |
| `LOC` | Lines of Code |
| `AST` | Abstract Syntax Tree |
| `DDL` | Data Definition Language |
| `DML` | Data Manipulation Language |
| `OLAP` | Online Analytical Processing |
| `OLTP` | Online Transaction Processing |

---

## Design Patterns Used

### Visitor Pattern
Used for AST traversal and operator traversal. Separates algorithm from object structure.

### Factory Pattern
Used for creating operators, functions, and catalog entries based on type information.

### Strategy Pattern
Used for compression algorithms, join algorithms, and aggregation methods.

### Pull-Based Execution
Operators pull data from children (GetChunk), enabling lazy evaluation and pipelining.

### Pipeline-Based Parallelism
Work is divided into pipelines that can execute in parallel with fine-grained tasks.
