//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/storage/statistics/histogram.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/types/value.hpp"
#include "duckdb/common/vector.hpp"
#include "duckdb/parser/expression/comparison_expression.hpp"

namespace duckdb {
class Vector;
class Serializer;
class Deserializer;

//! EquiHeightHistogram maintains bucket boundaries for equi-height histograms
//! Each bucket contains approximately the same number of rows
class EquiHeightHistogram {
public:
	static constexpr idx_t DEFAULT_NUM_BUCKETS = 100;
	static constexpr idx_t MIN_SAMPLE_SIZE = 1000;

	EquiHeightHistogram();
	EquiHeightHistogram(LogicalType type, vector<Value> bounds, idx_t rows_per_bucket, idx_t distinct_count);

	//! Bucket boundaries (num_buckets + 1 values)
	vector<Value> bounds;
	//! Approximate rows per bucket
	idx_t rows_per_bucket;
	//! Number of buckets
	idx_t num_buckets;
	//! Total distinct count estimate
	idx_t distinct_count;
	//! The type of values in this histogram
	LogicalType type;

public:
	//! Estimate selectivity for a comparison predicate
	double EstimateSelectivity(ExpressionType op, const Value &constant) const;

	//! Estimate selectivity for a range predicate (BETWEEN)
	double EstimateRangeSelectivity(const Value &low, const Value &high) const;

	//! Estimate the number of distinct values in a range
	idx_t EstimateDistinctInRange(const Value &low, const Value &high) const;

	//! Merge with another histogram (for parallel collection)
	void Merge(const EquiHeightHistogram &other);

	//! Create a copy of this histogram
	unique_ptr<EquiHeightHistogram> Copy() const;

	//! Convert to string representation
	string ToString() const;

	//! Check if a type is supported for histograms
	static bool TypeIsSupported(const LogicalType &type);

	//! Serialization
	void Serialize(Serializer &serializer) const;
	static unique_ptr<EquiHeightHistogram> Deserialize(Deserializer &deserializer);

private:
	//! Find the bucket index containing a value
	idx_t FindBucket(const Value &value) const;

	//! Count buckets entirely below a value
	idx_t CountBucketsBelow(const Value &value) const;

	//! Get the fractional part of a bucket that is below a value
	double PartialBucketFraction(const Value &value, idx_t bucket_idx) const;

	//! Estimate selectivity for less than operation
	double EstimateLessThan(const Value &value) const;

	//! Estimate selectivity for equality operation
	double EstimateEqual(const Value &value) const;
};

} // namespace duckdb
