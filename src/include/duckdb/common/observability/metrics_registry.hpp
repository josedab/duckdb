//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/common/observability/metrics_registry.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/common.hpp"
#include "duckdb/common/types/value.hpp"
#include "duckdb/common/mutex.hpp"
#include "duckdb/common/unordered_map.hpp"
#include "duckdb/common/vector.hpp"

#include <atomic>

namespace duckdb {

class DatabaseInstance;

//! Label pairs for metric values
using Labels = unordered_map<string, string>;

//===----------------------------------------------------------------------===//
// Metric Types
//===----------------------------------------------------------------------===//

//! Counter - monotonically increasing metric
class MetricCounter {
public:
	MetricCounter(const string &name, const string &help);

	void Inc(double value = 1.0);
	void Inc(const Labels &labels, double value = 1.0);

	const string &GetName() const {
		return name;
	}
	const string &GetHelp() const {
		return help;
	}

	//! Get all values with their labels
	vector<pair<Labels, double>> GetValues() const;

	//! Reset all values
	void Reset();

private:
	string name;
	string help;
	mutable mutex lock;
	unordered_map<string, double> values; // serialized labels -> value
	unordered_map<string, Labels> label_map; // serialized labels -> labels

	static string SerializeLabels(const Labels &labels);
};

//! Gauge - metric that can go up and down
class MetricGauge {
public:
	MetricGauge(const string &name, const string &help);

	void Set(double value);
	void Set(const Labels &labels, double value);
	void Inc(double value = 1.0);
	void Inc(const Labels &labels, double value = 1.0);
	void Dec(double value = 1.0);
	void Dec(const Labels &labels, double value = 1.0);

	const string &GetName() const {
		return name;
	}
	const string &GetHelp() const {
		return help;
	}

	//! Get all values with their labels
	vector<pair<Labels, double>> GetValues() const;

	//! Reset all values
	void Reset();

private:
	string name;
	string help;
	mutable mutex lock;
	unordered_map<string, double> values;
	unordered_map<string, Labels> label_map;

	static string SerializeLabels(const Labels &labels);
};

//! Histogram bucket
struct HistogramBucket {
	double upper_bound;
	uint64_t count;
};

//! Histogram - distribution of observations
class MetricHistogram {
public:
	MetricHistogram(const string &name, const string &help, vector<double> buckets);

	void Observe(double value);
	void Observe(const Labels &labels, double value);

	const string &GetName() const {
		return name;
	}
	const string &GetHelp() const {
		return help;
	}
	const vector<double> &GetBuckets() const {
		return bucket_boundaries;
	}

	//! Get histogram data for a label set
	struct HistogramData {
		Labels labels;
		vector<uint64_t> bucket_counts;
		double sum;
		uint64_t count;
	};

	vector<HistogramData> GetValues() const;

	//! Reset all values
	void Reset();

private:
	string name;
	string help;
	vector<double> bucket_boundaries;
	mutable mutex lock;

	struct InternalHistogramData {
		vector<uint64_t> bucket_counts;
		double sum;
		uint64_t count;
	};

	unordered_map<string, InternalHistogramData> values;
	unordered_map<string, Labels> label_map;

	static string SerializeLabels(const Labels &labels);
};

//===----------------------------------------------------------------------===//
// Metrics Registry
//===----------------------------------------------------------------------===//

//! Central registry for all DuckDB metrics
class MetricsRegistry {
public:
	MetricsRegistry();
	~MetricsRegistry();

	//! Register metrics
	MetricCounter &RegisterCounter(const string &name, const string &help);
	MetricGauge &RegisterGauge(const string &name, const string &help);
	MetricHistogram &RegisterHistogram(const string &name, const string &help, vector<double> buckets);

	//! Get existing metrics (returns nullptr if not found)
	MetricCounter *GetCounter(const string &name);
	MetricGauge *GetGauge(const string &name);
	MetricHistogram *GetHistogram(const string &name);

	//! Export metrics
	string ExportPrometheus() const;
	string ExportOpenTelemetry() const;

	//! Reset all metrics
	void Reset();

	//! Get the singleton instance for a database
	static MetricsRegistry &Get(DatabaseInstance &db);

	//! Check if a database has a metrics registry
	static bool HasRegistry(DatabaseInstance &db);

private:
	mutable mutex lock;
	unordered_map<string, unique_ptr<MetricCounter>> counters;
	unordered_map<string, unique_ptr<MetricGauge>> gauges;
	unordered_map<string, unique_ptr<MetricHistogram>> histograms;

	//! Format labels for Prometheus output
	static string FormatLabelsPrometheus(const Labels &labels);
};

} // namespace duckdb
