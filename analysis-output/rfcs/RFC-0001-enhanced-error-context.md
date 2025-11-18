# RFC-0001: Enhanced Error Context for Parse and Bind Errors

**Status:** Draft
**Author:** Claude Code Analysis
**Created:** November 18, 2025
**Commit Reference:** `52a07d06eafb63b60ed6275a494b85f93be4b986`

---

## Summary

Enhance DuckDB's error messages to include source code context, showing the exact location of errors with visual indicators. This improves developer experience by reducing time to diagnose issues.

---

## Motivation

Currently, DuckDB's error messages include position information but don't show the surrounding code:

```
Error: Parser Error: syntax error at or near "FRON"
```

Users must manually locate the error in their query. With complex queries, this is error-prone and time-consuming.

### Problem Examples

**Current behavior:**
```sql
SELECT * FRON users WHERE id = 1;
```
```
Error: Parser Error: syntax error at or near "FRON"
```

**Desired behavior:**
```sql
SELECT * FRON users WHERE id = 1;
```
```
Error: Parser Error: syntax error at or near "FRON"
  SELECT * FRON users WHERE id = 1;
           ^^^^
  Hint: Did you mean FROM?
```

---

## Detailed Design

### 1. Error Context Structure

Add a context structure to store error location:

```cpp
// src/include/duckdb/common/error_context.hpp
struct ErrorContext {
    string source;           // Original query
    idx_t error_position;    // Character position
    idx_t error_length;      // Error span length
    vector<string> hints;    // Suggested fixes

    string FormatContext(idx_t context_lines = 2) const;
};
```

### 2. Parser Integration

Modify the parser to capture and propagate context:

```cpp
// src/parser/parser.cpp
void Parser::ParseQuery(const string &query) {
    try {
        // Existing parsing logic
    } catch (ParserException &e) {
        // Enhance with context
        e.AddContext(ErrorContext{
            query,
            e.GetPosition(),
            e.GetLength(),
            GenerateHints(query, e)
        });
        throw;
    }
}
```

### 3. Context Formatting

Format the error with visual indicators:

```cpp
// src/common/error_context.cpp
string ErrorContext::FormatContext(idx_t context_lines) const {
    stringstream result;

    // Find line containing error
    auto lines = SplitLines(source);
    auto [line_num, col] = GetLineAndColumn(error_position);

    // Show context lines before
    for (idx_t i = max(0, line_num - context_lines); i < line_num; i++) {
        result << "  " << lines[i] << "\n";
    }

    // Show error line with indicator
    result << "  " << lines[line_num] << "\n";
    result << "  " << string(col, ' ') << string(error_length, '^') << "\n";

    // Add hints
    for (auto &hint : hints) {
        result << "  Hint: " << hint << "\n";
    }

    return result.str();
}
```

### 4. Hint Generation

Generate helpful suggestions:

```cpp
// src/parser/hint_generator.cpp
vector<string> GenerateHints(const string &query, const ParserException &e) {
    vector<string> hints;

    // Typo detection for keywords
    if (e.GetType() == ParserExceptionType::UNKNOWN_KEYWORD) {
        auto suggestions = FuzzyMatchKeywords(e.GetToken());
        if (!suggestions.empty()) {
            hints.push_back("Did you mean " + suggestions[0] + "?");
        }
    }

    // Missing clause detection
    if (e.GetType() == ParserExceptionType::UNEXPECTED_TOKEN) {
        auto expected = GetExpectedTokens(e);
        if (!expected.empty()) {
            hints.push_back("Expected: " + JoinStrings(expected, ", "));
        }
    }

    return hints;
}
```

### 5. Binder Integration

Extend to binder errors:

```cpp
// src/planner/binder.cpp
void Binder::Bind(SQLStatement &statement) {
    try {
        // Existing binding logic
    } catch (BinderException &e) {
        if (e.HasExpression()) {
            e.AddContext(ErrorContext{
                statement.query_string,
                e.GetExpression().query_location,
                e.GetExpression().GetLength(),
                GenerateBinderHints(e)
            });
        }
        throw;
    }
}
```

---

## Example Usage

### Before

```sql
SELECT naem, SUM(amunt)
FROM orders
GROUP BY naem;
```
```
Error: Binder Error: Referenced column "naem" not found in FROM clause!
```

### After

```sql
SELECT naem, SUM(amunt)
FROM orders
GROUP BY naem;
```
```
Error: Binder Error: Referenced column "naem" not found in FROM clause!
  SELECT naem, SUM(amunt)
         ^^^^
  Hint: Did you mean "name"? (similarity: 0.8)
  Available columns: id, name, amount, status, created_at
```

---

## Implementation Plan

### Phase 1: Core Infrastructure (Day 1)
- Add `ErrorContext` structure
- Implement context formatting
- Unit tests for formatting

### Phase 2: Parser Integration (Day 2)
- Capture error positions in parser
- Propagate context through exceptions
- Add keyword typo detection

### Phase 3: Binder Integration (Day 3)
- Capture expression locations
- Add column name suggestions
- Add table name suggestions

---

## Backwards Compatibility

This change is additive and backwards compatible:
- Existing error messages remain valid
- Context is additional information
- No API changes required

---

## Alternatives Considered

### Alternative 1: External Linter
- Pro: No core changes
- Con: Duplicate parsing, separate tool

### Alternative 2: IDE Integration Only
- Pro: Rich UI possible
- Con: CLI users left out

### Alternative 3: Machine-Readable Errors
- Pro: Tooling can parse
- Con: Human readability suffers

**Decision:** Inline context is the most universally useful approach.

---

## Open Questions

1. **Context length**: How many lines to show? (Proposed: 2)
2. **Hint aggressiveness**: Only high-confidence hints? (Proposed: > 0.7 similarity)
3. **Color output**: Terminal colors for CLI? (Proposed: Optional, off by default)

---

## Success Criteria

- [ ] 80% of parse errors include context
- [ ] 50% of binder errors include context
- [ ] Hints provided for common typos
- [ ] < 5% performance overhead for error cases
- [ ] Documentation updated

---

## Effort Estimation

**Total: 3 developer-days**
- Core infrastructure: 1 day
- Parser integration: 1 day
- Binder integration: 1 day

**Risk: Low** - Changes are localized to error handling.
