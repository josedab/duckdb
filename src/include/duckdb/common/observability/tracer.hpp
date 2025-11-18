//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/common/observability/tracer.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/common.hpp"
#include "duckdb/common/unordered_map.hpp"
#include "duckdb/common/chrono.hpp"

namespace duckdb {

class DatabaseInstance;
class ClientContext;

//! Span status
enum class SpanStatus : uint8_t {
	UNSET,
	OK,
	ERROR
};

//! A span represents a single operation within a trace
class Span {
public:
	Span(const string &name, const string &trace_id, const string &span_id, const string &parent_span_id = "");
	~Span();

	//! Set an attribute on the span
	void SetAttribute(const string &key, const string &value);
	void SetAttribute(const string &key, int64_t value);
	void SetAttribute(const string &key, double value);

	//! Add an event to the span
	void AddEvent(const string &name);

	//! Set the span status
	void SetStatus(SpanStatus status, const string &description = "");

	//! End the span (records end time)
	void End();

	//! Check if span has ended
	bool HasEnded() const {
		return ended;
	}

	//! Get span data
	const string &GetName() const {
		return name;
	}
	const string &GetTraceId() const {
		return trace_id;
	}
	const string &GetSpanId() const {
		return span_id;
	}
	const string &GetParentSpanId() const {
		return parent_span_id;
	}
	const unordered_map<string, string> &GetAttributes() const {
		return attributes;
	}
	int64_t GetStartTimeNs() const {
		return start_time_ns;
	}
	int64_t GetEndTimeNs() const {
		return end_time_ns;
	}
	SpanStatus GetStatus() const {
		return status;
	}

private:
	string name;
	string trace_id;
	string span_id;
	string parent_span_id;
	unordered_map<string, string> attributes;
	vector<pair<int64_t, string>> events; // timestamp_ns, event_name
	SpanStatus status;
	string status_description;
	int64_t start_time_ns;
	int64_t end_time_ns;
	bool ended;
};

//! Tracer for distributed tracing
class Tracer {
public:
	Tracer();
	~Tracer();

	//! Start a new span
	unique_ptr<Span> StartSpan(const string &name);

	//! Start a child span
	unique_ptr<Span> StartSpan(const string &name, const Span &parent);

	//! Context propagation - inject trace context into headers
	void InjectContext(const Span &span, unordered_map<string, string> &headers);

	//! Context propagation - extract trace context from headers
	pair<string, string> ExtractContext(const unordered_map<string, string> &headers);

	//! Set the sampling rate (0.0 to 1.0)
	void SetSampleRate(double rate);

	//! Check if tracing is enabled
	bool IsEnabled() const {
		return enabled;
	}

	//! Enable/disable tracing
	void SetEnabled(bool value) {
		enabled = value;
	}

	//! Get singleton for a database instance
	static Tracer &Get(DatabaseInstance &db);

	//! Get tracer for client context
	static Tracer &Get(ClientContext &context);

private:
	bool enabled;
	double sample_rate;

	//! Generate a random trace ID
	string GenerateTraceId();

	//! Generate a random span ID
	string GenerateSpanId();

	//! Check if this trace should be sampled
	bool ShouldSample();
};

} // namespace duckdb
