//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/common/observability/otlp_exporter.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/common.hpp"

namespace duckdb {

class MetricsRegistry;
class DatabaseInstance;

//! OTLP (OpenTelemetry Protocol) metrics exporter
class OTLPExporter {
public:
	OTLPExporter(MetricsRegistry &registry, const string &endpoint);
	~OTLPExporter();

	//! Export metrics to the OTLP collector
	bool Export();

	//! Set the export endpoint
	void SetEndpoint(const string &endpoint);

	//! Get the current endpoint
	const string &GetEndpoint() const {
		return endpoint;
	}

	//! Get singleton for a database instance
	static OTLPExporter &Get(DatabaseInstance &db);

private:
	MetricsRegistry &registry;
	string endpoint;

	//! Build OTLP metrics request (simplified JSON format)
	string BuildMetricsRequest();
};

} // namespace duckdb
