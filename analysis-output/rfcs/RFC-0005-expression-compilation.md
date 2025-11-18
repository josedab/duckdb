# RFC-0005: JIT Expression Compilation

**Status:** Draft
**Author:** Claude Code Analysis
**Created:** November 18, 2025
**Commit Reference:** `52a07d06eafb63b60ed6275a494b85f93be4b986`

---

## Summary

Implement just-in-time (JIT) compilation for frequently executed expressions, converting the interpreted expression tree into native machine code for significant performance improvements on expression-heavy queries.

---

## Motivation

DuckDB's ExpressionExecutor interprets expression trees at runtime:

```cpp
// Current: Interpreted execution
for each expression node:
    switch (node.type):
        case ADD: return left + right
        case MULTIPLY: return left * right
        ...
```

This interpretation overhead is significant for:
- Complex expressions with many operations
- Expressions executed millions of times
- Projection-heavy queries

### Performance Impact

For expression `(a + b) * c + d`:
- **Interpreted**: ~20 instructions per value (switches, indirections)
- **Compiled**: ~4 instructions per value (direct arithmetic)
- **Speedup**: 3-5x for expression evaluation

---

## Detailed Design

### 1. Compilation Framework

```cpp
// src/include/duckdb/execution/expression_compiler.hpp
class ExpressionCompiler {
public:
    // Compile expression to native code
    CompiledExpression Compile(Expression &expr);

    // Check if expression is compilable
    bool CanCompile(Expression &expr);

private:
    // LLVM context for code generation
    unique_ptr<LLVMContext> context;
    unique_ptr<Module> module;
    unique_ptr<IRBuilder<>> builder;
};

class CompiledExpression {
public:
    // Execute compiled code
    void Execute(DataChunk &input, Vector &result);

private:
    // Function pointer to compiled code
    using ExprFunc = void(*)(data_ptr_t*, data_ptr_t, idx_t);
    ExprFunc compiled_function;
};
```

### 2. IR Generation

Generate LLVM IR for expressions:

```cpp
// src/execution/expression_compiler.cpp
Value *ExpressionCompiler::GenerateIR(Expression &expr) {
    switch (expr.type) {
        case ExpressionType::VALUE_CONSTANT:
            return ConstantInt::get(context, expr.value);

        case ExpressionType::BOUND_COLUMN_REF:
            return LoadColumn(expr.column_index);

        case ExpressionType::OPERATOR_ADD:
            return builder->CreateAdd(
                GenerateIR(*expr.children[0]),
                GenerateIR(*expr.children[1])
            );

        case ExpressionType::OPERATOR_MULTIPLY:
            return builder->CreateMul(
                GenerateIR(*expr.children[0]),
                GenerateIR(*expr.children[1])
            );

        case ExpressionType::COMPARE_GREATERTHAN:
            return builder->CreateICmpSGT(
                GenerateIR(*expr.children[0]),
                GenerateIR(*expr.children[1])
            );
    }
}
```

### 3. Vectorized Code Generation

Generate SIMD-optimized loops:

```cpp
void ExpressionCompiler::GenerateVectorizedLoop(
    Expression &expr, Function *func) {

    // Create loop structure
    auto entry = BasicBlock::Create(context, "entry", func);
    auto loop = BasicBlock::Create(context, "loop", func);
    auto exit = BasicBlock::Create(context, "exit", func);

    // Entry: setup
    builder->SetInsertPoint(entry);
    auto count = func->getArg(2);
    builder->CreateBr(loop);

    // Loop body
    builder->SetInsertPoint(loop);
    auto idx = builder->CreatePHI(Type::getInt64Ty(context), 2);
    idx->addIncoming(ConstantInt::get(context, 0), entry);

    // Generate expression with SIMD intrinsics if available
    auto result = GenerateIR(expr);
    StoreResult(result, idx);

    // Loop increment
    auto next_idx = builder->CreateAdd(idx, ConstantInt::get(context, 1));
    idx->addIncoming(next_idx, loop);

    // Loop condition
    auto done = builder->CreateICmpULT(next_idx, count);
    builder->CreateCondBr(done, loop, exit);

    // Exit
    builder->SetInsertPoint(exit);
    builder->CreateRetVoid();
}
```

### 4. Compilation Cache

Cache compiled expressions for reuse:

```cpp
// src/include/duckdb/execution/compilation_cache.hpp
class CompilationCache {
public:
    // Get or compile expression
    CompiledExpression &GetOrCompile(Expression &expr);

    // Cache management
    void Evict(idx_t target_size);
    idx_t GetCacheSize();

private:
    // Hash expression tree for cache key
    hash_t HashExpression(Expression &expr);

    // LRU cache
    LRUCache<hash_t, CompiledExpression> cache;
};
```

### 5. Adaptive Compilation

Compile only hot expressions:

```cpp
// src/execution/expression_executor.cpp
void ExpressionExecutor::Execute(Expression &expr,
                                  DataChunk &input,
                                  Vector &result) {
    // Track execution count
    expr.execution_count++;

    // Compile if hot
    if (expr.execution_count > COMPILATION_THRESHOLD &&
        !expr.compiled &&
        compiler.CanCompile(expr)) {

        expr.compiled = compiler.Compile(expr);
    }

    // Execute compiled or interpreted
    if (expr.compiled) {
        expr.compiled->Execute(input, result);
    } else {
        ExecuteInterpreted(expr, input, result);
    }
}
```

### 6. NULL Handling

Generate code for NULL propagation:

```cpp
Value *ExpressionCompiler::GenerateWithNulls(Expression &expr) {
    auto left = GenerateIR(*expr.children[0]);
    auto right = GenerateIR(*expr.children[1]);

    // Load validity masks
    auto left_valid = LoadValidity(expr.children[0]);
    auto right_valid = LoadValidity(expr.children[1]);

    // Compute result validity
    auto result_valid = builder->CreateAnd(left_valid, right_valid);

    // Compute result (with select to avoid UB on invalid)
    auto result = builder->CreateAdd(left, right);
    result = builder->CreateSelect(result_valid, result,
                                   ConstantInt::get(context, 0));

    return result;
}
```

---

## Example Usage

### Automatic Compilation

```sql
-- Expression-heavy query
SELECT
    (price * quantity) * (1 - discount) * (1 + tax),
    CASE WHEN status = 1 THEN 'Active' ELSE 'Inactive' END
FROM orders;

-- First execution: interpreted
-- Subsequent executions: compiled (after threshold)
```

### Monitoring Compilation

```sql
-- View compilation statistics
SELECT * FROM duckdb_compilation_cache();
-- expression_hash | compile_time_ms | executions | speedup
-- 0x1234abcd      | 5.2             | 1000000    | 3.5x

-- Control compilation
SET jit_compilation_threshold = 10000;
SET jit_enabled = true;
```

---

## Implementation Plan

### Phase 1: Infrastructure (Week 1-2)
- Integrate LLVM
- Basic IR generation for arithmetic
- Compilation cache
- Unit tests

### Phase 2: Core Expressions (Week 3-4)
- Arithmetic operators
- Comparison operators
- Logical operators
- Type casts

### Phase 3: Advanced (Week 5-6)
- NULL handling
- String operations
- CASE expressions
- Function calls

### Phase 4: Optimization (Week 7-8)
- SIMD generation
- Adaptive thresholds
- Benchmarking
- Documentation

---

## Backwards Compatibility

### Behavior
- Results identical to interpreted execution
- Query plans unchanged
- Fallback to interpreted if compilation fails

### Performance
- Slight overhead for compilation (amortized)
- Significant speedup for hot expressions
- Memory usage for compiled code cache

### Configuration

```sql
-- Disable JIT
SET jit_enabled = false;

-- Adjust threshold
SET jit_compilation_threshold = 50000;

-- Cache size
SET jit_cache_size = '100MB';
```

---

## Alternatives Considered

### Alternative 1: Hand-Written Specializations
- Pro: No LLVM dependency
- Con: Combinatorial explosion of cases

### Alternative 2: WebAssembly Compilation
- Pro: Portable
- Con: Slower than native

### Alternative 3: Ahead-of-Time Compilation
- Pro: No runtime overhead
- Con: Can't adapt to actual expressions

**Decision:** JIT with LLVM provides best performance with manageable complexity.

---

## Open Questions

1. **LLVM dependency**: Bundle or require system? (Proposed: Bundle minimal LLVM)
2. **Compilation threshold**: Default value? (Proposed: 10000 executions)
3. **Cache eviction**: LRU or frequency-based? (Proposed: LRU with size limit)

---

## Success Criteria

- [ ] 50% speedup for expression-heavy queries
- [ ] < 10ms compilation time for typical expressions
- [ ] No correctness regressions
- [ ] Graceful fallback on unsupported expressions
- [ ] Documentation complete

---

## Effort Estimation

**Total: 8 weeks (40 developer-days)**
- Infrastructure: 10 days
- Core expressions: 10 days
- Advanced features: 10 days
- Optimization: 10 days

**Risk: High** - Complex feature with LLVM dependency. Requires careful testing and benchmarking.
