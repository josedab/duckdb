# DuckDB Testing Architecture

## Testing Hierarchy

```
DuckDB Test Infrastructure
│
├── SQL Logic Tests (3,081+ files)
│   ├── Fast Tests (1 minute runtime)
│   │   └── test/sql/{70-categories}/*.test
│   └── Slow Tests (run separately)
│       └── test/sql/{categories}/*.test_slow
│
├── C++ Unit Tests (138 files)
│   ├── API Tests (35 files)
│   │   ├── test/api/test_api.cpp
│   │   ├── test/api/test_threads.cpp
│   │   └── test/api/adbc/*.cpp
│   ├── Persistence Tests (4 files)
│   │   ├── test/persistence/test_persistence.cpp
│   │   ├── test/persistence/test_locking.cpp
│   │   └── ...
│   └── Other Tests (specialized)
│       ├── test/serialize/*.cpp
│       ├── test/parallel_csv/*.cpp
│       └── ...
│
├── Fuzzing Tests (Multi-tool)
│   ├── AFL (American Fuzzy Lop)
│   ├── DuckFuzz (Custom, 100+ regression cases)
│   ├── Pedro (SQL fuzzer)
│   ├── SQLSmith (Query generator)
│   └── OSS-Fuzz (Google continuous fuzzing)
│
├── Benchmark Tests (23+ categories)
│   ├── Micro Benchmarks (micro/)
│   ├── TPC-H (tpch/)
│   ├── TPC-DS (tpcds/)
│   ├── ClickBench (clickbench/)
│   ├── CSV/Parquet Ingestion
│   └── Domain-specific (LDBC, Taxi, IMDb)
│
├── Configuration Matrix (15+ configs)
│   ├── wal_verification
│   ├── compressed_in_memory
│   ├── storage_compatibility
│   ├── block_size variations
│   └── vector type variations
│
└── Compatibility Tests
    ├── SQLite (sqlite/)
    ├── SQL Server (sqlserver/)
    └── PostgreSQL (pg_catalog)
```

## Test Execution Pipeline

```
┌─────────────────────────────────────────┐
│  Developer runs: make unit              │
└──────────────┬──────────────────────────┘
               │
               v
┌─────────────────────────────────────────┐
│  CMake compiles test runner             │
│  (unittest executable)                  │
└──────────────┬──────────────────────────┘
               │
               v
┌─────────────────────────────────────────┐
│  unittest.cpp main()                    │
│  - Initialize TestConfiguration         │
│  - RegisterSqllogictests()              │
│  - Catch::Session().run()               │
└──────────────┬──────────────────────────┘
               │
        ┌──────┴──────┐
        v             v
   ┌─────────┐   ┌──────────┐
   │ SQL     │   │ C++      │
   │ Tests   │   │ Tests    │
   └────┬────┘   └────┬─────┘
        │             │
        v             v
   ┌─────────────────────┐
   │ Catch Framework     │
   │ - Aggregates        │
   │ - Reports           │
   │ - Failure Summary   │
   └────────┬────────────┘
            │
            v
        ┌────────────┐
        │ Pass/Fail  │
        │ Report     │
        └────────────┘
```

## CI/CD Workflow Overview

```
                    ┌─────────────────┐
                    │  PR Submitted   │
                    └────────┬────────┘
                             │
                             v
                    ┌─────────────────┐
                    │  Check Draft    │
                    │  Status         │
                    └────────┬────────┘
                             │
                    ┌────────┴────────┐
                    │                 │
              Draft=true         Draft=false
                    │                 │
                    v                 v
              ┌──────────┐      ┌──────────────┐
              │  SKIP    │      │ Run Tests    │
              │  ALL CI  │      └──────┬───────┘
              └──────────┘             │
                                       │
                    ┌──────────────────┼─────────────────┐
                    │                  │                 │
                    v                  v                 v
            ┌────────────────┐ ┌──────────────┐ ┌─────────────────┐
            │  CodeQuality   │ │   Main CI    │ │ ExtendedTests   │
            │  - Format      │ │ - Linux Dbg  │ │ - Benchmarks    │
            │  - Tidy        │ │ - Linux Rel  │ │ - Long tests    │
            │  - Enum Check  │ │ - Windows    │ │ - Extra config  │
            └────────┬───────┘ │ - macOS      │ └────────┬────────┘
                     │         │ - Android    │          │
                     │         └──────┬───────┘          │
                     │                │                  │
                     └────────┬───────┴──────────────────┘
                              │
                              v
                    ┌──────────────────┐
                    │  All Pass?       │
                    └────────┬─────────┘
                             │
                      ┌──────┴──────┐
                      v             v
                   PASS          FAIL
                      │             │
                      v             v
              ┌──────────────┐ ┌──────────────┐
              │  Merge OK    │ │  Block PR    │
              │  (if ready)  │ │  Need fixes  │
              └──────────────┘ └──────────────┘
```

## Code Quality Pipeline

```
Source Code
    │
    ├─ Check: Formatting
    │  Tool: clang-format 11.0.1
    │  Rules: 120 columns, tabs, LLVM style
    │  Auto-fix: make format-fix
    │
    ├─ Check: Generated Files
    │  Tool: git diff
    │  Purpose: Validate generated headers/enums
    │
    ├─ Check: C Enum Integrity
    │  Tool: scripts/verify_enum_integrity.py
    │  Scope: Public C API headers
    │
    ├─ Check: Static Analysis
    │  Tool: clang-tidy
    │  Checks: 100+ rules (bugprone, performance, modernize, etc)
    │  Cache: clang-tidy-cache for speed
    │  Parallel: 4 threads
    │
    └─ Build & Test
       Debug build with sanitizers:
       ├─ AddressSanitizer (ASAN)
       ├─ UndefinedBehavior (UBSAN)
       ├─ Memory (MSAN)
       ├─ Thread (TSAN)
       └─ Leak (LSAN)
```

## Test Configuration Matrix

```
Configuration                    Purpose
────────────────────────────────────────────────────────
wal_verification.json           WAL integrity checking
compressed_in_memory.json       In-memory compression
storage_compatibility.json      Version compatibility
block_size_16kB.json           Custom block sizes
no_local_filesystem.json       VFS abstraction layer
enable_verification_for_debug  Debug verifiers
force_storage.json             Always use storage
variant_vector.json            Vector type variations
compressed_materialization     Compression testing
latest_storage_*               Latest storage features
block_allocator_*              Custom allocators
in_memory_compression          Compression variants

Result: Each test runs with multiple configurations
        ensuring robustness across different setups
```

## Sanitizer Coverage Matrix

```
                AddressSanitizer  UndefinedBehavior  MemorySanitizer
              ┌──────────────────┬─────────────────┬──────────────────┐
OSS-Fuzz      │ Run 3600s        │ Run 3600s       │ Run 3600s        │
              └──────────────────┴─────────────────┴──────────────────┘
              ┌──────────────────┬─────────────────┬──────────────────┐
Main CI       │ Debug Build      │ N/A             │ N/A              │
              │ (enabled)        │                 │                  │
              └──────────────────┴─────────────────┴──────────────────┘
              ┌──────────────────┬─────────────────┬──────────────────┐
ThreadSanitizer Parallel tests  │ Race detection  │ Cross-thread UB  │
              │ test_threads.cpp│ enabled         │ enabled          │
              └──────────────────┴─────────────────┴──────────────────┘
              ┌──────────────────┬─────────────────┬──────────────────┐
LeakSanitizer │ Memory leaks     │ Known          │ Suppressions     │
              │ detected         │ suppressions   │ for safe leaks   │
              └──────────────────┴─────────────────┴──────────────────┘
```

## SQL Test Category Distribution

```
SQL Test Categories (70 total)          Estimated Test Count
───────────────────────────────────────────────────────────
Aggregate Functions                     ~150
ALTER TABLE                             ~100
Append/Insert                           ~200
Binder (SQL binding)                    ~80
Cast/Type Conversion                    ~150
Copy/Export                             ~120
CREATE (DDL)                            ~100
CTE (Common Table Expressions)          ~120
Delete                                  ~80
Error Handling                          ~50
Explain/Profiling                       ~60
Filter Operations                       ~100
Functions (built-in)                    ~400+
Generated Columns                       ~40
Index Operations                        ~100
Join Operations                         ~200+
JSON/Complex Types                      ~120
Keywords/Reserved Words                 ~60
Limit/Offset                            ~60
Merge/Upsert                            ~80
Optimizer                               ~150
Order By/Sorting                        ~100
Parser                                  ~100
Prepared Statements                     ~80
Projection                              ~80
Sample/Sampling                         ~40
Select (Core)                           ~200+
Set Operations                          ~100
Storage/Persistence                     ~150
Subqueries                              ~120
Transactions                            ~100
Types/Type System                       ~200+
UDF/User Defined Functions              ~80
Update                                  ~80
Variables/Settings                      ~60
Window Functions                        ~150
───────────────────────────────────────────────────────────
Total SQL Tests                         ~3,081+
```

## Benchmark Test Coverage

```
TPC-H (Decision Support)
├─ 22 Standard Queries
├─ Scale factors: 1, 10, 100+
└─ Covers: Joins, Aggregates, Subqueries

TPC-DS (Retail Analytics)
├─ 99 Complex Queries
├─ Multiple data generators
└─ Real-world analytical workload

ClickBench (Analytics)
├─ Query competition winners
├─ 43 Diverse analytics queries
└─ Performance benchmarking

Micro Benchmarks
├─ Nulls handling
├─ Type-specific operations
├─ Function performance
└─ Data structure operations

Ingestion Benchmarks
├─ CSV loading
├─ Parquet loading
├─ Data type performance
└─ Buffer optimization

Domain-specific
├─ NYC Taxi Dataset
├─ IMDb Dataset
├─ LDBC Graph Queries
└─ Custom workloads
```

## Test Framework Components

```
┌─────────────────────────────────────┐
│  Test Runner Infrastructure         │
├─────────────────────────────────────┤
│                                     │
│  unittest.cpp (Main Entry Point)    │
│  ├─ Test Discovery                  │
│  ├─ Configuration Loading           │
│  ├─ Environment Setup               │
│  └─ Results Aggregation             │
│                                     │
│  test_helpers.hpp (Utilities)       │
│  ├─ Database Creation/Deletion      │
│  ├─ Directory Management            │
│  ├─ CSV Comparison                  │
│  ├─ Error Handling                  │
│  └─ Assertion Macros                │
│                                     │
│  test_config.hpp (Configuration)    │
│  ├─ Test Selection                  │
│  ├─ Environment Variables           │
│  ├─ Extension Loading               │
│  ├─ Debug Settings                  │
│  └─ Verification Options            │
│                                     │
│  Catch2 Framework                   │
│  ├─ Test Registration               │
│  ├─ Test Execution                  │
│  ├─ Assertion Framework             │
│  └─ Reporting                       │
│                                     │
│  SQLLogicTest Parser                │
│  ├─ SQL Execution                   │
│  ├─ Result Validation               │
│  ├─ Error Matching                  │
│  └─ Environment Substitution        │
│                                     │
└─────────────────────────────────────┘
```

## Performance and Cache Strategy

```
Compilation Speed
├─ ccache
│  ├─ Caches intermediate objects
│  └─ Enabled on main/feature branches
└─ Conditional caching
   ├─ Based on BRANCHES_TO_BE_CACHED var
   └─ Reduces redundant compilations

Static Analysis Speed
├─ clang-tidy-cache
│  ├─ Pre-built binary
│  ├─ Incremental analysis
│  └─ 4-thread parallel execution
└─ Differential analysis
   └─ Only checks changed files

Test Execution Optimization
├─ Parallel execution (multiple configs)
├─ Fail-fast on critical tests
├─ Artifact caching
└─ Workflow concurrency groups
   └─ Cancels previous runs on new commits
```

## Extension and Plugin Testing

```
Extension Testing Infrastructure
│
├─ Extension Loading Tests
│  ├─ Dynamic library loading
│  ├─ Symbol resolution
│  └─ Initialization verification
│
├─ Extension Compatibility
│  ├─ Version checking
│  ├─ API compatibility
│  └─ Breaking change detection
│
├─ Custom Types
│  ├─ Type definition
│  ├─ Serialization
│  └─ Operator overloading
│
├─ Custom Functions
│  ├─ Scalar functions
│  ├─ Aggregate functions
│  ├─ Table functions
│  └─ Window functions
│
├─ Loadable Optimizers
│  ├─ Rule registration
│  ├─ Plan transformation
│  └─ Cost estimation
│
└─ Remote Features
    ├─ Remote optimizer
    ├─ Remote execution
    └─ Distributed queries
```

This comprehensive testing architecture ensures DuckDB maintains high code quality through multiple layers of validation, from unit tests to integration tests to fuzzing, all orchestrated through continuous integration.
