#include "duckdb/storage/statistics/histogram.hpp"

#include "duckdb/common/serializer/deserializer.hpp"
#include "duckdb/common/serializer/serializer.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/types/value.hpp"

namespace duckdb {

EquiHeightHistogram::EquiHeightHistogram()
    : rows_per_bucket(0), num_buckets(0), distinct_count(0), type(LogicalType::SQLNULL) {
}

EquiHeightHistogram::EquiHeightHistogram(LogicalType type_p, vector<Value> bounds_p, idx_t rows_per_bucket_p,
                                         idx_t distinct_count_p)
    : bounds(std::move(bounds_p)), rows_per_bucket(rows_per_bucket_p), distinct_count(distinct_count_p),
      type(std::move(type_p)) {
	num_buckets = bounds.empty() ? 0 : bounds.size() - 1;
}

bool EquiHeightHistogram::TypeIsSupported(const LogicalType &type) {
	switch (type.InternalType()) {
	case PhysicalType::BOOL:
	case PhysicalType::INT8:
	case PhysicalType::INT16:
	case PhysicalType::INT32:
	case PhysicalType::INT64:
	case PhysicalType::INT128:
	case PhysicalType::UINT8:
	case PhysicalType::UINT16:
	case PhysicalType::UINT32:
	case PhysicalType::UINT64:
	case PhysicalType::UINT128:
	case PhysicalType::FLOAT:
	case PhysicalType::DOUBLE:
	case PhysicalType::VARCHAR:
		return true;
	default:
		return false;
	}
}

idx_t EquiHeightHistogram::FindBucket(const Value &value) const {
	if (bounds.empty()) {
		return 0;
	}

	// Binary search for the bucket containing the value
	idx_t low = 0;
	idx_t high = num_buckets;

	while (low < high) {
		idx_t mid = (low + high) / 2;
		// Bucket mid spans [bounds[mid], bounds[mid+1])
		if (value < bounds[mid]) {
			high = mid;
		} else if (value >= bounds[mid + 1]) {
			low = mid + 1;
		} else {
			return mid;
		}
	}

	return low;
}

idx_t EquiHeightHistogram::CountBucketsBelow(const Value &value) const {
	if (bounds.empty()) {
		return 0;
	}

	idx_t count = 0;
	for (idx_t i = 0; i < num_buckets; i++) {
		if (bounds[i + 1] <= value) {
			count++;
		} else {
			break;
		}
	}
	return count;
}

double EquiHeightHistogram::PartialBucketFraction(const Value &value, idx_t bucket_idx) const {
	if (bucket_idx >= num_buckets || bounds.empty()) {
		return 0.0;
	}

	const Value &low = bounds[bucket_idx];
	const Value &high = bounds[bucket_idx + 1];

	// For numeric types, compute the fraction
	if (type.IsNumeric()) {
		try {
			double low_d = low.GetValue<double>();
			double high_d = high.GetValue<double>();
			double val_d = value.GetValue<double>();

			if (high_d <= low_d) {
				return 0.0;
			}

			double fraction = (val_d - low_d) / (high_d - low_d);
			return std::max(0.0, std::min(1.0, fraction));
		} catch (...) {
			// If conversion fails, return 0.5 as default
			return 0.5;
		}
	}

	// For strings, use lexicographic comparison
	// This is a rough estimate
	return 0.5;
}

double EquiHeightHistogram::EstimateLessThan(const Value &value) const {
	if (bounds.empty() || num_buckets == 0) {
		return 0.5; // Unknown
	}

	// If value is below minimum, selectivity is 0
	if (value <= bounds[0]) {
		return 0.0;
	}

	// If value is above maximum, selectivity is 1
	if (value > bounds[num_buckets]) {
		return 1.0;
	}

	// Count full buckets below
	idx_t full_buckets = CountBucketsBelow(value);

	// Find partial bucket contribution
	idx_t bucket_idx = FindBucket(value);
	double partial = 0.0;
	if (bucket_idx < num_buckets) {
		partial = PartialBucketFraction(value, bucket_idx);
	}

	return (static_cast<double>(full_buckets) + partial) / static_cast<double>(num_buckets);
}

double EquiHeightHistogram::EstimateEqual(const Value &value) const {
	if (bounds.empty() || num_buckets == 0 || distinct_count == 0) {
		return 0.01; // Default estimate for unknown
	}

	// Check if value is outside histogram range
	if (value < bounds[0] || value > bounds[num_buckets]) {
		return 0.0;
	}

	// Assume uniform distribution within each bucket
	// Selectivity = 1 / distinct_count
	return 1.0 / static_cast<double>(distinct_count);
}

double EquiHeightHistogram::EstimateSelectivity(ExpressionType op, const Value &constant) const {
	if (bounds.empty() || num_buckets == 0) {
		// No histogram data, return default estimates
		switch (op) {
		case ExpressionType::COMPARE_EQUAL:
			return 0.01;
		case ExpressionType::COMPARE_NOTEQUAL:
			return 0.99;
		default:
			return 0.33;
		}
	}

	switch (op) {
	case ExpressionType::COMPARE_EQUAL:
		return EstimateEqual(constant);

	case ExpressionType::COMPARE_NOTEQUAL:
		return 1.0 - EstimateEqual(constant);

	case ExpressionType::COMPARE_LESSTHAN:
		return EstimateLessThan(constant);

	case ExpressionType::COMPARE_LESSTHANOREQUALTO: {
		// Adjust for inclusive comparison
		double lt = EstimateLessThan(constant);
		double eq = EstimateEqual(constant);
		return std::min(1.0, lt + eq);
	}

	case ExpressionType::COMPARE_GREATERTHAN:
		return 1.0 - EstimateLessThan(constant) - EstimateEqual(constant);

	case ExpressionType::COMPARE_GREATERTHANOREQUALTO:
		return 1.0 - EstimateLessThan(constant);

	default:
		return 0.33; // Default for unknown operators
	}
}

double EquiHeightHistogram::EstimateRangeSelectivity(const Value &low, const Value &high) const {
	// Range selectivity = P(x < high) - P(x < low)
	double high_sel = EstimateLessThan(high);
	double low_sel = EstimateLessThan(low);
	return std::max(0.0, high_sel - low_sel);
}

idx_t EquiHeightHistogram::EstimateDistinctInRange(const Value &low, const Value &high) const {
	if (distinct_count == 0) {
		return 0;
	}

	double selectivity = EstimateRangeSelectivity(low, high);
	return static_cast<idx_t>(std::max(1.0, selectivity * static_cast<double>(distinct_count)));
}

void EquiHeightHistogram::Merge(const EquiHeightHistogram &other) {
	// Merging equi-height histograms is complex
	// For simplicity, we keep the histogram with more buckets
	// A more sophisticated approach would involve re-bucketing

	if (other.num_buckets > num_buckets) {
		bounds = other.bounds;
		num_buckets = other.num_buckets;
		rows_per_bucket = other.rows_per_bucket;
		type = other.type;
	}

	// Merge distinct counts (take maximum as estimate)
	distinct_count = std::max(distinct_count, other.distinct_count);
}

unique_ptr<EquiHeightHistogram> EquiHeightHistogram::Copy() const {
	return make_uniq<EquiHeightHistogram>(type, bounds, rows_per_bucket, distinct_count);
}

string EquiHeightHistogram::ToString() const {
	if (bounds.empty()) {
		return "[No histogram data]";
	}

	string result = StringUtil::Format("[Histogram: %llu buckets, ~%llu rows/bucket, %llu distinct", num_buckets,
	                                   rows_per_bucket, distinct_count);

	// Show first few and last few bucket bounds
	if (bounds.size() <= 6) {
		result += ", bounds=[";
		for (idx_t i = 0; i < bounds.size(); i++) {
			if (i > 0) {
				result += ", ";
			}
			result += bounds[i].ToString();
		}
		result += "]";
	} else {
		result += ", bounds=[" + bounds[0].ToString() + ", " + bounds[1].ToString() + ", ..., " +
		          bounds[bounds.size() - 2].ToString() + ", " + bounds[bounds.size() - 1].ToString() + "]";
	}

	result += "]";
	return result;
}

void EquiHeightHistogram::Serialize(Serializer &serializer) const {
	serializer.WriteProperty(100, "type", type);
	serializer.WritePropertyWithDefault<vector<Value>>(101, "bounds", bounds);
	serializer.WritePropertyWithDefault<idx_t>(102, "rows_per_bucket", rows_per_bucket);
	serializer.WritePropertyWithDefault<idx_t>(103, "num_buckets", num_buckets);
	serializer.WritePropertyWithDefault<idx_t>(104, "distinct_count", distinct_count);
}

unique_ptr<EquiHeightHistogram> EquiHeightHistogram::Deserialize(Deserializer &deserializer) {
	auto type = deserializer.ReadProperty<LogicalType>(100, "type");
	auto bounds = deserializer.ReadPropertyWithDefault<vector<Value>>(101, "bounds");
	auto rows_per_bucket = deserializer.ReadPropertyWithDefault<idx_t>(102, "rows_per_bucket");
	auto num_buckets = deserializer.ReadPropertyWithDefault<idx_t>(103, "num_buckets");
	auto distinct_count = deserializer.ReadPropertyWithDefault<idx_t>(104, "distinct_count");

	auto result = make_uniq<EquiHeightHistogram>();
	result->type = std::move(type);
	result->bounds = std::move(bounds);
	result->rows_per_bucket = rows_per_bucket;
	result->num_buckets = num_buckets;
	result->distinct_count = distinct_count;
	return result;
}

} // namespace duckdb
