# RFC-0008: Observability Metrics Export

**Status:** Draft
**Author:** Claude Code Analysis
**Created:** November 18, 2025
**Commit Reference:** `52a07d06eafb63b60ed6275a494b85f93be4b986`

---

## Summary

Add native support for exporting DuckDB metrics to standard observability systems (Prometheus, OpenTelemetry), enabling production monitoring, alerting, and performance analysis.

---

## Motivation

DuckDB is increasingly used in production environments, but lacks standard observability integration:

### Current State
- Internal metrics available via SQL functions
- No standard export format
- No push/pull metrics support
- No distributed tracing

### Production Needs
1. **Monitoring dashboards**: Grafana, Datadog
2. **Alerting**: Query latency, memory usage, error rates
3. **Performance analysis**: Query patterns, resource utilization
4. **Distributed tracing**: Track queries across services

---

## Detailed Design

### 1. Metrics Registry

Central registry for all DuckDB metrics:

```cpp
// src/include/duckdb/common/metrics/metrics_registry.hpp
class MetricsRegistry {
public:
    // Register metrics
    Counter &RegisterCounter(const string &name, const string &help);
    Gauge &RegisterGauge(const string &name, const string &help);
    Histogram &RegisterHistogram(const string &name, const string &help,
                                  vector<double> buckets);

    // Get metrics
    string ExportPrometheus();
    string ExportOpenTelemetry();

    // Singleton access
    static MetricsRegistry &Get();
};

// Metric types
class Counter {
public:
    void Inc(double value = 1.0);
    void Inc(Labels labels, double value = 1.0);
};

class Gauge {
public:
    void Set(double value);
    void Inc(double value = 1.0);
    void Dec(double value = 1.0);
};

class Histogram {
public:
    void Observe(double value);
};
```

### 2. Built-in Metrics

Define standard DuckDB metrics:

```cpp
// src/common/metrics/builtin_metrics.cpp
void RegisterBuiltinMetrics(MetricsRegistry &registry) {
    // Query metrics
    registry.RegisterCounter(
        "duckdb_queries_total",
        "Total number of queries executed"
    );
    registry.RegisterHistogram(
        "duckdb_query_duration_seconds",
        "Query execution duration",
        {0.001, 0.01, 0.1, 1.0, 10.0, 60.0}
    );
    registry.RegisterCounter(
        "duckdb_query_errors_total",
        "Total number of query errors"
    );

    // Memory metrics
    registry.RegisterGauge(
        "duckdb_memory_usage_bytes",
        "Current memory usage"
    );
    registry.RegisterGauge(
        "duckdb_buffer_pool_size_bytes",
        "Buffer pool size"
    );

    // Storage metrics
    registry.RegisterGauge(
        "duckdb_database_size_bytes",
        "Database file size"
    );
    registry.RegisterCounter(
        "duckdb_blocks_read_total",
        "Total blocks read from disk"
    );
    registry.RegisterCounter(
        "duckdb_blocks_written_total",
        "Total blocks written to disk"
    );

    // Connection metrics
    registry.RegisterGauge(
        "duckdb_connections_active",
        "Number of active connections"
    );
}
```

### 3. Prometheus Export

HTTP endpoint for Prometheus scraping:

```cpp
// src/common/metrics/prometheus_exporter.cpp
class PrometheusExporter {
public:
    PrometheusExporter(MetricsRegistry &registry, int port = 9090);

    void Start();  // Start HTTP server
    void Stop();

private:
    void HandleMetricsRequest(HttpRequest &req, HttpResponse &res) {
        res.SetContent(registry.ExportPrometheus(), "text/plain");
    }
};

// Export format
string MetricsRegistry::ExportPrometheus() {
    stringstream ss;
    for (auto &metric : metrics) {
        ss << "# HELP " << metric.name << " " << metric.help << "\n";
        ss << "# TYPE " << metric.name << " " << metric.type << "\n";
        for (auto &value : metric.values) {
            ss << metric.name;
            if (!value.labels.empty()) {
                ss << "{" << FormatLabels(value.labels) << "}";
            }
            ss << " " << value.value << "\n";
        }
    }
    return ss.str();
}
```

### 4. OpenTelemetry Export

Support OTLP export:

```cpp
// src/common/metrics/otlp_exporter.cpp
class OTLPExporter {
public:
    OTLPExporter(MetricsRegistry &registry, const string &endpoint);

    void Export();  // Push metrics to collector

private:
    void BuildMetricsRequest(ExportMetricsServiceRequest &request);
};
```

### 5. Distributed Tracing

Trace query execution across systems:

```cpp
// src/include/duckdb/common/tracing/tracer.hpp
class Tracer {
public:
    // Start a span for an operation
    Span StartSpan(const string &name);

    // Context propagation
    void InjectContext(map<string, string> &headers);
    void ExtractContext(const map<string, string> &headers);
};

class Span {
public:
    void SetAttribute(const string &key, const string &value);
    void AddEvent(const string &name);
    void SetStatus(SpanStatus status);
    void End();
};

// Usage in executor
void Executor::Execute(QueryResult &result) {
    auto span = tracer.StartSpan("execute_query");
    span.SetAttribute("query", query_string);
    span.SetAttribute("database", database_name);

    // ... execution ...

    span.SetAttribute("rows_returned", result.RowCount());
    span.End();
}
```

### 6. Configuration

Enable via settings:

```sql
-- Enable Prometheus export
SET enable_metrics_export = true;
SET metrics_export_port = 9090;

-- Enable OTLP export
SET otlp_endpoint = 'http://collector:4317';

-- Enable tracing
SET enable_tracing = true;
SET trace_sample_rate = 0.1;  -- 10% sampling
```

---

## Example Usage

### Prometheus + Grafana Setup

```yaml
# prometheus.yml
scrape_configs:
  - job_name: 'duckdb'
    static_configs:
      - targets: ['localhost:9090']
```

```sql
-- In DuckDB
SET enable_metrics_export = true;
SET metrics_export_port = 9090;
```

### Grafana Dashboard

Example queries:
```promql
# Query rate
rate(duckdb_queries_total[5m])

# 99th percentile latency
histogram_quantile(0.99, rate(duckdb_query_duration_seconds_bucket[5m]))

# Memory usage
duckdb_memory_usage_bytes

# Error rate
rate(duckdb_query_errors_total[5m]) / rate(duckdb_queries_total[5m])
```

### Application Integration

```python
import duckdb
from opentelemetry import trace
from opentelemetry.exporter.otlp.proto.grpc.trace_exporter import OTLPSpanExporter

# Configure DuckDB
con = duckdb.connect()
con.execute("SET enable_tracing = true")
con.execute("SET otlp_endpoint = 'http://collector:4317'")

# Queries will now generate spans
con.execute("SELECT * FROM large_table")
# Span: execute_query
#   - database: memory
#   - query: SELECT * FROM large_table
#   - rows_returned: 1000000
#   - duration_ms: 523
```

---

## Implementation Plan

### Phase 1: Core Registry (Day 1-2)
- Implement `MetricsRegistry`
- Add metric types (Counter, Gauge, Histogram)
- Register built-in metrics

### Phase 2: Prometheus Export (Day 2-3)
- Implement HTTP server
- Format Prometheus output
- Test with Prometheus

### Phase 3: Instrumentation (Day 3-4)
- Instrument executor
- Instrument buffer manager
- Instrument storage

### Phase 4: OTLP/Tracing (Day 4-5)
- Implement OTLP export
- Add basic tracing
- Documentation

---

## Backwards Compatibility

This is a purely additive feature:
- Disabled by default
- No performance impact when disabled
- No API changes to existing functionality

---

## Alternatives Considered

### Alternative 1: StatsD Export
- Pro: Simple, widely supported
- Con: Less expressive, no histograms

### Alternative 2: Custom Format
- Pro: Complete control
- Con: Requires custom tooling

### Alternative 3: SQL-only Metrics
- Pro: No external dependencies
- Con: Requires polling, no standard tooling

**Decision:** Prometheus/OTLP are industry standards with rich ecosystem support.

---

## Open Questions

1. **Cardinality limits**: How many label combinations? (Proposed: Configurable, default 1000)
2. **Metric retention**: Keep history in DuckDB? (Proposed: Export only, no retention)
3. **Security**: Authentication for metrics endpoint? (Proposed: Optional basic auth)

---

## Success Criteria

- [ ] Prometheus endpoint functional
- [ ] Standard metrics exported (query, memory, storage)
- [ ] < 1% overhead when enabled
- [ ] Grafana dashboard template provided
- [ ] OTLP export working
- [ ] Documentation complete

---

## Effort Estimation

**Total: 5 developer-days**
- Core registry: 1 day
- Prometheus export: 1 day
- Instrumentation: 2 days
- OTLP/tracing: 1 day

**Risk: Low** - Additive feature with clear standards to follow.
