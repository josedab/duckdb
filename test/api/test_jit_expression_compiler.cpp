#include "catch.hpp"
#include "test_helpers.hpp"
#include "duckdb/main/connection.hpp"
#include "duckdb/main/database.hpp"
#include "duckdb/execution/expression_compiler.hpp"
#include "duckdb/execution/compilation_cache.hpp"
#include "duckdb/planner/expression/bound_constant_expression.hpp"
#include "duckdb/planner/expression/bound_comparison_expression.hpp"
#include "duckdb/planner/expression/bound_reference_expression.hpp"

using namespace duckdb;
using namespace std;

TEST_CASE("Test JIT expression compiler basic API", "[api][jit]") {
	DuckDB db(nullptr);
	Connection con(db);

	// Test that JIT is disabled by default
	auto result = con.Query("SELECT current_setting('enable_jit_compilation')::boolean");
	REQUIRE_NO_FAIL(*result);
	REQUIRE(result->GetValue(0, 0) == Value::BOOLEAN(false));

	// Test enabling JIT
	REQUIRE_NO_FAIL(con.Query("SET enable_jit_compilation = true"));
	result = con.Query("SELECT current_setting('enable_jit_compilation')::boolean");
	REQUIRE_NO_FAIL(*result);
	REQUIRE(result->GetValue(0, 0) == Value::BOOLEAN(true));

	// Test setting threshold
	REQUIRE_NO_FAIL(con.Query("SET jit_compilation_threshold = 100"));
	result = con.Query("SELECT current_setting('jit_compilation_threshold')::bigint");
	REQUIRE_NO_FAIL(*result);
	REQUIRE(result->GetValue(0, 0) == Value::BIGINT(100));

	// Test JIT cache table function
	result = con.Query("SELECT * FROM duckdb_jit_cache()");
	REQUIRE_NO_FAIL(*result);
	REQUIRE(result->ColumnCount() == 8);
}

TEST_CASE("Test JIT expression compiler with queries", "[api][jit]") {
	DuckDB db(nullptr);
	Connection con(db);

	// Enable JIT compilation with low threshold
	REQUIRE_NO_FAIL(con.Query("SET enable_jit_compilation = true"));
	REQUIRE_NO_FAIL(con.Query("SET jit_compilation_threshold = 1"));

	// Create test data
	REQUIRE_NO_FAIL(con.Query("CREATE TABLE jit_test (a INTEGER, b INTEGER)"));
	REQUIRE_NO_FAIL(con.Query("INSERT INTO jit_test VALUES (1, 2), (3, 4), (5, 6)"));

	// Run expressions multiple times to trigger compilation attempts
	for (int i = 0; i < 5; i++) {
		auto result = con.Query("SELECT a + b, a * b, a - b FROM jit_test");
		REQUIRE_NO_FAIL(*result);
		REQUIRE(result->RowCount() == 3);
	}

	// Verify results are correct
	auto result = con.Query("SELECT a + b FROM jit_test ORDER BY a");
	REQUIRE_NO_FAIL(*result);
	REQUIRE(result->GetValue(0, 0) == Value::INTEGER(3));
	REQUIRE(result->GetValue(0, 1) == Value::INTEGER(7));
	REQUIRE(result->GetValue(0, 2) == Value::INTEGER(11));
}

TEST_CASE("Test JIT compilation cache table function", "[api][jit]") {
	DuckDB db(nullptr);
	Connection con(db);

	// Query cache info
	auto result = con.Query("SELECT cache_size_bytes, entry_count, jit_enabled, compilation_threshold "
	                        "FROM duckdb_jit_cache()");
	REQUIRE_NO_FAIL(*result);
	REQUIRE(result->RowCount() == 1);

	// Verify defaults
	auto jit_enabled = result->GetValue(2, 0).GetValue<bool>();
	REQUIRE(jit_enabled == false);

	auto threshold = result->GetValue(3, 0).GetValue<int64_t>();
	REQUIRE(threshold == 10000);
}

TEST_CASE("Test complex expressions with JIT", "[api][jit]") {
	DuckDB db(nullptr);
	Connection con(db);

	// Enable JIT
	REQUIRE_NO_FAIL(con.Query("SET enable_jit_compilation = true"));

	// Create table
	REQUIRE_NO_FAIL(con.Query("CREATE TABLE expr_test (x INTEGER, y INTEGER, z INTEGER)"));
	REQUIRE_NO_FAIL(con.Query("INSERT INTO expr_test VALUES (10, 20, 30)"));

	// Test complex arithmetic expression
	auto result = con.Query("SELECT (x + y) * z FROM expr_test");
	REQUIRE_NO_FAIL(*result);
	REQUIRE(result->GetValue(0, 0) == Value::INTEGER(900)); // (10 + 20) * 30 = 900

	// Test nested expressions
	result = con.Query("SELECT ((x * y) + (y * z)) / x FROM expr_test");
	REQUIRE_NO_FAIL(*result);
	// (10 * 20) + (20 * 30) = 200 + 600 = 800
	// 800 / 10 = 80
	REQUIRE(result->GetValue(0, 0) == Value::INTEGER(80));

	// Test comparison expressions
	result = con.Query("SELECT x > y, y > z, x < z FROM expr_test");
	REQUIRE_NO_FAIL(*result);
	REQUIRE(result->GetValue(0, 0) == Value::BOOLEAN(false));
	REQUIRE(result->GetValue(1, 0) == Value::BOOLEAN(false));
	REQUIRE(result->GetValue(2, 0) == Value::BOOLEAN(true));
}

TEST_CASE("Test JIT settings persistence", "[api][jit]") {
	DuckDB db(nullptr);
	Connection con(db);

	// Test default values
	auto result = con.Query("SELECT current_setting('enable_jit_compilation')::boolean, "
	                        "current_setting('jit_compilation_threshold')::bigint");
	REQUIRE_NO_FAIL(*result);
	REQUIRE(result->GetValue(0, 0) == Value::BOOLEAN(false));
	REQUIRE(result->GetValue(1, 0) == Value::BIGINT(10000));

	// Change settings
	REQUIRE_NO_FAIL(con.Query("SET enable_jit_compilation = true"));
	REQUIRE_NO_FAIL(con.Query("SET jit_compilation_threshold = 50000"));

	// Verify changes
	result = con.Query("SELECT current_setting('enable_jit_compilation')::boolean, "
	                   "current_setting('jit_compilation_threshold')::bigint");
	REQUIRE_NO_FAIL(*result);
	REQUIRE(result->GetValue(0, 0) == Value::BOOLEAN(true));
	REQUIRE(result->GetValue(1, 0) == Value::BIGINT(50000));

	// Reset and verify
	REQUIRE_NO_FAIL(con.Query("RESET enable_jit_compilation"));
	REQUIRE_NO_FAIL(con.Query("RESET jit_compilation_threshold"));

	result = con.Query("SELECT current_setting('enable_jit_compilation')::boolean, "
	                   "current_setting('jit_compilation_threshold')::bigint");
	REQUIRE_NO_FAIL(*result);
	REQUIRE(result->GetValue(0, 0) == Value::BOOLEAN(false));
	REQUIRE(result->GetValue(1, 0) == Value::BIGINT(10000));
}
