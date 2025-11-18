#include "duckdb/common/observability/otlp_exporter.hpp"
#include "duckdb/common/observability/metrics_registry.hpp"
#include "duckdb/main/database.hpp"

#include <sstream>

namespace duckdb {

OTLPExporter::OTLPExporter(MetricsRegistry &registry, const string &endpoint)
    : registry(registry), endpoint(endpoint) {
}

OTLPExporter::~OTLPExporter() {
}

void OTLPExporter::SetEndpoint(const string &new_endpoint) {
	endpoint = new_endpoint;
}

bool OTLPExporter::Export() {
	if (endpoint.empty()) {
		return false;
	}

	// Build the OTLP metrics request
	string request_body = BuildMetricsRequest();

	// Note: In a full implementation, this would use HTTPUtil to POST
	// the metrics to the OTLP collector endpoint.
	// For now, we just build the request body.

	// The actual HTTP POST would be:
	// HTTPUtil::Get(db).POST(endpoint + "/v1/metrics", request_body, "application/json");

	return true;
}

string OTLPExporter::BuildMetricsRequest() {
	// Use the OTLP JSON format from MetricsRegistry
	return registry.ExportOpenTelemetry();
}

OTLPExporter &OTLPExporter::Get(DatabaseInstance &db) {
	return *db.config.otlp_exporter;
}

} // namespace duckdb
