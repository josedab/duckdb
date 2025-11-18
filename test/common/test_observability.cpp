#include "catch.hpp"
#include "duckdb/common/observability/metrics_registry.hpp"
#include "duckdb/common/observability/tracer.hpp"
#include "duckdb/common/observability/instrumentation.hpp"

#include <string>

using namespace duckdb;
using namespace std;

TEST_CASE("MetricsRegistry counter tests", "[observability]") {
	MetricsRegistry registry;

	SECTION("Basic counter operations") {
		auto &counter = registry.RegisterCounter("test_counter", "A test counter");
		counter.Inc();
		counter.Inc(5.0);

		auto values = counter.GetValues();
		REQUIRE(values.size() == 1);
		REQUIRE(values[0].second == 6.0);
	}

	SECTION("Counter with labels") {
		auto &counter = registry.RegisterCounter("labeled_counter", "Counter with labels");

		Labels labels1;
		labels1["method"] = "GET";
		labels1["status"] = "200";
		counter.Inc(labels1, 10.0);

		Labels labels2;
		labels2["method"] = "POST";
		labels2["status"] = "201";
		counter.Inc(labels2, 5.0);

		auto values = counter.GetValues();
		REQUIRE(values.size() == 2);
	}
}

TEST_CASE("MetricsRegistry gauge tests", "[observability]") {
	MetricsRegistry registry;

	SECTION("Basic gauge operations") {
		auto &gauge = registry.RegisterGauge("test_gauge", "A test gauge");
		gauge.Set(100.0);

		auto values = gauge.GetValues();
		REQUIRE(values.size() == 1);
		REQUIRE(values[0].second == 100.0);

		gauge.Inc(10.0);
		values = gauge.GetValues();
		REQUIRE(values[0].second == 110.0);

		gauge.Dec(20.0);
		values = gauge.GetValues();
		REQUIRE(values[0].second == 90.0);
	}
}

TEST_CASE("MetricsRegistry histogram tests", "[observability]") {
	MetricsRegistry registry;

	SECTION("Basic histogram operations") {
		vector<double> buckets = {1.0, 5.0, 10.0, 50.0, 100.0};
		auto &histogram = registry.RegisterHistogram("test_histogram", "A test histogram", buckets);

		histogram.Observe(0.5);
		histogram.Observe(3.0);
		histogram.Observe(7.0);
		histogram.Observe(75.0);
		histogram.Observe(150.0);

		auto values = histogram.GetValues();
		REQUIRE(values.size() == 1);
		REQUIRE(values[0].count == 5);
		REQUIRE(values[0].sum == 235.5);
	}
}

TEST_CASE("Prometheus export format", "[observability]") {
	MetricsRegistry registry;

	auto &counter = registry.RegisterCounter("http_requests_total", "Total HTTP requests");
	counter.Inc(100.0);

	auto &gauge = registry.RegisterGauge("memory_bytes", "Memory usage");
	gauge.Set(1024.0);

	string output = registry.ExportPrometheus();

	// Check that output contains expected metric names
	REQUIRE(output.find("http_requests_total") != string::npos);
	REQUIRE(output.find("memory_bytes") != string::npos);
	REQUIRE(output.find("# HELP") != string::npos);
	REQUIRE(output.find("# TYPE") != string::npos);
	REQUIRE(output.find("counter") != string::npos);
	REQUIRE(output.find("gauge") != string::npos);
}

TEST_CASE("OpenTelemetry export format", "[observability]") {
	MetricsRegistry registry;

	auto &counter = registry.RegisterCounter("test_metric", "Test metric");
	counter.Inc(50.0);

	string output = registry.ExportOpenTelemetry();

	// Check that output is valid JSON-like structure
	REQUIRE(output.find("resourceMetrics") != string::npos);
	REQUIRE(output.find("scopeMetrics") != string::npos);
	REQUIRE(output.find("test_metric") != string::npos);
}

TEST_CASE("Tracer span tests", "[observability]") {
	Tracer tracer;
	tracer.SetEnabled(true);
	tracer.SetSampleRate(1.0);

	SECTION("Basic span creation") {
		auto span = tracer.StartSpan("test_operation");
		REQUIRE(span != nullptr);
		REQUIRE(span->GetName() == "test_operation");
		REQUIRE(!span->GetTraceId().empty());
		REQUIRE(!span->GetSpanId().empty());
	}

	SECTION("Span attributes") {
		auto span = tracer.StartSpan("test_operation");
		span->SetAttribute("key1", "value1");
		span->SetAttribute("key2", 42);
		span->SetAttribute("key3", 3.14);

		auto &attrs = span->GetAttributes();
		REQUIRE(attrs.size() == 3);
		REQUIRE(attrs.at("key1") == "value1");
	}

	SECTION("Child span") {
		auto parent = tracer.StartSpan("parent_operation");
		auto child = tracer.StartSpan("child_operation", *parent);

		REQUIRE(child->GetTraceId() == parent->GetTraceId());
		REQUIRE(child->GetParentSpanId() == parent->GetSpanId());
	}

	SECTION("Context propagation") {
		auto span = tracer.StartSpan("test_operation");

		unordered_map<string, string> headers;
		tracer.InjectContext(*span, headers);

		REQUIRE(headers.find("traceparent") != headers.end());
		REQUIRE(headers["traceparent"].find(span->GetTraceId()) != string::npos);
	}
}

TEST_CASE("Tracer sampling", "[observability]") {
	Tracer tracer;
	tracer.SetEnabled(true);

	SECTION("Zero sample rate") {
		tracer.SetSampleRate(0.0);
		auto span = tracer.StartSpan("test");
		REQUIRE(span == nullptr);
	}

	SECTION("Full sample rate") {
		tracer.SetSampleRate(1.0);
		auto span = tracer.StartSpan("test");
		REQUIRE(span != nullptr);
	}
}

TEST_CASE("Tracer disabled", "[observability]") {
	Tracer tracer;
	tracer.SetEnabled(false);

	auto span = tracer.StartSpan("test");
	REQUIRE(span == nullptr);
}

TEST_CASE("Registry get existing metrics", "[observability]") {
	MetricsRegistry registry;

	auto &counter = registry.RegisterCounter("my_counter", "Help text");
	counter.Inc(10.0);

	// Get the same counter
	auto *retrieved = registry.GetCounter("my_counter");
	REQUIRE(retrieved != nullptr);
	REQUIRE(retrieved == &counter);

	// Non-existent metric returns nullptr
	REQUIRE(registry.GetCounter("nonexistent") == nullptr);
	REQUIRE(registry.GetGauge("my_counter") == nullptr);
}

TEST_CASE("Registry reset", "[observability]") {
	MetricsRegistry registry;

	auto &counter = registry.RegisterCounter("test_counter", "Help");
	counter.Inc(100.0);

	auto &gauge = registry.RegisterGauge("test_gauge", "Help");
	gauge.Set(50.0);

	registry.Reset();

	auto counter_values = counter.GetValues();
	auto gauge_values = gauge.GetValues();

	REQUIRE(counter_values.empty());
	REQUIRE(gauge_values.empty());
}

TEST_CASE("Prometheus label escaping", "[observability]") {
	MetricsRegistry registry;

	auto &counter = registry.RegisterCounter("test_counter", "Test");

	Labels labels;
	labels["path"] = "/api/v1/users";
	labels["message"] = "Hello \"World\"";
	counter.Inc(labels, 1.0);

	string output = registry.ExportPrometheus();

	// Escaped quotes should be present
	REQUIRE(output.find("\\\"World\\\"") != string::npos);
}
