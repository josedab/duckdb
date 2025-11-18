//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/common/observability/prometheus_exporter.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/common.hpp"
#include "duckdb/common/mutex.hpp"
#include "duckdb/common/atomic.hpp"

#include <thread>

namespace duckdb {

class MetricsRegistry;
class DatabaseInstance;

//! HTTP server for Prometheus metrics scraping
class PrometheusExporter {
public:
	PrometheusExporter(MetricsRegistry &registry, int port = 9090);
	~PrometheusExporter();

	//! Start the HTTP server
	void Start();

	//! Stop the HTTP server
	void Stop();

	//! Check if server is running
	bool IsRunning() const {
		return running;
	}

	//! Get the port
	int GetPort() const {
		return port;
	}

	//! Get singleton for a database instance
	static PrometheusExporter &Get(DatabaseInstance &db);

private:
	MetricsRegistry &registry;
	int port;
	atomic<bool> running;
	atomic<bool> should_stop;

	unique_ptr<std::thread> server_thread;
	int server_socket;

	//! Server loop
	void ServerLoop();

	//! Handle a single HTTP request
	void HandleRequest(int client_socket);

	//! Parse HTTP request and check if it's for metrics
	bool IsMetricsRequest(const string &request);

	//! Build HTTP response
	string BuildResponse(const string &body, const string &content_type, int status_code = 200);
};

} // namespace duckdb
