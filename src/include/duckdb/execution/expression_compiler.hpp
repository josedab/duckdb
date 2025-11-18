//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/execution/expression_compiler.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/common.hpp"
#include "duckdb/common/types/data_chunk.hpp"
#include "duckdb/common/unordered_map.hpp"
#include "duckdb/planner/expression.hpp"

namespace duckdb {

class ClientContext;
struct ExpressionState;

//! CompiledExpression represents a JIT-compiled expression that can be executed
//! on input data chunks with potentially higher performance than interpreted execution.
class CompiledExpression {
public:
	CompiledExpression();
	virtual ~CompiledExpression();

	//! Execute the compiled expression on the input data chunk
	//! @param input Pointer array to input column data
	//! @param result Pointer to result data
	//! @param count Number of rows to process
	virtual void Execute(data_ptr_t *input, data_ptr_t result, idx_t count);

	//! Execute the compiled expression using DuckDB's vector types
	//! @param input Input data chunk
	//! @param result Output vector
	void Execute(DataChunk &input, Vector &result);

	//! Check if the compiled expression is valid and ready to execute
	bool IsValid() const;

	//! Get the compilation time in milliseconds
	double GetCompilationTimeMs() const;

	//! Get the number of times this compiled expression has been executed
	idx_t GetExecutionCount() const;

	//! Increment execution count (called by executor)
	void IncrementExecutionCount();

	//! Get the expression hash for cache lookup
	hash_t GetExpressionHash() const;

	//! Set the expression hash
	void SetExpressionHash(hash_t hash);

protected:
	//! Whether the compiled expression is valid
	bool is_valid;
	//! Compilation time in milliseconds
	double compilation_time_ms;
	//! Number of executions
	atomic<idx_t> execution_count;
	//! Hash of the original expression tree
	hash_t expression_hash;
	//! Return type of the expression
	LogicalType return_type;
	//! Number of input columns
	idx_t input_count;
};

//! ExpressionCompiler is responsible for compiling expression trees into optimized
//! executable code. This implementation provides the framework for JIT compilation
//! and can be extended with LLVM or other backends.
class ExpressionCompiler {
public:
	explicit ExpressionCompiler(ClientContext &context);
	~ExpressionCompiler();

	//! Compile an expression tree into native code
	//! @param expr The expression to compile
	//! @return A compiled expression, or nullptr if compilation failed
	unique_ptr<CompiledExpression> Compile(const Expression &expr);

	//! Check if an expression can be compiled
	//! @param expr The expression to check
	//! @return true if the expression is compilable
	bool CanCompile(const Expression &expr) const;

	//! Get compilation statistics
	idx_t GetTotalCompilations() const;
	idx_t GetFailedCompilations() const;
	double GetAverageCompilationTimeMs() const;

	//! Check if JIT compilation is enabled
	bool IsJITEnabled() const;

	//! Get the compilation threshold (number of executions before compilation)
	idx_t GetCompilationThreshold() const;

private:
	//! Generate code for an expression (recursive)
	bool GenerateCode(const Expression &expr, CompiledExpression &compiled);

	//! Generate code for specific expression types
	bool GenerateArithmetic(const Expression &expr, CompiledExpression &compiled);
	bool GenerateComparison(const Expression &expr, CompiledExpression &compiled);
	bool GenerateConjunction(const Expression &expr, CompiledExpression &compiled);
	bool GenerateConstant(const Expression &expr, CompiledExpression &compiled);
	bool GenerateCast(const Expression &expr, CompiledExpression &compiled);
	bool GenerateFunction(const Expression &expr, CompiledExpression &compiled);

	//! Hash an expression tree for cache lookup
	hash_t HashExpression(const Expression &expr) const;

	//! Check if a type is supported for compilation
	bool IsSupportedType(const LogicalType &type) const;

private:
	//! Client context
	ClientContext &context;
	//! Total number of compilations
	idx_t total_compilations;
	//! Number of failed compilations
	idx_t failed_compilations;
	//! Total compilation time
	double total_compilation_time_ms;
};

} // namespace duckdb
