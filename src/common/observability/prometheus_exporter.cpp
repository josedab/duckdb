#include "duckdb/common/observability/prometheus_exporter.hpp"
#include "duckdb/common/observability/metrics_registry.hpp"
#include "duckdb/main/database.hpp"

#ifndef _WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#endif

#include <cstring>

namespace duckdb {

PrometheusExporter::PrometheusExporter(MetricsRegistry &registry, int port)
    : registry(registry), port(port), running(false), should_stop(false), server_socket(-1) {
}

PrometheusExporter::~PrometheusExporter() {
	Stop();
}

void PrometheusExporter::Start() {
	if (running) {
		return;
	}

	// Create socket
#ifdef _WIN32
	WSADATA wsa_data;
	if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
		throw IOException("Failed to initialize Winsock");
	}
#endif

	server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (server_socket < 0) {
		throw IOException("Failed to create socket for Prometheus exporter");
	}

	// Set socket options
	int opt = 1;
#ifdef _WIN32
	setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));
#else
	setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	// Set non-blocking
	int flags = fcntl(server_socket, F_GETFL, 0);
	fcntl(server_socket, F_SETFL, flags | O_NONBLOCK);
#endif

	// Bind
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(port);

	if (bind(server_socket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
#ifdef _WIN32
		closesocket(server_socket);
#else
		close(server_socket);
#endif
		throw IOException("Failed to bind Prometheus exporter to port " + std::to_string(port));
	}

	// Listen
	if (listen(server_socket, 5) < 0) {
#ifdef _WIN32
		closesocket(server_socket);
#else
		close(server_socket);
#endif
		throw IOException("Failed to listen on Prometheus exporter socket");
	}

	should_stop = false;
	running = true;

	// Start server thread
	server_thread = make_uniq<std::thread>(&PrometheusExporter::ServerLoop, this);
}

void PrometheusExporter::Stop() {
	if (!running) {
		return;
	}

	should_stop = true;

	// Close server socket to interrupt accept()
	if (server_socket >= 0) {
#ifdef _WIN32
		closesocket(server_socket);
		WSACleanup();
#else
		close(server_socket);
#endif
		server_socket = -1;
	}

	// Wait for thread to finish
	if (server_thread && server_thread->joinable()) {
		server_thread->join();
	}

	running = false;
}

void PrometheusExporter::ServerLoop() {
	while (!should_stop) {
#ifndef _WIN32
		struct pollfd pfd;
		pfd.fd = server_socket;
		pfd.events = POLLIN;

		int poll_result = poll(&pfd, 1, 100); // 100ms timeout
		if (poll_result <= 0) {
			continue;
		}
#endif

		struct sockaddr_in client_addr;
		socklen_t client_len = sizeof(client_addr);

		int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
		if (client_socket < 0) {
			if (should_stop) {
				break;
			}
			continue;
		}

		// Handle the request
		HandleRequest(client_socket);

// Close client socket
#ifdef _WIN32
		closesocket(client_socket);
#else
		close(client_socket);
#endif
	}
}

void PrometheusExporter::HandleRequest(int client_socket) {
	// Read request
	char buffer[4096];
	memset(buffer, 0, sizeof(buffer));

#ifdef _WIN32
	int bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
#else
	ssize_t bytes_read = read(client_socket, buffer, sizeof(buffer) - 1);
#endif

	if (bytes_read <= 0) {
		return;
	}

	string request(buffer);

	// Check if this is a metrics request
	string response;
	if (IsMetricsRequest(request)) {
		string metrics = registry.ExportPrometheus();
		response = BuildResponse(metrics, "text/plain; version=0.0.4; charset=utf-8");
	} else {
		// Return 404 for other paths
		response = BuildResponse("Not Found", "text/plain", 404);
	}

// Send response
#ifdef _WIN32
	send(client_socket, response.c_str(), (int)response.length(), 0);
#else
	write(client_socket, response.c_str(), response.length());
#endif
}

bool PrometheusExporter::IsMetricsRequest(const string &request) {
	// Check for GET /metrics or GET /
	return request.find("GET /metrics") != string::npos || request.find("GET / HTTP") != string::npos;
}

string PrometheusExporter::BuildResponse(const string &body, const string &content_type, int status_code) {
	string status_text;
	switch (status_code) {
	case 200:
		status_text = "OK";
		break;
	case 404:
		status_text = "Not Found";
		break;
	default:
		status_text = "Unknown";
		break;
	}

	std::stringstream ss;
	ss << "HTTP/1.1 " << status_code << " " << status_text << "\r\n";
	ss << "Content-Type: " << content_type << "\r\n";
	ss << "Content-Length: " << body.length() << "\r\n";
	ss << "Connection: close\r\n";
	ss << "\r\n";
	ss << body;

	return ss.str();
}

PrometheusExporter &PrometheusExporter::Get(DatabaseInstance &db) {
	return *db.config.prometheus_exporter;
}

} // namespace duckdb
