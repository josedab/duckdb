//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/parser/hint_generator.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/common.hpp"
#include "duckdb/common/vector.hpp"

namespace duckdb {

//! The HintGenerator class provides methods for generating helpful suggestions
//! for common errors in SQL queries, such as typos in keywords or identifiers.
class HintGenerator {
public:
	//! Minimum similarity threshold for keyword suggestions (Jaro-Winkler score)
	static constexpr double KEYWORD_SIMILARITY_THRESHOLD = 0.7;

	//! Minimum similarity threshold for identifier suggestions
	static constexpr double IDENTIFIER_SIMILARITY_THRESHOLD = 0.6;

	//! Maximum number of suggestions to return
	static constexpr idx_t MAX_SUGGESTIONS = 3;

	//! Generate keyword suggestions for a potential typo
	//! Returns a vector of similar SQL keywords sorted by similarity
	static vector<string> GetKeywordSuggestions(const string &token, idx_t max_suggestions = MAX_SUGGESTIONS);

	//! Generate suggestions from a list of candidates
	//! Useful for column names, table names, function names, etc.
	static vector<string> GetSuggestions(const string &name, const vector<string> &candidates,
	                                     idx_t max_suggestions = MAX_SUGGESTIONS,
	                                     double threshold = IDENTIFIER_SIMILARITY_THRESHOLD);

	//! Format suggestions into a hint message
	//! Returns a string like "Did you mean: FROM, FORM?"
	static string FormatKeywordHint(const vector<string> &suggestions);

	//! Format identifier suggestions into a hint message
	//! Returns a string like "Did you mean 'column_name'?"
	static string FormatIdentifierHint(const vector<string> &suggestions);

	//! Check if a token looks like it could be a misspelled keyword
	//! Returns true if the token is close to at least one SQL keyword
	static bool IsPotentialKeywordTypo(const string &token);

	//! Extract the error token from a parser error message
	//! Parser errors often contain "at or near \"TOKEN\"" - this extracts TOKEN
	static string ExtractErrorToken(const string &error_message);

	//! Get the list of common SQL keywords for suggestions
	static vector<string> GetCommonKeywords();
};

} // namespace duckdb
