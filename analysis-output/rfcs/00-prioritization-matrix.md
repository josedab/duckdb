# RFC Prioritization Matrix

**Commit SHA:** `52a07d06eafb63b60ed6275a494b85f93be4b986`
**Analysis Date:** November 18, 2025

---

## Overview

This matrix prioritizes the proposed improvements based on impact and effort. Each RFC is categorized and scored to help with planning.

---

## Impact vs. Effort Grid

```
High    │ RFC-0003        │ RFC-0006         │
Impact  │ RFC-0008        │ RFC-0005         │
        │                 │                  │
        ├─────────────────┼──────────────────┤
Medium  │ RFC-0001        │ RFC-0004         │
Impact  │ RFC-0002        │ RFC-0007         │
        │                 │                  │
        └─────────────────┴──────────────────┘
         Low Effort        High Effort
         (Quick Wins)      (Strategic/Long-term)
```

---

## RFC Summary Table

| RFC | Title | Category | Effort | Impact | Priority |
|-----|-------|----------|--------|--------|----------|
| 0001 | Enhanced Error Context | Quick Win | 3 days | Medium | High |
| 0002 | Query Progress API | Quick Win | 4 days | Medium | High |
| 0003 | Unified Memory Manager | Strategic | 3 weeks | High | High |
| 0004 | Statistics Histograms | Strategic | 4 weeks | Medium | Medium |
| 0005 | Expression Compilation | Long-term | 8 weeks | High | Medium |
| 0006 | Adaptive Join Selection | Strategic | 3 weeks | High | High |
| 0007 | Parallel DDL Operations | Long-term | 6 weeks | Medium | Low |
| 0008 | Observability Export | Quick Win | 5 days | High | High |

---

## Categories

### Quick Wins (< 1 week)
- **RFC-0001**: Enhanced Error Context
- **RFC-0002**: Query Progress API
- **RFC-0008**: Observability Export

### Strategic (2-4 weeks)
- **RFC-0003**: Unified Memory Manager
- **RFC-0004**: Statistics Histograms
- **RFC-0006**: Adaptive Join Selection

### Long-term (> 1 month)
- **RFC-0005**: Expression Compilation
- **RFC-0007**: Parallel DDL Operations

---

## Recommended Implementation Order

### Phase 1: Foundation (Weeks 1-2)
1. **RFC-0008**: Observability Export - Enables measuring other improvements
2. **RFC-0001**: Enhanced Error Context - Improves developer experience

### Phase 2: Performance Infrastructure (Weeks 3-6)
3. **RFC-0003**: Unified Memory Manager - Foundation for memory improvements
4. **RFC-0006**: Adaptive Join Selection - Significant query speedup

### Phase 3: Optimization (Weeks 7-12)
5. **RFC-0002**: Query Progress API - User experience
6. **RFC-0004**: Statistics Histograms - Better optimizer decisions

### Phase 4: Advanced (Weeks 13+)
7. **RFC-0005**: Expression Compilation - Advanced optimization
8. **RFC-0007**: Parallel DDL Operations - Large-scale operations

---

## Success Metrics

| RFC | Success Criteria |
|-----|------------------|
| 0001 | 80% of parse errors include context |
| 0002 | Progress available for queries > 1s |
| 0003 | Single allocation tracking point |
| 0004 | 20% reduction in cardinality estimation error |
| 0005 | 50% speedup for expression-heavy queries |
| 0006 | Automatic join algorithm switching |
| 0007 | 4x speedup for bulk DDL |
| 0008 | Prometheus/OpenTelemetry export |

---

## Dependencies

```
RFC-0005 (Expression Compilation)
    └── RFC-0003 (Unified Memory Manager)

RFC-0006 (Adaptive Join)
    └── RFC-0004 (Statistics Histograms)

RFC-0008 (Observability)
    └── (none - start here)
```

---

## Risk Assessment

| RFC | Risk Level | Mitigation |
|-----|------------|------------|
| 0001 | Low | Localized changes |
| 0002 | Low | Additive API |
| 0003 | Medium | Gradual migration |
| 0004 | Low | Backward compatible |
| 0005 | High | Feature flag, benchmarks |
| 0006 | Medium | Fallback to static |
| 0007 | Medium | Per-table locking |
| 0008 | Low | Optional feature |

---

## Resource Requirements

| RFC | Primary Skills | Est. LOC |
|-----|----------------|----------|
| 0001 | Parser, Exceptions | 500 |
| 0002 | Executor, API | 800 |
| 0003 | Memory, Buffer Mgmt | 2000 |
| 0004 | Statistics, Optimizer | 1500 |
| 0005 | Codegen, LLVM | 5000 |
| 0006 | Join Algorithms | 1200 |
| 0007 | DDL, Parallelism | 1800 |
| 0008 | Metrics, Export | 600 |
