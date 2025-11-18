# DuckDB Testing Quick Reference

## Test Counts at a Glance
- **SQL Logic Tests:** 3,081+ files across 70 feature categories
- **C++ Unit Tests:** 138 files using Catch2 framework
- **Fuzzing Tests:** 100+ regression cases (AFL, DuckFuzz, Pedro, SQLSmith, OSS-Fuzz)
- **Benchmark Tests:** 9 files covering TPC-H, TPC-DS, ClickBench, etc.
- **Test Configurations:** 15+ parametric configurations
- **GitHub Workflows:** 35 automated CI/CD jobs

## Quick Commands
```bash
# Build and run tests
make unit              # Fast tests (~1 min)
make allunit           # All tests (~1 hour)
make unittest_release  # Release build tests

# Code quality
make format-check      # Check formatting
make format-fix        # Auto-fix formatting
make tidy-check        # Static analysis

# Individual test types
make unittestarrow     # Arrow tests only
make sqlite            # SQLite compatibility
```

## Test File Organization
```
/test/sql/             - 3,081 SQL logic tests (70 categories)
/test/api/             - 35 C++ API tests
/test/fuzzer/          - Fuzzing corpus (AFL, DuckFuzz, SQLSmith, Pedro)
/test/ossfuzz/         - Google OSS-Fuzz integration
/test/persistence/     - Crash recovery & locking tests
/test/configs/         - 15+ test configurations
/benchmark/            - 9 C++ benchmark runners (TPC-H, TPC-DS, ClickBench)
```

## Testing Frameworks
1. **SQLLogicTest** - SQL feature tests with `.test` files
   - Preferred format for SQL testing
   - 3,000+ tests covering all SQL features
   
2. **Catch2** - C++ unit testing framework
   - API tests, persistence tests, serialization tests
   - Tagged with `[tag]` notation
   
3. **Fuzzing** - Multiple fuzzer integration
   - AFL, DuckFuzz, Pedro, SQLSmith, OSS-Fuzz
   - 5 sanitizer types (ASAN, UBSAN, MSAN, TSAN, LSAN)

## Code Quality Tools
- **Clang Format 11.0.1** - Code formatting (120 column limit)
- **Clang-Tidy** - 100+ static analysis checks
- **Clang-Tidy-Cache** - Faster incremental analysis
- **AddressSanitizer** - Memory corruption detection
- **ThreadSanitizer** - Race condition detection
- **LeakSanitizer** - Memory leak detection

## Test Naming Conventions
- `test_*.test` - Fast SQL tests (run by `make unit`)
- `test_*.test_slow` - Slow SQL tests (run by `make allunit` only)
- `[tag]` - C++ test tags (e.g., `[persistence]` or `[.]` for slow)
- `[.]` at end - Marks C++ test as slow

## CI/CD Pipeline
- **Main.yml** - Core tests (every branch, PR, merge)
- **CodeQuality.yml** - Format, tidy, enum checks
- **NightlyTests.yml** - Long-running tests, packages
- **cifuzz.yml** - OSS-Fuzz with 3 sanitizer types
- **ExtendedTests.yml** - Benchmark comparisons, additional platforms

## Writing Tests

### SQL Test Example
```
# name: test/sql/example.test
# description: Test description
# group: [category]

statement ok
CREATE TABLE t (id INT)

query I
SELECT * FROM t
----
```

### C++ Test Example
```cpp
TEST_CASE("Description", "[category][.]") {
    DuckDB db(nullptr);
    Connection con(db);
    auto result = con.Query("SELECT 42");
    REQUIRE_NO_FAIL(result);
}
```

## Test Environment Variables
```
TEST_NAME              # Full test path
TEST_UUID              # Unique ID per run
WORKING_DIR            # Repository directory
BUILD_DIR              # Build output
DATA_DIR               # Test data location
TEMP_DIR               # Temporary directory
```

## Build Configuration
- **Compiler Flags:** Treat warnings as errors
- **Sanitizers:** ASAN, UBSAN, MSAN, TSAN in CI
- **Cache:** ccache + clang-tidy-cache for speed
- **Parallel:** Ninja build system supported

## Code Quality Checks Enabled
- **Clang-Tidy:** bugprone, performance, modernize, readability, google, cppcoreguidelines
- **Naming Rules:** CamelCase for classes/functions, lower_case for variables, UPPER_CASE for macros
- **Memory:** Leak suppressions for known-safe leaks (dsdgen, third-party)
- **Threading:** Thread sanitizer with suppression list
- **Coverage:** Tracked per-file with coverage-specific tests

## Key Documentation
- `test/README.md` - Test contract and environment
- `CONTRIBUTING.md` - Testing guidelines and best practices
- `https://duckdb.org/dev/testing` - Official documentation

## Common Issues & Solutions
| Issue | Solution |
|-------|----------|
| Test timeout | Mark with `.test_slow` or `[.]` tag |
| Format errors | Run `make format-fix` |
| Static analysis | Run `make tidy-check` |
| Memory leaks | Add to `.sanitizer-leak-suppressions.txt` if safe |
| Test flakiness | Increase thread limits or isolate in configs |

## Best Practices
1. Prefer SQLLogicTest for SQL features
2. Write tests before fixing bugs
3. Test error cases, not just happy path
4. Cover all code paths in fast tests
5. Run `make format-fix` before commits
6. Match code coverage for your changes
7. Document test intent with clear names
8. Use appropriate configuration for isolation needs
