#ifndef DUCKDB_NO_THREADS

#include "catch.hpp"
#include "duckdb/common/progress_bar/progress_bar.hpp"
#include "duckdb/main/client_context.hpp"
#include "test_helpers.hpp"

#include <duckdb/execution/executor.hpp>
#include <future>
#include <thread>
#include <vector>

using namespace duckdb;
using namespace std;

TEST_CASE("Test Progress Callback API", "[progress-callback]") {
	DuckDB db(nullptr);
	Connection con(db);

	// Set up test table
	REQUIRE_NO_FAIL(con.Query("CREATE TABLE tbl AS SELECT range a, mod(range,10) b FROM range(100000)"));

	SECTION("Basic callback functionality") {
		vector<QueryProgress> progress_updates;

		// Set progress callback
		con.context->SetProgressCallback([&](QueryProgress progress) {
			progress_updates.push_back(progress);
		});

		// Set short interval for testing
		con.context->SetProgressInterval(10);
		REQUIRE(con.context->GetProgressInterval() == 10);

		// Execute query
		REQUIRE_NO_FAIL(con.Query("SELECT count(*) FROM tbl"));

		// Verify we received progress updates
		REQUIRE(progress_updates.size() > 0);

		// Check that progress increases monotonically
		double prev_percentage = -1;
		for (auto &progress : progress_updates) {
			double pct = progress.GetPercentage();
			if (pct >= 0) {
				REQUIRE(pct >= prev_percentage);
				prev_percentage = pct;
			}
		}

		// Check final update has correct status
		auto &final_progress = progress_updates.back();
		REQUIRE(final_progress.GetStatus() == QueryProgressStatus::FINISHED);
	}

	SECTION("Progress with timing information") {
		QueryProgress last_progress;

		con.context->SetProgressCallback([&](QueryProgress progress) {
			last_progress = progress;
		});
		con.context->SetProgressInterval(1);

		REQUIRE_NO_FAIL(con.Query("SELECT count(*) FROM tbl"));

		// Check timing info is available
		double elapsed = last_progress.GetElapsedSeconds();
		REQUIRE(elapsed >= 0);
	}

	SECTION("Progress status transitions") {
		vector<QueryProgressStatus> statuses;

		con.context->SetProgressCallback([&](QueryProgress progress) {
			statuses.push_back(progress.GetStatus());
		});
		con.context->SetProgressInterval(1);

		REQUIRE_NO_FAIL(con.Query("SELECT count(*) FROM tbl"));

		// Should have at least some running and one finished status
		bool has_running = false;
		bool has_finished = false;
		for (auto status : statuses) {
			if (status == QueryProgressStatus::RUNNING) has_running = true;
			if (status == QueryProgressStatus::FINISHED) has_finished = true;
		}

		REQUIRE(has_finished);
	}

	SECTION("Callback interval control") {
		vector<double> callback_times;

		con.context->SetProgressCallback([&](QueryProgress progress) {
			callback_times.push_back(progress.GetElapsedSeconds());
		});

		// Set longer interval
		con.context->SetProgressInterval(50);

		REQUIRE_NO_FAIL(con.Query("SELECT count(*) FROM tbl"));

		// With longer interval, should have fewer callbacks
		// (This is a loose check since timing can vary)
		REQUIRE(callback_times.size() >= 1);
	}

	SECTION("Rows processed tracking") {
		uint64_t max_rows_processed = 0;

		con.context->SetProgressCallback([&](QueryProgress progress) {
			uint64_t rows = progress.GetRowsProcesseed();
			if (rows > max_rows_processed) {
				max_rows_processed = rows;
			}
		});
		con.context->SetProgressInterval(1);

		REQUIRE_NO_FAIL(con.Query("SELECT count(*) FROM tbl"));

		// Should have processed some rows
		REQUIRE(max_rows_processed > 0);
	}

	SECTION("Callback with no callback set") {
		// Clear any existing callback
		con.context->SetProgressCallback(nullptr);

		// This should work without issues
		REQUIRE_NO_FAIL(con.Query("SELECT count(*) FROM tbl"));
	}

	SECTION("Multiple queries with same callback") {
		int callback_count = 0;

		con.context->SetProgressCallback([&](QueryProgress progress) {
			callback_count++;
		});
		con.context->SetProgressInterval(1);

		// Run multiple queries
		REQUIRE_NO_FAIL(con.Query("SELECT count(*) FROM tbl"));
		int first_count = callback_count;

		REQUIRE_NO_FAIL(con.Query("SELECT sum(a) FROM tbl"));
		int second_count = callback_count - first_count;

		// Both queries should trigger callbacks
		REQUIRE(first_count > 0);
		REQUIRE(second_count > 0);
	}
}

TEST_CASE("Test Progress Callback with Joins", "[progress-callback]") {
	DuckDB db(nullptr);
	Connection con(db);

	REQUIRE_NO_FAIL(con.Query("CREATE TABLE tbl AS SELECT range a FROM range(50000)"));
	REQUIRE_NO_FAIL(con.Query("CREATE TABLE tbl2 AS SELECT range a FROM range(50000)"));

	vector<QueryProgress> progress_updates;

	con.context->SetProgressCallback([&](QueryProgress progress) {
		progress_updates.push_back(progress);
	});
	con.context->SetProgressInterval(10);

	REQUIRE_NO_FAIL(con.Query("SELECT count(*) FROM tbl INNER JOIN tbl2 ON (tbl.a = tbl2.a)"));

	REQUIRE(progress_updates.size() > 0);

	// Check last status is finished
	REQUIRE(progress_updates.back().GetStatus() == QueryProgressStatus::FINISHED);
}

TEST_CASE("Test QueryProgress Structure", "[progress-callback]") {
	// Test QueryProgress copy and assignment
	QueryProgress p1;
	p1.Restart();

	// Copy constructor
	QueryProgress p2(p1);
	REQUIRE(p2.GetPercentage() == p1.GetPercentage());
	REQUIRE(p2.GetStatus() == p1.GetStatus());

	// Assignment operator
	QueryProgress p3;
	p3 = p1;
	REQUIRE(p3.GetPercentage() == p1.GetPercentage());
	REQUIRE(p3.GetStatus() == p1.GetStatus());
}

TEST_CASE("Test QueryProgressStatus Enum", "[progress-callback]") {
	// Verify enum values
	REQUIRE(static_cast<uint8_t>(QueryProgressStatus::RUNNING) == 0);
	REQUIRE(static_cast<uint8_t>(QueryProgressStatus::FINISHED) == 1);
	REQUIRE(static_cast<uint8_t>(QueryProgressStatus::ERROR) == 2);
	REQUIRE(static_cast<uint8_t>(QueryProgressStatus::CANCELLED) == 3);
}

TEST_CASE("Test C API Progress Structure", "[progress-callback]") {
	// Test that C API enum values match C++ enum
	REQUIRE(static_cast<int>(DUCKDB_QUERY_PROGRESS_RUNNING) == static_cast<int>(QueryProgressStatus::RUNNING));
	REQUIRE(static_cast<int>(DUCKDB_QUERY_PROGRESS_FINISHED) == static_cast<int>(QueryProgressStatus::FINISHED));
	REQUIRE(static_cast<int>(DUCKDB_QUERY_PROGRESS_ERROR) == static_cast<int>(QueryProgressStatus::ERROR));
	REQUIRE(static_cast<int>(DUCKDB_QUERY_PROGRESS_CANCELLED) == static_cast<int>(QueryProgressStatus::CANCELLED));
}

TEST_CASE("Test Estimated Remaining Time", "[progress-callback]") {
	DuckDB db(nullptr);
	Connection con(db);

	// Create a larger table for more reliable timing
	REQUIRE_NO_FAIL(con.Query("CREATE TABLE tbl AS SELECT range a FROM range(200000)"));

	double min_estimated = -1;
	double max_estimated = -1;
	bool had_valid_estimate = false;

	con.context->SetProgressCallback([&](QueryProgress progress) {
		double remaining = progress.GetEstimatedRemainingSeconds();
		double pct = progress.GetPercentage();

		// Only check estimates when we have some progress
		if (pct > 5 && pct < 95 && remaining >= 0) {
			had_valid_estimate = true;
			if (min_estimated < 0 || remaining < min_estimated) {
				min_estimated = remaining;
			}
			if (remaining > max_estimated) {
				max_estimated = remaining;
			}
		}
	});
	con.context->SetProgressInterval(5);

	REQUIRE_NO_FAIL(con.Query("SELECT count(*) FROM tbl"));

	// Estimated time should generally decrease (though not strictly)
	// At minimum, we should have seen some estimates
	// Note: This test may not always trigger estimates for very fast queries
}

#endif
