# Enhanced Error Context and Hint Generator

## Overview

The HintGenerator class provides intelligent suggestions for common SQL errors, improving the developer experience by:

1. Suggesting keyword corrections for typos (e.g., `FRON` -> `FROM`)
2. Suggesting similar column/table names for binding errors
3. Formatting helpful hint messages with the suggestions

## Usage

### Keyword Suggestions

```cpp
#include "duckdb/parser/hint_generator.hpp"

// Get keyword suggestions for a typo
auto suggestions = HintGenerator::GetKeywordSuggestions("FRON");
// Returns: ["FROM"]

// Format as a hint message
string hint = HintGenerator::FormatKeywordHint(suggestions);
// Returns: "Did you mean FROM?"
```

### Identifier Suggestions

```cpp
vector<string> columns = {"customer_id", "customer_name", "order_id"};

// Get suggestions for a misspelled column name
auto suggestions = HintGenerator::GetSuggestions("cusomer_id", columns);
// Returns: ["customer_id"]

// Format as a hint message
string hint = HintGenerator::FormatIdentifierHint(suggestions);
// Returns: "Did you mean \"customer_id\"?"
```

### In Parser Exceptions

```cpp
// Parser errors automatically include hints via ParserException::SyntaxError
throw ParserException::SyntaxError(query, error_message, error_location);
// Will include keyword hints if the error token matches a known keyword typo
```

### In Binder Exceptions

```cpp
// Column not found with suggestions
throw BinderException::ColumnNotFound(column_name, similar_columns, context);

// Table not found with suggestions
throw BinderException::TableNotFound(table_name, similar_tables, context);

// Ambiguous column reference
throw BinderException::AmbiguousReference(column_name, tables_containing_column, context);
```

## Configuration

- `KEYWORD_SIMILARITY_THRESHOLD = 0.7` - Minimum Jaro-Winkler similarity for keyword suggestions
- `IDENTIFIER_SIMILARITY_THRESHOLD = 0.6` - Minimum similarity for identifier suggestions
- `MAX_SUGGESTIONS = 3` - Maximum number of suggestions to return

## Example Output

### Parser Error

```
Error: Parser Error: syntax error at or near "FRON"

LINE 1: SELECT * FRON users WHERE id = 1;
                 ^
Did you mean FROM?
```

### Binder Error

```
Error: Binder Error: Referenced column "cusomer_id" not found in FROM clause!
Candidate bindings: "customer_id"
```

## Implementation Details

The hint generator uses the Jaro-Winkler similarity algorithm (via `StringUtil::TopNJaroWinkler`) to find similar strings. This algorithm is particularly good for detecting typos as it gives higher scores to strings that match from the beginning.

The list of SQL keywords is focused on commonly used keywords that are frequently misspelled. The similarity threshold (0.7 by default) ensures that only high-confidence suggestions are shown to avoid confusing users with irrelevant options.
