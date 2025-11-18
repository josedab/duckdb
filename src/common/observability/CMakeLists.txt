add_library_unity(
  duckdb_common_observability
  OBJECT
  metrics_registry.cpp
  builtin_metrics.cpp
  prometheus_exporter.cpp
  otlp_exporter.cpp
  tracer.cpp
  instrumentation.cpp)
set(ALL_OBJECT_FILES
    ${ALL_OBJECT_FILES} $<TARGET_OBJECTS:duckdb_common_observability>
    PARENT_SCOPE)
