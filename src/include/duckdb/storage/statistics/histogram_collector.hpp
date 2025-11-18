//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/storage/statistics/histogram_collector.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/types/hyperloglog.hpp"
#include "duckdb/common/types/value.hpp"
#include "duckdb/common/random_engine.hpp"
#include "duckdb/storage/statistics/histogram.hpp"

namespace duckdb {
class Vector;

//! HistogramCollector collects sample values and builds equi-height histograms
class HistogramCollector {
public:
	static constexpr idx_t DEFAULT_SAMPLE_SIZE = 10000;
	static constexpr double SAMPLE_RATE = 0.1;
	static constexpr double INTEGRAL_SAMPLE_RATE = 0.3;

	explicit HistogramCollector(LogicalType type, idx_t num_buckets = EquiHeightHistogram::DEFAULT_NUM_BUCKETS,
	                            idx_t max_samples = DEFAULT_SAMPLE_SIZE);

	//! Add values from a vector to the collector
	void AddValues(Vector &vec, idx_t count);

	//! Add a single value to the collector
	void AddValue(const Value &value);

	//! Build the histogram from collected samples
	unique_ptr<EquiHeightHistogram> Build();

	//! Merge another collector into this one
	void Merge(const HistogramCollector &other);

	//! Create a copy of this collector
	unique_ptr<HistogramCollector> Copy() const;

	//! Get the number of values seen (before sampling)
	idx_t GetTotalCount() const {
		return total_count;
	}

	//! Get the number of samples collected
	idx_t GetSampleCount() const {
		return samples.size();
	}

	//! Check if type is supported for histogram collection
	static bool TypeIsSupported(const LogicalType &type);

private:
	//! Type of values being collected
	LogicalType type;
	//! Target number of buckets
	idx_t num_buckets;
	//! Maximum number of samples to collect
	idx_t max_samples;
	//! Total number of values seen
	idx_t total_count;
	//! Collected sample values
	vector<Value> samples;
	//! HyperLogLog for distinct count estimation
	unique_ptr<HyperLogLog> distinct_counter;
	//! Random engine for sampling
	RandomEngine random;

	//! Add a value to the sample using reservoir sampling
	void ReservoirSample(const Value &value);
};

} // namespace duckdb
