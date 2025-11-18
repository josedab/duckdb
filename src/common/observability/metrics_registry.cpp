#include "duckdb/common/observability/metrics_registry.hpp"
#include "duckdb/main/database.hpp"

#include <sstream>
#include <algorithm>

namespace duckdb {

//===----------------------------------------------------------------------===//
// MetricCounter
//===----------------------------------------------------------------------===//

MetricCounter::MetricCounter(const string &name, const string &help) : name(name), help(help) {
}

string MetricCounter::SerializeLabels(const Labels &labels) {
	if (labels.empty()) {
		return "";
	}
	vector<string> parts;
	for (auto &kv : labels) {
		parts.push_back(kv.first + "=" + kv.second);
	}
	std::sort(parts.begin(), parts.end());
	string result;
	for (auto &part : parts) {
		if (!result.empty()) {
			result += ",";
		}
		result += part;
	}
	return result;
}

void MetricCounter::Inc(double value) {
	Inc(Labels(), value);
}

void MetricCounter::Inc(const Labels &labels, double value) {
	lock_guard<mutex> guard(lock);
	string key = SerializeLabels(labels);
	values[key] += value;
	if (label_map.find(key) == label_map.end()) {
		label_map[key] = labels;
	}
}

vector<pair<Labels, double>> MetricCounter::GetValues() const {
	lock_guard<mutex> guard(lock);
	vector<pair<Labels, double>> result;
	for (auto &kv : values) {
		result.emplace_back(label_map.at(kv.first), kv.second);
	}
	return result;
}

void MetricCounter::Reset() {
	lock_guard<mutex> guard(lock);
	values.clear();
	label_map.clear();
}

//===----------------------------------------------------------------------===//
// MetricGauge
//===----------------------------------------------------------------------===//

MetricGauge::MetricGauge(const string &name, const string &help) : name(name), help(help) {
}

string MetricGauge::SerializeLabels(const Labels &labels) {
	if (labels.empty()) {
		return "";
	}
	vector<string> parts;
	for (auto &kv : labels) {
		parts.push_back(kv.first + "=" + kv.second);
	}
	std::sort(parts.begin(), parts.end());
	string result;
	for (auto &part : parts) {
		if (!result.empty()) {
			result += ",";
		}
		result += part;
	}
	return result;
}

void MetricGauge::Set(double value) {
	Set(Labels(), value);
}

void MetricGauge::Set(const Labels &labels, double value) {
	lock_guard<mutex> guard(lock);
	string key = SerializeLabels(labels);
	values[key] = value;
	if (label_map.find(key) == label_map.end()) {
		label_map[key] = labels;
	}
}

void MetricGauge::Inc(double value) {
	Inc(Labels(), value);
}

void MetricGauge::Inc(const Labels &labels, double value) {
	lock_guard<mutex> guard(lock);
	string key = SerializeLabels(labels);
	values[key] += value;
	if (label_map.find(key) == label_map.end()) {
		label_map[key] = labels;
	}
}

void MetricGauge::Dec(double value) {
	Dec(Labels(), value);
}

void MetricGauge::Dec(const Labels &labels, double value) {
	lock_guard<mutex> guard(lock);
	string key = SerializeLabels(labels);
	values[key] -= value;
	if (label_map.find(key) == label_map.end()) {
		label_map[key] = labels;
	}
}

vector<pair<Labels, double>> MetricGauge::GetValues() const {
	lock_guard<mutex> guard(lock);
	vector<pair<Labels, double>> result;
	for (auto &kv : values) {
		result.emplace_back(label_map.at(kv.first), kv.second);
	}
	return result;
}

void MetricGauge::Reset() {
	lock_guard<mutex> guard(lock);
	values.clear();
	label_map.clear();
}

//===----------------------------------------------------------------------===//
// MetricHistogram
//===----------------------------------------------------------------------===//

MetricHistogram::MetricHistogram(const string &name, const string &help, vector<double> buckets)
    : name(name), help(help), bucket_boundaries(std::move(buckets)) {
	std::sort(bucket_boundaries.begin(), bucket_boundaries.end());
}

string MetricHistogram::SerializeLabels(const Labels &labels) {
	if (labels.empty()) {
		return "";
	}
	vector<string> parts;
	for (auto &kv : labels) {
		parts.push_back(kv.first + "=" + kv.second);
	}
	std::sort(parts.begin(), parts.end());
	string result;
	for (auto &part : parts) {
		if (!result.empty()) {
			result += ",";
		}
		result += part;
	}
	return result;
}

void MetricHistogram::Observe(double value) {
	Observe(Labels(), value);
}

void MetricHistogram::Observe(const Labels &labels, double value) {
	lock_guard<mutex> guard(lock);
	string key = SerializeLabels(labels);

	auto it = values.find(key);
	if (it == values.end()) {
		InternalHistogramData data;
		data.bucket_counts.resize(bucket_boundaries.size() + 1, 0); // +1 for +Inf
		data.sum = 0;
		data.count = 0;
		values[key] = data;
		label_map[key] = labels;
		it = values.find(key);
	}

	// Update histogram
	it->second.sum += value;
	it->second.count++;

	// Find bucket
	for (size_t i = 0; i < bucket_boundaries.size(); i++) {
		if (value <= bucket_boundaries[i]) {
			it->second.bucket_counts[i]++;
			break;
		}
	}
	// Always increment +Inf bucket
	it->second.bucket_counts[bucket_boundaries.size()]++;
}

vector<MetricHistogram::HistogramData> MetricHistogram::GetValues() const {
	lock_guard<mutex> guard(lock);
	vector<HistogramData> result;
	for (auto &kv : values) {
		HistogramData data;
		data.labels = label_map.at(kv.first);
		data.bucket_counts = kv.second.bucket_counts;
		data.sum = kv.second.sum;
		data.count = kv.second.count;
		result.push_back(data);
	}
	return result;
}

void MetricHistogram::Reset() {
	lock_guard<mutex> guard(lock);
	values.clear();
	label_map.clear();
}

//===----------------------------------------------------------------------===//
// MetricsRegistry
//===----------------------------------------------------------------------===//

MetricsRegistry::MetricsRegistry() {
}

MetricsRegistry::~MetricsRegistry() {
}

MetricCounter &MetricsRegistry::RegisterCounter(const string &name, const string &help) {
	lock_guard<mutex> guard(lock);
	auto it = counters.find(name);
	if (it != counters.end()) {
		return *it->second;
	}
	auto counter = make_uniq<MetricCounter>(name, help);
	auto &ref = *counter;
	counters[name] = std::move(counter);
	return ref;
}

MetricGauge &MetricsRegistry::RegisterGauge(const string &name, const string &help) {
	lock_guard<mutex> guard(lock);
	auto it = gauges.find(name);
	if (it != gauges.end()) {
		return *it->second;
	}
	auto gauge = make_uniq<MetricGauge>(name, help);
	auto &ref = *gauge;
	gauges[name] = std::move(gauge);
	return ref;
}

MetricHistogram &MetricsRegistry::RegisterHistogram(const string &name, const string &help, vector<double> buckets) {
	lock_guard<mutex> guard(lock);
	auto it = histograms.find(name);
	if (it != histograms.end()) {
		return *it->second;
	}
	auto histogram = make_uniq<MetricHistogram>(name, help, std::move(buckets));
	auto &ref = *histogram;
	histograms[name] = std::move(histogram);
	return ref;
}

MetricCounter *MetricsRegistry::GetCounter(const string &name) {
	lock_guard<mutex> guard(lock);
	auto it = counters.find(name);
	if (it == counters.end()) {
		return nullptr;
	}
	return it->second.get();
}

MetricGauge *MetricsRegistry::GetGauge(const string &name) {
	lock_guard<mutex> guard(lock);
	auto it = gauges.find(name);
	if (it == gauges.end()) {
		return nullptr;
	}
	return it->second.get();
}

MetricHistogram *MetricsRegistry::GetHistogram(const string &name) {
	lock_guard<mutex> guard(lock);
	auto it = histograms.find(name);
	if (it == histograms.end()) {
		return nullptr;
	}
	return it->second.get();
}

string MetricsRegistry::FormatLabelsPrometheus(const Labels &labels) {
	if (labels.empty()) {
		return "";
	}
	std::stringstream ss;
	ss << "{";
	bool first = true;
	for (auto &kv : labels) {
		if (!first) {
			ss << ",";
		}
		first = false;
		// Escape special characters in label values
		string escaped_value;
		for (char c : kv.second) {
			if (c == '\\') {
				escaped_value += "\\\\";
			} else if (c == '"') {
				escaped_value += "\\\"";
			} else if (c == '\n') {
				escaped_value += "\\n";
			} else {
				escaped_value += c;
			}
		}
		ss << kv.first << "=\"" << escaped_value << "\"";
	}
	ss << "}";
	return ss.str();
}

string MetricsRegistry::ExportPrometheus() const {
	lock_guard<mutex> guard(lock);
	std::stringstream ss;

	// Export counters
	for (auto &kv : counters) {
		auto &counter = *kv.second;
		ss << "# HELP " << counter.GetName() << " " << counter.GetHelp() << "\n";
		ss << "# TYPE " << counter.GetName() << " counter\n";
		auto values = counter.GetValues();
		if (values.empty()) {
			ss << counter.GetName() << " 0\n";
		} else {
			for (auto &val : values) {
				ss << counter.GetName() << FormatLabelsPrometheus(val.first) << " " << val.second << "\n";
			}
		}
	}

	// Export gauges
	for (auto &kv : gauges) {
		auto &gauge = *kv.second;
		ss << "# HELP " << gauge.GetName() << " " << gauge.GetHelp() << "\n";
		ss << "# TYPE " << gauge.GetName() << " gauge\n";
		auto values = gauge.GetValues();
		if (values.empty()) {
			ss << gauge.GetName() << " 0\n";
		} else {
			for (auto &val : values) {
				ss << gauge.GetName() << FormatLabelsPrometheus(val.first) << " " << val.second << "\n";
			}
		}
	}

	// Export histograms
	for (auto &kv : histograms) {
		auto &histogram = *kv.second;
		ss << "# HELP " << histogram.GetName() << " " << histogram.GetHelp() << "\n";
		ss << "# TYPE " << histogram.GetName() << " histogram\n";

		auto values = histogram.GetValues();
		auto &buckets = histogram.GetBuckets();

		for (auto &val : values) {
			// Cumulative bucket counts
			uint64_t cumulative = 0;
			for (size_t i = 0; i < buckets.size(); i++) {
				cumulative += val.bucket_counts[i];
				Labels bucket_labels = val.labels;
				bucket_labels["le"] = std::to_string(buckets[i]);
				ss << histogram.GetName() << "_bucket" << FormatLabelsPrometheus(bucket_labels) << " " << cumulative
				   << "\n";
			}
			// +Inf bucket
			cumulative += val.bucket_counts[buckets.size()];
			Labels inf_labels = val.labels;
			inf_labels["le"] = "+Inf";
			ss << histogram.GetName() << "_bucket" << FormatLabelsPrometheus(inf_labels) << " " << cumulative << "\n";

			// Sum and count
			ss << histogram.GetName() << "_sum" << FormatLabelsPrometheus(val.labels) << " " << val.sum << "\n";
			ss << histogram.GetName() << "_count" << FormatLabelsPrometheus(val.labels) << " " << val.count << "\n";
		}
	}

	return ss.str();
}

string MetricsRegistry::ExportOpenTelemetry() const {
	// Simplified OTLP JSON format
	lock_guard<mutex> guard(lock);
	std::stringstream ss;
	ss << "{\n";
	ss << "  \"resourceMetrics\": [{\n";
	ss << "    \"scopeMetrics\": [{\n";
	ss << "      \"metrics\": [\n";

	bool first_metric = true;

	// Export counters
	for (auto &kv : counters) {
		if (!first_metric)
			ss << ",\n";
		first_metric = false;

		auto &counter = *kv.second;
		ss << "        {\n";
		ss << "          \"name\": \"" << counter.GetName() << "\",\n";
		ss << "          \"description\": \"" << counter.GetHelp() << "\",\n";
		ss << "          \"sum\": {\n";
		ss << "            \"dataPoints\": [";

		auto values = counter.GetValues();
		bool first_val = true;
		for (auto &val : values) {
			if (!first_val)
				ss << ",";
			first_val = false;
			ss << "{\"asDouble\": " << val.second << "}";
		}
		if (values.empty()) {
			ss << "{\"asDouble\": 0}";
		}

		ss << "],\n";
		ss << "            \"isMonotonic\": true\n";
		ss << "          }\n";
		ss << "        }";
	}

	// Export gauges
	for (auto &kv : gauges) {
		if (!first_metric)
			ss << ",\n";
		first_metric = false;

		auto &gauge = *kv.second;
		ss << "        {\n";
		ss << "          \"name\": \"" << gauge.GetName() << "\",\n";
		ss << "          \"description\": \"" << gauge.GetHelp() << "\",\n";
		ss << "          \"gauge\": {\n";
		ss << "            \"dataPoints\": [";

		auto values = gauge.GetValues();
		bool first_val = true;
		for (auto &val : values) {
			if (!first_val)
				ss << ",";
			first_val = false;
			ss << "{\"asDouble\": " << val.second << "}";
		}
		if (values.empty()) {
			ss << "{\"asDouble\": 0}";
		}

		ss << "]\n";
		ss << "          }\n";
		ss << "        }";
	}

	ss << "\n      ]\n";
	ss << "    }]\n";
	ss << "  }]\n";
	ss << "}\n";

	return ss.str();
}

void MetricsRegistry::Reset() {
	lock_guard<mutex> guard(lock);
	for (auto &kv : counters) {
		kv.second->Reset();
	}
	for (auto &kv : gauges) {
		kv.second->Reset();
	}
	for (auto &kv : histograms) {
		kv.second->Reset();
	}
}

MetricsRegistry &MetricsRegistry::Get(DatabaseInstance &db) {
	return *db.config.metrics_registry;
}

bool MetricsRegistry::HasRegistry(DatabaseInstance &db) {
	return db.config.metrics_registry != nullptr;
}

} // namespace duckdb
