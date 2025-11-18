# DuckDB Repository Structure

**Commit SHA:** `52a07d06eafb63b60ed6275a494b85f93be4b986`

---

## Root Directory Overview

```
duckdb/
├── src/                    # Core source code (~415K LOC)
├── test/                   # Test suites (3,870+ files)
├── extension/              # Built-in extensions (10)
├── third_party/            # Vendored dependencies (30)
├── tools/                  # Development utilities
├── benchmark/              # Performance benchmarks
├── scripts/                # Build and CI scripts
├── examples/               # Usage examples
├── data/                   # Test data files
└── .github/                # CI/CD workflows
```

---

## Source Code Structure (`src/`)

### Core Components

| Directory | Lines | Purpose | Key Abstractions |
|-----------|-------|---------|------------------|
| `include/` | ~122K | Header files | All public interfaces |
| `function/` | ~45K | Built-in functions | ScalarFunction, AggregateFunction |
| `storage/` | ~40K | Persistence layer | DataTable, BufferManager, WAL |
| `common/` | ~35K | Shared utilities | Vector, DataChunk, Types |
| `execution/` | ~30K | Query execution | PhysicalOperator, Pipeline |
| `optimizer/` | ~25K | Query optimization | OptimizerExtension, Rules |
| `parser/` | ~20K | SQL parsing | Transformer, Statements |
| `planner/` | ~18K | Query planning | Binder, LogicalOperator |
| `catalog/` | ~12K | Metadata management | Catalog, CatalogEntry |
| `main/` | ~10K | Entry points | Database, Connection |
| `transaction/` | ~8K | MVCC transactions | Transaction, LocalStorage |
| `parallel/` | ~5K | Parallelization | Pipeline, TaskScheduler |
| `logging/` | ~2K | Query logging | QueryProfiler |
| `verification/` | ~1K | Debug validation | StatementVerifier |

### Detailed Breakdown

#### `src/include/duckdb/`
The public API and internal interfaces:

```
include/duckdb/
├── main/                   # Database, Connection, ClientContext
├── common/                 # Types, Vector, DataChunk, Allocator
├── parser/                 # ParsedExpression, SQLStatement
├── planner/                # LogicalOperator, BoundExpression
├── optimizer/              # Optimizer, OptimizerExtension
├── execution/              # PhysicalOperator, ExpressionExecutor
├── storage/                # StorageManager, DataTable, Segment
├── catalog/                # Catalog, CatalogEntry, Schema
├── transaction/            # TransactionManager, Transaction
├── function/               # Function types and registry
└── parallel/               # Pipeline, Task, Executor
```

#### `src/execution/`
Query execution engine:

```
execution/
├── operator/               # PhysicalOperator implementations
│   ├── scan/              # TableScan, IndexScan
│   ├── join/              # HashJoin, MergeJoin, NestedLoop
│   ├── aggregate/         # HashAggregate, WindowAggregate
│   ├── order/             # OrderBy, TopN
│   └── projection/        # Projection, Filter
├── expression_executor.cpp # Vectorized expression evaluation
├── executor.cpp            # Main execution coordinator
└── pipeline.cpp            # Pipeline construction
```

#### `src/storage/`
Columnar storage engine:

```
storage/
├── table/                  # DataTable, RowGroup, ColumnSegment
├── buffer/                 # BufferManager, BufferPool
├── compression/            # RLE, Dictionary, Bitpacking, ALP
├── checkpoint/             # Database checkpointing
├── wal/                    # Write-ahead logging
├── statistics/             # Column statistics
└── serialization/          # Data serialization
```

#### `src/optimizer/`
Query optimization:

```
optimizer/
├── filter_pushdown/        # Predicate pushdown
├── join_order/             # Join enumeration
├── column_lifetime/        # Dead column elimination
├── expression_rewriter/    # Expression simplification
├── statistics_propagation/ # Cardinality estimation
└── compressed_materialization/ # Compression-aware planning
```

---

## Test Structure (`test/`)

```
test/
├── sql/                    # SQLLogicTest files (3,081+)
│   ├── aggregate/         # Aggregation tests
│   ├── join/              # Join algorithm tests
│   ├── types/             # Type system tests
│   ├── function/          # Function tests
│   ├── storage/           # Persistence tests
│   └── ...                # 70+ categories
├── api/                    # C++ API tests
├── persistence/            # Storage/recovery tests
├── fuzzer/                 # Fuzz testing
│   ├── duckfuzz/          # SQL mutation fuzzer
│   ├── sqlsmith/          # Grammar-based fuzzer
│   └── pedro/             # Differential testing
└── unittest.cpp            # Test harness entry
```

---

## Extensions (`extension/`)

| Extension | Purpose | Key Features |
|-----------|---------|--------------|
| `autocomplete` | SQL completion | Tab completion in CLI |
| `core_functions` | Standard functions | Math, string, date functions |
| `icu` | Unicode support | Collation, case folding |
| `json` | JSON processing | JSON parsing and queries |
| `parquet` | Parquet I/O | Columnar file format |
| `tpch` | TPC-H benchmark | Data generation |
| `tpcds` | TPC-DS benchmark | Data generation |
| `jemalloc` | Memory allocator | Alternative allocator |
| `delta` | Delta Lake | Delta table support |
| `demo_capi` | C API demo | Extension example |

---

## Third-Party Dependencies (`third_party/`)

### Core Libraries

| Library | Version | Purpose |
|---------|---------|---------|
| `libpg_query` | - | PostgreSQL parser |
| `re2` | - | Regular expressions |
| `fmt` | 8.1.1 | String formatting |
| `utf8proc` | 2.6.1 | Unicode processing |
| `hyperloglog` | - | Cardinality estimation |

### Compression

| Library | Version | Purpose |
|---------|---------|---------|
| `zstd` | 1.5.6 | General compression |
| `lz4` | - | Fast compression |
| `miniz` | 3.0.2 | ZIP/DEFLATE |
| `snappy` | - | Fast compression |
| `fsst` | - | String compression |

### Networking/Security

| Library | Purpose |
|---------|---------|
| `mbedtls` | TLS/SSL support |
| `httplib` | HTTP client/server |
| `openssl` | Cryptography |

### Data Formats

| Library | Purpose |
|---------|---------|
| `yyjson` | JSON parsing |
| `fast_float` | Float parsing |
| `fastpforlib` | Integer compression |

---

## Build System

### Key Files

- `CMakeLists.txt` - Main build configuration (64K lines)
- `Makefile` - Development shortcuts
- `scripts/` - Build and CI utilities

### Build Targets

```bash
make                    # Debug build
make release            # Optimized build
make unit               # Run fast tests
make allunit            # Run all tests
make benchmark          # Run benchmarks
```

---

## Configuration Files

| File | Purpose |
|------|---------|
| `.clang-format` | Code formatting rules |
| `.clang-tidy` | Static analysis checks |
| `.codecov.yml` | Coverage reporting |
| `.github/workflows/` | CI/CD pipelines |
| `Doxyfile` | Documentation generation |

---

## Development Workflow

### Adding a New Function

1. Declare in `src/include/duckdb/function/`
2. Implement in `src/function/`
3. Register in catalog
4. Add tests in `test/sql/function/`

### Adding a New Operator

1. Create logical operator in `src/planner/operator/`
2. Create physical operator in `src/execution/operator/`
3. Add optimizer rules if needed
4. Add plan tests and execution tests

### Adding an Extension

1. Create directory in `extension/`
2. Implement extension interface
3. Register functions/types
4. Add to build system
