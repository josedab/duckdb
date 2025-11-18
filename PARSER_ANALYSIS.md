# DuckDB SQL Parser and Language Support Analysis

## 1. Parser Architecture

### 1.1 Parser Generation Strategy
- **Architecture**: Generated parser using libpg_query (PostgreSQL parser fork)
- **Technology**: Yacc/Bison-based (`.y` grammar files)
- **Approach**: Hand-written transformer on top of generated parser
- **Generator**: libpg_query from PostgreSQL, integrated in `third_party/libpg_query/`

### 1.2 Parser Flow
```
SQL Input
    ↓
Parser (libpg_query) → PostgreSQL AST (PGNode structures)
    ↓
Transformer (TransformStatement, TransformExpression, etc.)
    ↓
DuckDB AST (SQLStatement, ParsedExpression, QueryNode)
    ↓
Binder (Type resolution and semantic analysis)
    ↓
Execution Plan
```

### 1.3 Key Components

#### Parser Classes (`src/parser/`)
- **Parser**: Main entry point for parsing
  - Methods: `ParseQuery()`, `ParseExpressionList()`, `ParseGroupByList()`, `ParseOrderList()`
  - Supports streaming and incremental parsing
  - Unicode space normalization
  - Dollar-quoted string handling

#### Transformer Classes (`src/parser/transformer.hpp`)
- **Transformer**: Converts libpgquery AST to DuckDB AST
  - Statement transformers: ~30 different statement types
  - Expression transformers: Binary, unary, function calls, window functions
  - TableRef transformers: JOINs, subqueries, table functions
  - Constraint transformers
  - Handles prepared statement parameters (positional and named)
  - Stack depth checking to prevent stack overflow

### 1.4 Grammar Files
```
third_party/libpg_query/grammar/
├── grammar.y (main grammar)
├── statements/
│   ├── select.y (4,318 lines)
│   ├── alter_sequence.y
│   ├── pragma.y
│   ├── create_function.y
│   ├── execute.y
│   ├── copy.y
│   ├── alter_table.y
│   ├── create_sequence.y
│   ├── index.y
│   ├── variable_reset.y
│   ├── create_type.y
│   ├── variable_show.y
│   ├── rename.y
│   ├── create_secret.y
│   ├── drop.y
│   ├── comment_on.y
│   ├── update_extensions.y
│   ├── delete.y
│   └── analyze.y
```

---

## 2. AST Representation

### 2.1 Base Classes

#### ParsedExpression Hierarchy
- **Base Class**: `ParsedExpression`
  - Base of all expressions in parser phase (untyped, unbound)
  - Contains: type, expression_class, alias, query_location
  - Methods: Copy(), Serialize/Deserialize(), Hash(), Equals()

#### Expression Types
1. **Literal/Value Expressions**
   - ConstantExpression: Literal values
   - ParameterExpression: $1, $2, etc.
   - DefaultExpression: DEFAULT keyword

2. **Reference Expressions**
   - ColumnRefExpression: Column references
   - StarExpression: SELECT *, table.*, COLUMNS, UNPACKED
   - PositionalReferenceExpression: #1, #2 positional references
   - LambdaRefExpression: Lambda variable references

3. **Operator Expressions**
   - ComparisonExpression: =, !=, <, >, <=, >=, IN, BETWEEN, DISTINCT FROM
   - ConjunctionExpression: AND, OR
   - OperatorExpression: Unary and binary operators
   - BetweenExpression: BETWEEN ... AND ...
   - CollateExpression: Collation modifiers

4. **Function and Aggregate Expressions**
   - FunctionExpression: Regular function calls
     - Supports: catalog.schema.function_name
     - Properties: distinct, filter, order_by, export_state
   - WindowExpression: Window functions with PARTITION BY, ORDER BY
   - LambdaExpression: Lambda functions (e.g., x, y -> x + y)
   - CaseExpression: CASE WHEN ... THEN ... END

5. **Control Flow Expressions**
   - CastExpression: CAST(expr AS type)
   - SubqueryExpression: (SELECT ...) subqueries

#### Statement Hierarchy
- **Base Class**: `SQLStatement`
  - 29 different statement types

### 2.2 Statement Types

| Statement Type | Purpose |
|---|---|
| SelectStatement | SELECT queries |
| InsertStatement | INSERT INTO ... VALUES/SELECT |
| UpdateStatement | UPDATE ... SET |
| DeleteStatement | DELETE FROM ... |
| CreateStatement | CREATE TABLE/VIEW/SCHEMA/INDEX/FUNCTION/TYPE |
| AlterStatement | ALTER TABLE/DATABASE/SCHEMA/SEQUENCE |
| DropStatement | DROP TABLE/VIEW/SCHEMA/INDEX/FUNCTION |
| MergeIntoStatement | MERGE INTO ... (DuckDB extension) |
| CopyStatement | COPY TO/FROM |
| CopyDatabaseStatement | COPY DATABASE FROM (DuckDB extension) |
| PrepareStatement | PREPARE ... AS |
| ExecuteStatement | EXECUTE prepared_statement |
| TransactionStatement | BEGIN/COMMIT/ROLLBACK |
| PragmaStatement | PRAGMA commands |
| ExplainStatement | EXPLAIN SELECT ... |
| ExportStatement | EXPORT ... AS |
| LoadStatement | LOAD 'extension' |
| AttachStatement | ATTACH DATABASE |
| DetachStatement | DETACH DATABASE |
| CallStatement | CALL function(...) |
| SetStatement | SET/RESET variables |
| VacuumStatement | VACUUM |
| MultiStatement | Multiple statements in one |
| ExtensionStatement | Custom extension statements |

### 2.3 Query Nodes
Represent structure of SELECT statements (separate from statements):
- **SelectNode**: Single SELECT
- **SetOperationNode**: UNION, INTERSECT, EXCEPT
- **RecursiveCTENode**: Recursive CTEs
- **CTENode**: WITH clauses

### 2.4 Expression Types Enum
```cpp
enum class ExpressionType : uint8_t {
    // Operators
    OPERATOR_CAST, OPERATOR_NOT, OPERATOR_IS_NULL, OPERATOR_IS_NOT_NULL, 
    OPERATOR_UNPACK, OPERATOR_NULLIF, OPERATOR_COALESCE, OPERATOR_TRY
    
    // Comparisons
    COMPARE_EQUAL, COMPARE_NOTEQUAL, COMPARE_LESSTHAN, COMPARE_GREATERTHAN,
    COMPARE_LESSTHANOREQUALTO, COMPARE_GREATERTHANOREQUALTO, COMPARE_IN,
    COMPARE_NOT_IN, COMPARE_DISTINCT_FROM, COMPARE_NOT_DISTINCT_FROM,
    COMPARE_BETWEEN, COMPARE_NOT_BETWEEN
    
    // Logical
    CONJUNCTION_AND, CONJUNCTION_OR
    
    // Values
    VALUE_CONSTANT, VALUE_PARAMETER, VALUE_TUPLE, VALUE_TUPLE_ADDRESS,
    VALUE_NULL, VALUE_VECTOR, VALUE_SCALAR, VALUE_DEFAULT
    
    // Aggregates & Window Functions
    AGGREGATE, BOUND_AGGREGATE, GROUPING_FUNCTION,
    WINDOW_AGGREGATE, WINDOW_RANK, WINDOW_RANK_DENSE, WINDOW_NTILE,
    WINDOW_PERCENT_RANK, WINDOW_CUME_DIST, WINDOW_ROW_NUMBER,
    WINDOW_FIRST_VALUE, WINDOW_LAST_VALUE, WINDOW_LEAD, WINDOW_LAG,
    WINDOW_NTH_VALUE, WINDOW_FILL
    
    // Functions
    FUNCTION, BOUND_FUNCTION
    
    // Array/Struct
    ARRAY_EXTRACT, ARRAY_SLICE, ARRAY_CONSTRUCTOR, STRUCT_EXTRACT, ARROW
    
    // Other
    CASE_EXPR, SUBQUERY, STAR, TABLE_STAR, COLLATE, LAMBDA, CAST, BOUND_REF
}
```

---

## 3. SQL Standard Compliance and Extensions

### 3.1 Standard SQL Support
- **Core SQL**: SELECT, INSERT, UPDATE, DELETE
- **DDL**: CREATE TABLE/VIEW/INDEX/SCHEMA, ALTER TABLE, DROP TABLE/VIEW
- **DML**: Full SQL expressions, subqueries, CTEs
- **Window Functions**: PARTITION BY, ORDER BY, frame specifications
- **Set Operations**: UNION, UNION ALL, INTERSECT, EXCEPT
- **Joins**: INNER, LEFT, RIGHT, FULL, CROSS, NATURAL
- **Aggregates**: Standard aggregate functions with FILTER clause
- **Type Casting**: CAST(expr AS type)
- **Comparisons**: =, !=, <, >, <=, >=, IN, BETWEEN, IS NULL, LIKE
- **Expressions**: CASE WHEN, COALESCE, NULLIF
- **CTEs**: WITH clause, recursive CTEs

### 3.2 DuckDB-Specific Extensions

#### MERGE INTO Statement
- Extension for conditional insert/update/delete
- Supports multiple WHEN MATCHED/NOT MATCHED conditions
- Actions: INSERT, UPDATE, DELETE, DO NOTHING, ERROR
```sql
MERGE INTO target_table t
USING source_table s
ON t.id = s.id
WHEN MATCHED AND condition THEN UPDATE SET ...
WHEN MATCHED AND condition THEN DELETE
WHEN NOT MATCHED THEN INSERT ...
WHEN NOT MATCHED THEN ERROR 'message'
```

#### PIVOT/UNPIVOT (Temporal Pivot)
- Syntax: `PIVOT (aggregate_function FOR column IN (value1, value2, ...))`
- Alternative: `PIVOT_WIDER` and `PIVOT_LONGER` keywords
- Supports: `INCLUDE NULLS` / `EXCLUDE NULLS`
- Implementation: Transformed into SELECT with CASE statements
- Internally creates ENUM types for pivot columns

#### Advanced JOIN Types
- **ANTI JOIN**: Returns rows from left with no matches in right
- **SEMI JOIN**: Returns rows from left with any match in right
- **ASOF JOIN**: Temporal/approximate match joins
- **POSITIONAL JOIN**: Row-by-row matching

#### Lambda Functions
- Syntax: `lambda x, y -> x + y` or `(x, y) -> x + y`
- Used in: list operations, array manipulation
- Supports: Multiple parameters, nested lambdas

#### Rich Type System
- **Structured Types**: STRUCT
- **Collection Types**: LIST, MAP
- **Temporal Types**: DATE, TIME, TIMESTAMP, TIMESTAMP_TZ, TIME_TZ, INTERVAL
- **Numeric Types**: TINYINT, SMALLINT, INTEGER, BIGINT, HUGEINT, UHUGEINT, DECIMAL, FLOAT, DOUBLE
- **Text Types**: VARCHAR, CHAR, STRING (with collations)
- **Binary Types**: BLOB, BIT
- **Other Types**: UUID, GEOMETRY, ENUM
- **Unsigned Variants**: UTINYINT, USMALLINT, UINTEGER, UBIGINT

#### Feature Extensions
- **Prepared Statements**: $1, $2 or named parameters
- **Comments**: COMMENT ON statement
- **Secrets**: CREATE SECRET for credential management
- **Extensions**: LOAD, UPDATE EXTENSIONS statements
- **Multiple INSERT Syntax**: RETURNING clauses
- **COPY Statement**: Enhanced CSV/Parquet/JSON support
- **PRAGMA Commands**: DuckDB-specific configurations

#### Named Parameters
- Syntax: `function(param_name := value)`
- Support in aggregate functions with FILTER clause

#### Table Macros
- Syntax: `CREATE MACRO table_name(params) AS SELECT ...`
- Scalar macros also supported

---

## 4. Type System

### 4.1 LogicalTypeId Enum
Core type system with 50+ types:

```cpp
enum class LogicalTypeId : uint8_t {
    // Basic types
    INVALID, SQLNULL, UNKNOWN, ANY, USER, TEMPLATE,
    
    // Integer types
    BOOLEAN, TINYINT, SMALLINT, INTEGER, BIGINT,
    UTINYINT, USMALLINT, UINTEGER, UBIGINT, HUGEINT, UHUGEINT,
    
    // Float types
    FLOAT, DOUBLE,
    
    // Fixed-point
    DECIMAL,
    
    // String types
    VARCHAR, CHAR, STRING_LITERAL, INTEGER_LITERAL,
    
    // Date/Time
    DATE, TIME, TIME_TZ, TIME_NS,
    TIMESTAMP_SEC, TIMESTAMP_MS, TIMESTAMP, TIMESTAMP_NS, TIMESTAMP_TZ,
    INTERVAL,
    
    // Binary
    BLOB, BIT,
    
    // Special types
    UUID, POINTER, VALIDITY, BIGNUM,
    GEOMETRY,
    
    // Composite types (have child types)
    LIST, STRUCT, MAP, ENUM,
    
    // Binding-only types
    STRING_LITERAL, INTEGER_LITERAL
}
```

### 4.2 Type Features
- **Child Types**: STRUCT (named fields), LIST (element type), MAP (key/value types), ENUM (ordered values)
- **Modifiers**: DECIMAL(precision, scale), VARCHAR(n), CHAR(n)
- **Collations**: Support for COLLATE modifiers
- **Casts**: Comprehensive type casting rules
- **Type Aliases**: LONGINT → BIGINT, FLOAT4 → FLOAT, FLOAT8 → DOUBLE, INT → INTEGER

### 4.3 Type Safety
- **Implicit Casts**: Based on cast rules
- **Explicit Casts**: CAST(expr AS type)
- **COLLATE**: Collation specifications
- **NULL handling**: ISNULL, IS NOT NULL operators

---

## 5. Function Registration System

### 5.1 Function Categories

#### Scalar Functions
- **Location**: `src/function/scalar/`
- **Subdirectories**:
  - `string/`: String operations
  - `date/`: Date/time functions
  - `list/`: List manipulation
  - `struct/`: Structure operations
  - `map/`: Map operations
  - `generic/`: Generic functions
  - `sequence/`: Sequence functions
  - `system/`: System functions
  - `operator/`: Operator functions
  - `geometry/`: Geometric functions
  - `variant/`: Variant type functions

#### Aggregate Functions
- **Location**: `src/function/aggregate/`
- **Support**: COUNT, SUM, AVG, MIN, MAX, GROUP_CONCAT, STDDEV, VARIANCE, etc.
- **Features**: DISTINCT, FILTER, ORDER BY (ordered aggregates)

#### Table Functions
- **Location**: `src/function/table/`
- **Purpose**: Functions that return multiple rows
- **Examples**: GENERATE_SERIES, JSON reading, Parquet reading

#### Window Functions
- **Location**: `src/function/window/`
- **Types**: Rank (ROW_NUMBER, RANK, DENSE_RANK), aggregate over windows, LAG/LEAD, FIRST/LAST

#### Pragma Functions
- **Location**: `src/function/pragma/`
- **Purpose**: Database pragmas and configuration

#### Copy Functions
- **Location**: `src/function/copy/`
- **Purpose**: COPY FROM/TO implementations

### 5.2 Function Registration

**Registration Flow**:
```
Function Definition (ScalarFunction, AggregateFunction, etc.)
    ↓
CreateScalarFunctionInfo / CreateAggregateFunctionInfo
    ↓
BuiltinFunctions::AddFunction()
    ↓
Catalog::CreateFunction()
    ↓
FunctionCatalogEntry (in-memory registry)
```

#### ScalarFunction Properties
```cpp
class ScalarFunction {
    string name;
    vector<LogicalType> arguments;
    LogicalType return_type;
    scalar_function_t function;
    bind_scalar_function_t bind;
    LogicalType varargs;
    FunctionStability stability;  // CONSISTENT, VOLATILE, CONSISTENT_WITHIN_QUERY
    FunctionNullHandling null_handling;
    FunctionCollationHandling collation_handling;
    NamedParameterMap named_parameters;
};
```

#### Function Resolution
1. **Function Lookup**: By name and argument count
2. **Overload Resolution**: Type matching and casting
3. **Binding**: Custom bind functions for complex logic
4. **Function Data**: Persistent state across calls
5. **Local State**: Per-thread execution state

### 5.3 Built-in Functions
Over 200+ built-in functions including:
- String: LENGTH, SUBSTR, UPPER, LOWER, TRIM, LTRIM, RTRIM, SPLIT_PART, REGEXP_MATCHES
- Math: ABS, ROUND, CEIL, FLOOR, SIN, COS, TAN, LN, LOG, EXP, POWER, SQRT
- Date: NOW, CURRENT_DATE, DATE_PART, DATE_TRUNC, EXTRACT
- Type Conversion: CAST, TRY_CAST
- Conditional: CASE, COALESCE, NULLIF, IF
- Aggregates: COUNT, SUM, AVG, MIN, MAX, GROUP_CONCAT, STDDEV, VAR_POP, VAR_SAMP

---

## 6. Parser Options and Extensions

### 6.1 ParserOptions
- **max_expression_depth**: Limit expression nesting (default: 1000)
- **preserve_identifier_case**: Keep original identifier casing
- **max_parser_depth**: Prevent stack overflow during parsing
- **accept_errors**: Graceful error handling mode

### 6.2 Parameter Handling
**Prepared Statement Parameters**:
- **Positional**: $1, $2, ... $n
- **Named**: `:name`, `:param`
- **Validation**: Type checking during binding
- **Parameter Map**: Mapping from names to indices

### 6.3 Parser Extensions
- **ParserExtension**: Hook for custom statement parsing
- **Replacement Scans**: Custom scan implementations
- **Custom Functions**: Extension functions
- **Auto-loading**: Extension loading on-demand

---

## 7. Key Implementation Details

### 7.1 Unicode Handling
- **Unicode Spaces**: Normalization of U+00A0, U+2000-U+200B, U+202F, U+205F, U+2060, U+3000
- **Dollar-quoted Strings**: PostgreSQL-compatible `$tag$...$tag$` syntax
- **UTF-8 Support**: Full UTF-8 string handling

### 7.2 Query Location Tracking
- **query_location**: Every parsed element has location info for error reporting
- **Error Context**: Query error context reporting with snippet highlighting

### 7.3 Expression Iterator
- **ParsedExpressionIterator**: Depth-first traversal of expression trees
- **Purpose**: Finding subqueries, parameters, aggregates, window functions

### 7.4 Stack Safety
- **Stack Checking**: Prevents stack overflow in deep expressions
- **Configurable Limit**: max_expression_depth option
- **Context-aware**: Different limits for different expression types

---

## 8. AST to Bound Expression Flow

### 8.1 Transformation Phases

```
Parse Phase (Parser → ParsedExpression)
    ↓ ParsedExpression tree
    
Binding Phase (Binder → Expression)
    - Type resolution
    - Function overload resolution
    - Semantic analysis
    - Collation handling
    - Parameter type deduction
    ↓ Typed Expression tree
    
Planning Phase (Planner → LogicalOperator)
    - Logical plan generation
    - Join reordering
    - Subquery decorrelation
    ↓ Logical plan
    
Optimization Phase (Optimizer)
    - Expression optimization
    - Join order optimization
    - Statistics-based optimization
    ↓ Optimized plan
    
Execution Phase (Executor)
    - Code generation
    - Vector execution
```

### 8.2 Expression Classes
```cpp
enum class ExpressionClass {
    BOUND_AGGREGATE = 1,
    BOUND_CASE = 2,
    BOUND_CAST = 3,
    BOUND_COLUMN_REF = 4,
    BOUND_COMPARISON = 5,
    BOUND_CONJUNCTION = 6,
    BOUND_CONSTANT = 7,
    BOUND_DEFAULT = 8,
    BOUND_FUNCTION = 9,
    BOUND_OPERATOR = 10,
    BOUND_PARAMETER = 11,
    BOUND_REF = 12,
    BOUND_SUBQUERY = 13,
    BOUND_WINDOW = 14,
    BETWEEN = 15,
    CASE = 16,
    CAST = 17,
    COLUMN_REF = 18,
    COMPARISON = 19,
    CONJUNCTION = 20,
    CONSTANT = 21,
    DEFAULT = 22,
    FUNCTION = 23,
    LAMBDA = 24,
    OPERATOR = 25,
    PARAMETER = 26,
    POSITIONAL_REFERENCE = 27,
    STAR = 28,
    SUBQUERY = 29,
    WINDOW = 30,
    COLLATE = 31,
    LAMBDA_REF = 32,
    BOUND_UNNEST = 33,
    BOUND_LAMBDA_REF = 34,
    BOUND_EXPRESSION = 35,
};
```

---

## 9. SQL Feature Support Matrix

| Feature | Status | Notes |
|---------|--------|-------|
| SELECT | Full | With all clauses (WHERE, GROUP BY, HAVING, ORDER BY, LIMIT) |
| INSERT | Full | VALUES, SELECT, DEFAULT VALUES, RETURNING |
| UPDATE | Full | WITH WHERE, SET multiple columns, RETURNING |
| DELETE | Full | WITH WHERE conditions |
| CREATE TABLE | Full | Column constraints, table constraints, temporary tables |
| CREATE VIEW | Full | Materialized views |
| CREATE INDEX | Full | On columns or expressions |
| CREATE FUNCTION | Full | Scalar, aggregate, table functions |
| CREATE MACRO | Full | Scalar and table macros |
| CREATE TYPE | Full | ENUM types |
| CREATE SECRET | Full | For credential management |
| ALTER TABLE | Full | ADD, DROP, RENAME columns; constraints |
| DROP | Full | With CASCADE, RESTRICT, IF EXISTS |
| TRANSACTION | Full | BEGIN, COMMIT, ROLLBACK, SAVEPOINT |
| WITH (CTE) | Full | Recursive and non-recursive |
| UNION | Full | UNION, UNION ALL |
| INTERSECT | Full | Set intersection |
| EXCEPT | Full | Set difference |
| JOIN | Full | All types including ASOF, ANTI, SEMI |
| WINDOW FUNCTIONS | Full | All standard window functions |
| SUBQUERIES | Full | In SELECT, FROM, WHERE clauses |
| LATERAL | Partial | Limited support |
| PIVOT | Full | With ENUM creation |
| UNPIVOT | Full | With INCLUDE/EXCLUDE NULLS |
| MERGE INTO | Full | DuckDB extension |
| COPY | Full | CSV, JSON, Parquet formats |
| EXPLAIN | Full | Query plans and statistics |
| VACUUM | Full | Table optimization |
| PRAGMA | Full | Database configuration |
| Lambda Functions | Full | In higher-order functions |
| CASE | Full | Simple and searched forms |
| CAST | Full | Explicit and implicit casts |
| COLLATE | Full | Collation specifications |

---

## 10. Summary

DuckDB's parser is a sophisticated system built on proven PostgreSQL infrastructure with significant DuckDB-specific enhancements:

### Strengths:
1. **Proven Foundation**: Based on PostgreSQL parser (libpg_query)
2. **Comprehensive SQL Support**: Standard SQL + modern extensions
3. **Type Safety**: Strong typing with implicit/explicit casts
4. **Extensibility**: Functions, macros, custom types, extensions
5. **Innovation**: MERGE INTO, PIVOT/UNPIVOT, ASOF joins, lambda functions
6. **Safety**: Stack checking, error context reporting, parameter validation
7. **Performance**: Lazy evaluation, prepared statements, parameter binding

### Key Design Decisions:
1. Hand-written transformer on top of generated parser allows flexibility
2. Two-phase parsing (PostgreSQL AST → DuckDB AST) provides stability
3. Comprehensive expression types capture SQL semantics
4. Function registration system enables dynamic extension loading
5. Parameter handling supports both positional and named parameters

### Extension Points:
1. Custom functions (scalar, aggregate, table, pragma)
2. Custom types and type implementations
3. Replacement scans for table sources
4. Parser extensions for new statement types
5. Macro functions for code reuse

