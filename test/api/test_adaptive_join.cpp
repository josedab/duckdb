#include "catch.hpp"
#include "duckdb/execution/operator/join/join_algorithm.hpp"
#include "duckdb/execution/operator/join/join_execution_state.hpp"
#include "duckdb/execution/operator/join/join_thresholds.hpp"
#include "duckdb/execution/operator/join/physical_hash_join.hpp"
#include "test_helpers.hpp"

using namespace duckdb;

TEST_CASE("JoinAlgorithm enum to string conversion", "[adaptive-join]") {
	REQUIRE(JoinAlgorithmToString(JoinAlgorithm::HASH_JOIN) == "HASH_JOIN");
	REQUIRE(JoinAlgorithmToString(JoinAlgorithm::NESTED_LOOP) == "NESTED_LOOP");
	REQUIRE(JoinAlgorithmToString(JoinAlgorithm::MERGE_JOIN) == "MERGE_JOIN");
	REQUIRE(JoinAlgorithmToString(JoinAlgorithm::INDEX_JOIN) == "INDEX_JOIN");
}

TEST_CASE("JoinExecutionState tracking", "[adaptive-join]") {
	JoinExecutionState state;

	// Initial state
	REQUIRE(state.build_rows_seen == 0);
	REQUIRE(state.probe_rows_seen == 0);
	REQUIRE(state.GetCollisionRate() == 0.0);

	// Track some rows
	state.build_rows_seen = 100;
	state.probe_rows_seen = 1000;

	// Track collisions
	state.total_lookups = 1000;
	state.collision_count = 300;

	REQUIRE(state.GetCollisionRate() == Approx(0.3));

	// Test reset
	state.Reset();
	REQUIRE(state.build_rows_seen == 0);
	REQUIRE(state.probe_rows_seen == 0);
	REQUIRE(state.GetCollisionRate() == 0.0);
}

TEST_CASE("JoinThresholds default values", "[adaptive-join]") {
	JoinThresholds thresholds;

	REQUIRE(thresholds.nested_loop_threshold == 128);
	REQUIRE(thresholds.high_collision_threshold == Approx(0.3));
	REQUIRE(thresholds.switch_threshold == Approx(1.5));
	REQUIRE(thresholds.min_rows_for_evaluation == 1000);
}

TEST_CASE("Cost estimation functions", "[adaptive-join]") {
	JoinExecutionState state;
	JoinThresholds thresholds;

	state.build_rows_seen = 1000;
	state.probe_rows_seen = 10000;
	state.total_lookups = 10000;
	state.collision_count = 0;

	// Estimate hash cost with no collisions
	double hash_cost = PhysicalHashJoin::EstimateRemainingHashCost(state, thresholds);
	REQUIRE(hash_cost > 0);

	// Add collisions - cost should increase
	state.collision_count = 3000;
	double hash_cost_with_collisions = PhysicalHashJoin::EstimateRemainingHashCost(state, thresholds);
	REQUIRE(hash_cost_with_collisions > hash_cost);

	// Estimate merge cost
	double merge_cost = PhysicalHashJoin::EstimateMergeCost(state, thresholds);
	REQUIRE(merge_cost > 0);

	// Estimate nested loop cost
	double nested_loop_cost = PhysicalHashJoin::EstimateNestedLoopCost(state, thresholds);
	REQUIRE(nested_loop_cost > 0);

	// For large tables, nested loop should be most expensive
	REQUIRE(nested_loop_cost > hash_cost);
	REQUIRE(nested_loop_cost > merge_cost);
}

TEST_CASE("Adaptive join with tiny build side", "[adaptive-join][.]") {
	DuckDB db(nullptr);
	Connection con(db);

	// Enable adaptive joins
	REQUIRE_NO_FAIL(con.Query("SET adaptive_join_enabled = true"));

	// Create tables
	REQUIRE_NO_FAIL(con.Query("CREATE TABLE big AS SELECT i FROM range(10000) t(i)"));
	REQUIRE_NO_FAIL(con.Query("CREATE TABLE tiny AS SELECT i FROM range(5) t(i)"));

	// Execute join - should work correctly regardless of algorithm
	auto result = con.Query("SELECT COUNT(*) FROM big JOIN tiny ON big.i = tiny.i");
	REQUIRE_NO_FAIL(*result);
	REQUIRE(CHECK_COLUMN(result, 0, {5}));
}

TEST_CASE("Adaptive join settings", "[adaptive-join][.]") {
	DuckDB db(nullptr);
	Connection con(db);

	// Test setting adaptive_join_enabled
	REQUIRE_NO_FAIL(con.Query("SET adaptive_join_enabled = true"));
	REQUIRE_NO_FAIL(con.Query("SET adaptive_join_enabled = false"));

	// Test setting nested_loop_threshold
	REQUIRE_NO_FAIL(con.Query("SET adaptive_join_nested_loop_threshold = 256"));
	REQUIRE_NO_FAIL(con.Query("SET adaptive_join_nested_loop_threshold = 64"));

	// Verify queries still work with different settings
	REQUIRE_NO_FAIL(con.Query("CREATE TABLE t1 AS SELECT i FROM range(100) t(i)"));
	REQUIRE_NO_FAIL(con.Query("CREATE TABLE t2 AS SELECT i FROM range(10) t(i)"));

	auto result = con.Query("SELECT COUNT(*) FROM t1 JOIN t2 ON t1.i = t2.i");
	REQUIRE_NO_FAIL(*result);
	REQUIRE(CHECK_COLUMN(result, 0, {10}));
}
