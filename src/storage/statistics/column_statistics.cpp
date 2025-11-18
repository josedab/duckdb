#include "duckdb/storage/statistics/column_statistics.hpp"

#include "duckdb/common/serializer/deserializer.hpp"
#include "duckdb/common/serializer/serializer.hpp"

namespace duckdb {

ColumnStatistics::ColumnStatistics(BaseStatistics stats_p) : stats(std::move(stats_p)) {
	if (DistinctStatistics::TypeIsSupported(stats.GetType())) {
		distinct_stats = make_uniq<DistinctStatistics>();
	}
}
ColumnStatistics::ColumnStatistics(BaseStatistics stats_p, unique_ptr<DistinctStatistics> distinct_stats_p)
    : stats(std::move(stats_p)), distinct_stats(std::move(distinct_stats_p)) {
}
ColumnStatistics::ColumnStatistics(BaseStatistics stats_p, unique_ptr<DistinctStatistics> distinct_stats_p,
                                   unique_ptr<EquiHeightHistogram> histogram_p)
    : stats(std::move(stats_p)), distinct_stats(std::move(distinct_stats_p)), histogram(std::move(histogram_p)) {
}

shared_ptr<ColumnStatistics> ColumnStatistics::CreateEmptyStats(const LogicalType &type) {
	return make_shared_ptr<ColumnStatistics>(BaseStatistics::CreateEmpty(type));
}

void ColumnStatistics::Merge(ColumnStatistics &other) {
	stats.Merge(other.stats);
	if (distinct_stats && other.distinct_stats) {
		distinct_stats->Merge(*other.distinct_stats);
	}
	if (histogram && other.histogram) {
		histogram->Merge(*other.histogram);
	}
}

BaseStatistics &ColumnStatistics::Statistics() {
	return stats;
}

bool ColumnStatistics::HasDistinctStats() {
	return distinct_stats.get();
}

DistinctStatistics &ColumnStatistics::DistinctStats() {
	if (!distinct_stats) {
		throw InternalException("DistinctStats called without distinct_stats");
	}
	return *distinct_stats;
}

void ColumnStatistics::SetDistinct(unique_ptr<DistinctStatistics> distinct) {
	this->distinct_stats = std::move(distinct);
}

bool ColumnStatistics::HasHistogram() {
	return histogram.get();
}

EquiHeightHistogram &ColumnStatistics::Histogram() {
	if (!histogram) {
		throw InternalException("Histogram called without histogram data");
	}
	return *histogram;
}

void ColumnStatistics::SetHistogram(unique_ptr<EquiHeightHistogram> hist) {
	this->histogram = std::move(hist);
}

double ColumnStatistics::EstimateSelectivity(ExpressionType op, const Value &constant) {
	if (histogram) {
		return histogram->EstimateSelectivity(op, constant);
	}
	// Default estimates when no histogram available
	switch (op) {
	case ExpressionType::COMPARE_EQUAL:
		return 0.01;
	case ExpressionType::COMPARE_NOTEQUAL:
		return 0.99;
	default:
		return 0.33;
	}
}

void ColumnStatistics::UpdateDistinctStatistics(Vector &v, idx_t count, Vector &hashes) {
	if (!distinct_stats) {
		return;
	}
	// we use a sample to update the distinct statistics for performance reasons
	distinct_stats->UpdateSample(v, count, hashes);
}

shared_ptr<ColumnStatistics> ColumnStatistics::Copy() const {
	return make_shared_ptr<ColumnStatistics>(stats.Copy(), distinct_stats ? distinct_stats->Copy() : nullptr,
	                                         histogram ? histogram->Copy() : nullptr);
}

void ColumnStatistics::Serialize(Serializer &serializer) const {
	serializer.WriteProperty(100, "statistics", stats);
	serializer.WritePropertyWithDefault(101, "distinct", distinct_stats, unique_ptr<DistinctStatistics>());
	serializer.WritePropertyWithDefault(102, "histogram", histogram, unique_ptr<EquiHeightHistogram>());
}

shared_ptr<ColumnStatistics> ColumnStatistics::Deserialize(Deserializer &deserializer) {
	auto stats = deserializer.ReadProperty<BaseStatistics>(100, "statistics");
	auto distinct_stats = deserializer.ReadPropertyWithExplicitDefault<unique_ptr<DistinctStatistics>>(
	    101, "distinct", unique_ptr<DistinctStatistics>());
	auto histogram = deserializer.ReadPropertyWithExplicitDefault<unique_ptr<EquiHeightHistogram>>(
	    102, "histogram", unique_ptr<EquiHeightHistogram>());
	return make_shared_ptr<ColumnStatistics>(std::move(stats), std::move(distinct_stats), std::move(histogram));
}

} // namespace duckdb
