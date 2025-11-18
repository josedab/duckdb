#include "duckdb/storage/statistics/histogram_collector.hpp"

#include "duckdb/common/algorithm.hpp"
#include "duckdb/common/types/vector.hpp"
#include "duckdb/common/vector_operations/vector_operations.hpp"

#include <set>

namespace duckdb {

HistogramCollector::HistogramCollector(LogicalType type_p, idx_t num_buckets_p, idx_t max_samples_p)
    : type(std::move(type_p)), num_buckets(num_buckets_p), max_samples(max_samples_p), total_count(0),
      distinct_counter(make_uniq<HyperLogLog>()), random(42) {
	samples.reserve(max_samples);
}

bool HistogramCollector::TypeIsSupported(const LogicalType &type) {
	return EquiHeightHistogram::TypeIsSupported(type);
}

void HistogramCollector::AddValue(const Value &value) {
	if (value.IsNull()) {
		total_count++;
		return;
	}

	// Reservoir sampling
	ReservoirSample(value);
	total_count++;
}

void HistogramCollector::ReservoirSample(const Value &value) {
	if (samples.size() < max_samples) {
		// Still filling the reservoir
		samples.push_back(value);
	} else {
		// Reservoir sampling: replace with probability max_samples/total_count
		idx_t j = random.NextRandomInteger(0, total_count);
		if (j < max_samples) {
			samples[j] = value;
		}
	}
}

void HistogramCollector::AddValues(Vector &vec, idx_t count) {
	if (count == 0) {
		return;
	}

	// Update distinct counter
	Vector hashes(LogicalType::HASH);
	VectorOperations::Hash(vec, hashes, count);
	distinct_counter->Update(vec, hashes, count);

	// Sample values for histogram building
	// Use sampling rate based on type
	double sample_rate = type.IsIntegral() ? INTEGRAL_SAMPLE_RATE : SAMPLE_RATE;

	UnifiedVectorFormat vdata;
	vec.ToUnifiedFormat(count, vdata);

	for (idx_t i = 0; i < count; i++) {
		idx_t idx = vdata.sel->get_index(i);
		if (!vdata.validity.RowIsValid(idx)) {
			total_count++;
			continue;
		}

		// Apply sampling
		if (random.NextRandom() > sample_rate && samples.size() >= max_samples) {
			total_count++;
			continue;
		}

		Value val = vec.GetValue(i);
		ReservoirSample(val);
		total_count++;
	}
}

unique_ptr<EquiHeightHistogram> HistogramCollector::Build() {
	if (samples.empty()) {
		return make_uniq<EquiHeightHistogram>();
	}

	// Sort samples
	std::sort(samples.begin(), samples.end(), [](const Value &a, const Value &b) {
		if (a.IsNull()) {
			return false;
		}
		if (b.IsNull()) {
			return true;
		}
		return a < b;
	});

	// Remove any null values at the end
	while (!samples.empty() && samples.back().IsNull()) {
		samples.pop_back();
	}

	if (samples.empty()) {
		return make_uniq<EquiHeightHistogram>();
	}

	// Calculate actual number of buckets (may be less if not enough samples)
	idx_t actual_buckets = std::min(num_buckets, samples.size());
	if (actual_buckets == 0) {
		actual_buckets = 1;
	}

	// Calculate bucket boundaries for equi-height histogram
	vector<Value> bounds;
	bounds.reserve(actual_buckets + 1);

	// First bound is minimum value
	bounds.push_back(samples[0]);

	// Calculate rows per bucket
	idx_t rows_per_bucket = samples.size() / actual_buckets;
	if (rows_per_bucket == 0) {
		rows_per_bucket = 1;
	}

	// Add intermediate bucket boundaries
	for (idx_t i = 1; i < actual_buckets; i++) {
		idx_t boundary_idx = i * rows_per_bucket;
		if (boundary_idx >= samples.size()) {
			boundary_idx = samples.size() - 1;
		}
		bounds.push_back(samples[boundary_idx]);
	}

	// Last bound is maximum value
	bounds.push_back(samples[samples.size() - 1]);

	// Get distinct count from HyperLogLog
	idx_t distinct_count = distinct_counter->Count();
	if (distinct_count == 0 && !samples.empty()) {
		// Estimate from samples if HLL returns 0
		std::set<Value> unique_samples(samples.begin(), samples.end());
		distinct_count = unique_samples.size();
	}

	// Scale distinct count by sampling ratio
	if (total_count > 0 && samples.size() < total_count) {
		double ratio = static_cast<double>(total_count) / static_cast<double>(samples.size());
		// Use Good-Turing estimation similar to DistinctStatistics
		distinct_count =
		    static_cast<idx_t>(std::min(static_cast<double>(distinct_count) * std::sqrt(ratio), static_cast<double>(total_count)));
	}

	return make_uniq<EquiHeightHistogram>(type, std::move(bounds), rows_per_bucket, distinct_count);
}

void HistogramCollector::Merge(const HistogramCollector &other) {
	// Merge HLL counters
	distinct_counter->Merge(*other.distinct_counter);

	// Merge samples using reservoir sampling merge
	total_count += other.total_count;

	// Simple merge strategy: randomly select from combined samples
	for (const auto &sample : other.samples) {
		if (samples.size() < max_samples) {
			samples.push_back(sample);
		} else {
			// Random replacement
			if (other.total_count > 0) {
				idx_t j = random.NextRandomInteger(0, total_count);
				if (j < max_samples) {
					samples[j] = sample;
				}
			}
		}
	}
}

unique_ptr<HistogramCollector> HistogramCollector::Copy() const {
	auto copy = make_uniq<HistogramCollector>(type, num_buckets, max_samples);
	copy->total_count = total_count;
	copy->samples = samples;
	copy->distinct_counter = distinct_counter->Copy();
	return copy;
}

} // namespace duckdb
