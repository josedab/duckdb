# DuckDB Parser - Quick Reference Guide

## Architecture Overview

```
SQL Input → libpg_query Parser → PostgreSQL AST (PGNode)
    ↓
Transformer (300+ transform methods)
    ↓
DuckDB AST (ParsedExpression, SQLStatement, QueryNode)
    ↓
Binder (Type Resolution)
    ↓
Execution Plan
```

## Key Files Location

| Component | Location | Files |
|-----------|----------|-------|
| **Parser Entry Point** | `src/parser/` | `parser.cpp`, `parser.hpp` |
| **AST Transformer** | `src/parser/` | `transformer.cpp`, `transformer.hpp` |
| **Expression Types** | `src/parser/expression/` | 19 expression types |
| **Statement Types** | `src/parser/statement/` | 29 statement types |
| **Transform Helpers** | `src/parser/transform/` | expression, statement, tableref, constraint |
| **Grammar Files** | `third_party/libpg_query/grammar/` | Main + 18 statement files |
| **Type System** | `src/include/duckdb/common/types.hpp` | 50+ LogicalTypeIds |
| **Functions** | `src/function/` | Scalar, Aggregate, Table, Window, Pragma, Copy |

## Expression Type Hierarchy

```
ParsedExpression (Base)
├── ConstantExpression
├── ParameterExpression
├── ColumnRefExpression
├── StarExpression
├── FunctionExpression (includes aggregates)
├── WindowExpression
├── CaseExpression
├── CastExpression
├── ComparisonExpression
├── ConjunctionExpression
├── OperatorExpression
├── BetweenExpression
├── SubqueryExpression
├── LambdaExpression
├── LambdaRefExpression
├── CollateExpression
├── DefaultExpression
├── PositionalReferenceExpression
└── BoundExpression (post-binding)
```

## Statement Type Hierarchy

```
SQLStatement (Base)
├── SelectStatement (+ QueryNode subtree)
├── InsertStatement
├── UpdateStatement
├── DeleteStatement
├── CreateStatement (+ CreateInfo subtype)
├── AlterStatement
├── DropStatement
├── MergeIntoStatement ← DuckDB Extension
├── CopyStatement / CopyDatabaseStatement
├── TransactionStatement
├── PragmaStatement
├── ExplainStatement
├── PrepareStatement
├── ExecuteStatement
├── LoadStatement
├── AttachStatement / DetachStatement
├── SetStatement
├── VacuumStatement
├── ExportStatement
├── CallStatement
└── ExtensionStatement
```

## Core Type System

### Numeric Types
- **Integer**: TINYINT, SMALLINT, INTEGER, BIGINT, HUGEINT, UHUGEINT
- **Unsigned**: UTINYINT, USMALLINT, UINTEGER, UBIGINT
- **Float**: FLOAT, DOUBLE
- **Fixed-point**: DECIMAL(precision, scale)

### String Types
- VARCHAR, CHAR (with length), STRING_LITERAL (binding-only)

### Temporal Types
- DATE, TIME, TIME_TZ, TIME_NS
- TIMESTAMP, TIMESTAMP_TZ, TIMESTAMP_SEC, TIMESTAMP_MS, TIMESTAMP_NS
- INTERVAL

### Composite Types
- LIST<T> (variable-length arrays)
- STRUCT (named fields with types)
- MAP<K, V> (key-value pairs)
- ENUM (ordered categorical values)

### Special Types
- BOOLEAN, UUID, BLOB, BIT, GEOMETRY
- NULL (SQLNULL), UNKNOWN, INVALID, TEMPLATE

## Expression Operators

### Comparison
```
COMPARE_EQUAL, COMPARE_NOTEQUAL,
COMPARE_LESSTHAN, COMPARE_GREATERTHAN,
COMPARE_LESSTHANOREQUALTO, COMPARE_GREATERTHANOREQUALTO,
COMPARE_IN, COMPARE_NOT_IN,
COMPARE_DISTINCT_FROM, COMPARE_NOT_DISTINCT_FROM,
COMPARE_BETWEEN, COMPARE_NOT_BETWEEN
```

### Logical
```
CONJUNCTION_AND, CONJUNCTION_OR
OPERATOR_NOT
```

### Unary/Special
```
OPERATOR_CAST, OPERATOR_IS_NULL, OPERATOR_IS_NOT_NULL,
OPERATOR_UNPACK, OPERATOR_COALESCE, OPERATOR_NULLIF,
OPERATOR_TRY
```

### Aggregates
```
AGGREGATE (parsed level)
BOUND_AGGREGATE (bound level)
GROUPING_FUNCTION
```

### Window Functions
```
WINDOW_AGGREGATE
WINDOW_RANK, WINDOW_RANK_DENSE, WINDOW_PERCENT_RANK, WINDOW_CUME_DIST
WINDOW_ROW_NUMBER, WINDOW_NTILE
WINDOW_FIRST_VALUE, WINDOW_LAST_VALUE, WINDOW_LEAD, WINDOW_LAG, WINDOW_NTH_VALUE
WINDOW_FILL
```

## Function Categories

| Category | Location | Count | Examples |
|----------|----------|-------|----------|
| Scalar | `src/function/scalar/` | 150+ | LENGTH, SUBSTR, ABS, SQRT, DATE_TRUNC |
| Aggregate | `src/function/aggregate/` | 30+ | COUNT, SUM, AVG, MIN, MAX, GROUP_CONCAT |
| Table | `src/function/table/` | 50+ | GENERATE_SERIES, read_csv, read_parquet |
| Window | `src/function/window/` | 15+ | ROW_NUMBER, RANK, LAG, LEAD |
| Pragma | `src/function/pragma/` | 20+ | Database configuration |
| Copy | `src/function/copy/` | 10+ | CSV, Parquet, JSON I/O |

## DuckDB-Specific Extensions

### MERGE INTO
```sql
MERGE INTO target_table t
USING source_table s
ON t.id = s.id
WHEN MATCHED AND cond THEN UPDATE SET ...
WHEN MATCHED AND cond THEN DELETE
WHEN NOT MATCHED THEN INSERT ...
WHEN NOT MATCHED THEN ERROR 'msg'
WHEN NOT MATCHED BY SOURCE THEN DELETE
```

### PIVOT/UNPIVOT
```sql
SELECT * FROM table
PIVOT (
  aggregate(column)
  FOR pivot_column IN (value1, value2, ...)
)

SELECT * FROM table
UNPIVOT (
  value FOR name IN (column1, column2, ...)
)
INCLUDE NULLS / EXCLUDE NULLS
```

### Advanced JOINs
```sql
t1 INNER JOIN t2 ON condition          -- Standard
t1 LEFT JOIN t2 ON condition           -- Left outer
t1 RIGHT JOIN t2 ON condition          -- Right outer
t1 FULL JOIN t2 ON condition           -- Full outer
t1 CROSS JOIN t2                       -- Cartesian product
t1 SEMI JOIN t2 ON condition           -- Any match from right (DuckDB)
t1 ANTI JOIN t2 ON condition           -- No match from right (DuckDB)
t1 ASOF JOIN t2 ON condition           -- Temporal approximate match (DuckDB)
```

### Lambda Functions
```sql
-- Element mapping
array.map(x -> x * 2)

-- Filtering
array.filter(x -> x > 5)

-- Multi-parameter lambda
list_filter(array, (x, i) -> x > i * 10)
```

## Function Stability Levels

```cpp
enum FunctionStability {
    CONSISTENT              // Always returns same result for same input
    VOLATILE                // Result changes per row (e.g., RANDOM())
    CONSISTENT_WITHIN_QUERY // Same within query but changes across queries (e.g., NOW())
}
```

## Function Resolution Order

1. **Name Lookup** → Find all functions with matching name
2. **Arity Matching** → Filter by argument count (consider varargs)
3. **Type Matching** → Attempt to match parameter types
4. **Implicit Casting** → Apply implicit type casts if needed
5. **Custom Binding** → Call bind function if available
6. **Return Type Resolution** → Infer/validate return type

## Parser Options

```cpp
struct ParserOptions {
    idx_t max_expression_depth = 1000;      // Limit nesting
    bool preserve_identifier_case = false;  // Case-sensitive names
    idx_t max_parser_depth = 10000;         // Stack depth limit
    bool accept_errors = false;             // Continue on errors
}
```

## Prepared Statement Parameters

### Positional
```sql
PREPARE stmt AS SELECT * FROM t WHERE id = $1 AND name = $2;
EXECUTE stmt(123, 'value');
```

### Named
```sql
PREPARE stmt AS SELECT * FROM t WHERE id = :id AND name = :name;
EXECUTE stmt(id := 123, name := 'value');
```

### Function Parameters
```sql
SELECT COUNT(*) FILTER (WHERE id > $1) FROM t;
SELECT some_func(param_name := 'value');
```

## Key Transformer Methods

| Method | Purpose |
|--------|---------|
| TransformStatement | Entry point for all statements |
| TransformExpression | Expression parsing |
| TransformSelectStmt | SELECT statement |
| TransformFuncCall | Function calls and aggregates |
| TransformColumnRef | Column references |
| TransformJoin | JOIN operations |
| TransformCTE | Common table expressions |
| TransformWindowDef | Window function specs |
| TransformTypeName | Type parsing |
| TransformConstraint | Table constraints |

## Error Handling

- **query_location**: Every parsed element tracks source location
- **QueryErrorContext**: Provides error context with code snippet
- **Stack Checking**: `StackChecker<Transformer>` prevents overflow
- **Unicode Normalization**: Handles special Unicode spaces
- **Parameter Validation**: Type checking during binding

## Performance Considerations

1. **Lazy Parsing**: Incremental parsing possible
2. **Caching**: Prepared statement caching
3. **Parameter Binding**: Pre-compiled parameter positions
4. **Stack Depth**: Configurable limits prevent issues
5. **Expression Trees**: Efficient copying and serialization

## Extension Points

1. **Custom Functions** → Add to function catalog
2. **Custom Types** → Extend LogicalType system
3. **Parser Extensions** → Hook for new statements
4. **Replacement Scans** → Custom table sources
5. **Macros** → Reusable SQL templates

## Common Parsing Patterns

### SELECT with Modifiers
```sql
SELECT DISTINCT col1, col2
FROM table1
WHERE condition
GROUP BY col1
HAVING aggregate > value
ORDER BY col1 ASC, col2 DESC
LIMIT n OFFSET m;
```

### INSERT with Multiple Rows
```sql
INSERT INTO table (col1, col2)
SELECT a, b FROM source
ON CONFLICT DO NOTHING
RETURNING *;
```

### CREATE with Constraints
```sql
CREATE TABLE t (
  id INTEGER PRIMARY KEY,
  name VARCHAR NOT NULL UNIQUE,
  value DECIMAL(10, 2) DEFAULT 0,
  FOREIGN KEY (parent_id) REFERENCES parent_table(id)
);
```

### CTEs and Set Operations
```sql
WITH RECURSIVE cte AS (
  SELECT 1 AS n
  UNION ALL
  SELECT n + 1 FROM cte WHERE n < 10
)
SELECT * FROM cte
UNION
SELECT * FROM another_cte
EXCEPT
SELECT * FROM excluded;
```

## Testing and Debugging

### Get Statement Type
```cpp
Parser parser;
parser.ParseQuery("SELECT 1");
// parser.statements[0]->type == StatementType::SELECT_STATEMENT
```

### Tokenization
```cpp
auto tokens = Parser::Tokenize("SELECT col FROM table");
// Returns SimplifiedToken vector with type, text, location
```

### Expression List Parsing
```cpp
auto exprs = Parser::ParseExpressionList("a, b + c, d AS alias");
// Returns vector of ParsedExpression unique_ptrs
```

---

## References

- **Main Parser**: `/home/user/duckdb/src/parser/parser.cpp`
- **Transformer**: `/home/user/duckdb/src/parser/transformer.hpp`
- **Grammar**: `/home/user/duckdb/third_party/libpg_query/grammar/`
- **Full Analysis**: `/home/user/duckdb/PARSER_ANALYSIS.md`

