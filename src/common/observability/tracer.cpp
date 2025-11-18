#include "duckdb/common/observability/tracer.hpp"
#include "duckdb/main/database.hpp"
#include "duckdb/main/client_context.hpp"

#include <random>
#include <iomanip>
#include <sstream>

namespace duckdb {

//===----------------------------------------------------------------------===//
// Span
//===----------------------------------------------------------------------===//

Span::Span(const string &name, const string &trace_id, const string &span_id, const string &parent_span_id)
    : name(name), trace_id(trace_id), span_id(span_id), parent_span_id(parent_span_id), status(SpanStatus::UNSET),
      ended(false) {
	// Record start time
	auto now = std::chrono::system_clock::now();
	auto duration = now.time_since_epoch();
	start_time_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
	end_time_ns = 0;
}

Span::~Span() {
	if (!ended) {
		End();
	}
}

void Span::SetAttribute(const string &key, const string &value) {
	attributes[key] = value;
}

void Span::SetAttribute(const string &key, int64_t value) {
	attributes[key] = std::to_string(value);
}

void Span::SetAttribute(const string &key, double value) {
	std::stringstream ss;
	ss << std::fixed << std::setprecision(6) << value;
	attributes[key] = ss.str();
}

void Span::AddEvent(const string &event_name) {
	auto now = std::chrono::system_clock::now();
	auto duration = now.time_since_epoch();
	int64_t timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
	events.emplace_back(timestamp_ns, event_name);
}

void Span::SetStatus(SpanStatus new_status, const string &description) {
	status = new_status;
	status_description = description;
}

void Span::End() {
	if (ended) {
		return;
	}

	auto now = std::chrono::system_clock::now();
	auto duration = now.time_since_epoch();
	end_time_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
	ended = true;
}

//===----------------------------------------------------------------------===//
// Tracer
//===----------------------------------------------------------------------===//

Tracer::Tracer() : enabled(false), sample_rate(1.0) {
}

Tracer::~Tracer() {
}

unique_ptr<Span> Tracer::StartSpan(const string &name) {
	if (!enabled || !ShouldSample()) {
		return nullptr;
	}

	string trace_id = GenerateTraceId();
	string span_id = GenerateSpanId();

	return make_uniq<Span>(name, trace_id, span_id);
}

unique_ptr<Span> Tracer::StartSpan(const string &name, const Span &parent) {
	if (!enabled || !ShouldSample()) {
		return nullptr;
	}

	string span_id = GenerateSpanId();

	return make_uniq<Span>(name, parent.GetTraceId(), span_id, parent.GetSpanId());
}

void Tracer::InjectContext(const Span &span, unordered_map<string, string> &headers) {
	// W3C Trace Context format
	// traceparent: 00-{trace-id}-{span-id}-{flags}
	string traceparent = "00-" + span.GetTraceId() + "-" + span.GetSpanId() + "-01";
	headers["traceparent"] = traceparent;
}

pair<string, string> Tracer::ExtractContext(const unordered_map<string, string> &headers) {
	auto it = headers.find("traceparent");
	if (it == headers.end()) {
		return {"", ""};
	}

	// Parse traceparent header
	// Format: 00-{trace-id}-{span-id}-{flags}
	string traceparent = it->second;
	if (traceparent.length() < 55) {
		return {"", ""};
	}

	string trace_id = traceparent.substr(3, 32);
	string parent_span_id = traceparent.substr(36, 16);

	return {trace_id, parent_span_id};
}

void Tracer::SetSampleRate(double rate) {
	if (rate < 0.0) {
		rate = 0.0;
	} else if (rate > 1.0) {
		rate = 1.0;
	}
	sample_rate = rate;
}

string Tracer::GenerateTraceId() {
	// Generate 32 hex characters (128 bits)
	static std::random_device rd;
	static std::mt19937_64 gen(rd());
	static std::uniform_int_distribution<uint64_t> dis;

	uint64_t high = dis(gen);
	uint64_t low = dis(gen);

	std::stringstream ss;
	ss << std::hex << std::setfill('0') << std::setw(16) << high << std::setw(16) << low;
	return ss.str();
}

string Tracer::GenerateSpanId() {
	// Generate 16 hex characters (64 bits)
	static std::random_device rd;
	static std::mt19937_64 gen(rd());
	static std::uniform_int_distribution<uint64_t> dis;

	uint64_t id = dis(gen);

	std::stringstream ss;
	ss << std::hex << std::setfill('0') << std::setw(16) << id;
	return ss.str();
}

bool Tracer::ShouldSample() {
	if (sample_rate >= 1.0) {
		return true;
	}
	if (sample_rate <= 0.0) {
		return false;
	}

	static std::random_device rd;
	static std::mt19937 gen(rd());
	static std::uniform_real_distribution<double> dis(0.0, 1.0);

	return dis(gen) < sample_rate;
}

Tracer &Tracer::Get(DatabaseInstance &db) {
	return *db.config.tracer;
}

Tracer &Tracer::Get(ClientContext &context) {
	return Get(DatabaseInstance::GetDatabase(context));
}

} // namespace duckdb
