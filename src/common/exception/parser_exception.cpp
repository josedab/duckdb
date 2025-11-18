#include "duckdb/common/exception/parser_exception.hpp"
#include "duckdb/common/to_string.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/parser/query_error_context.hpp"
#include "duckdb/parser/hint_generator.hpp"

namespace duckdb {

ParserException::ParserException(const string &msg) : Exception(ExceptionType::PARSER, msg) {
}

ParserException::ParserException(const unordered_map<string, string> &extra_info, const string &msg)
    : Exception(extra_info, ExceptionType::PARSER, msg) {
}

ParserException ParserException::SyntaxError(const string &query, const string &error_message,
                                             optional_idx error_location) {
	auto extra_info = Exception::InitializeExtraInfo("SYNTAX_ERROR", error_location);

	// Format the error with query context (showing line and caret)
	string formatted_message = QueryErrorContext::Format(query, error_message, error_location);

	// Try to extract the error token and generate hints
	string error_token = HintGenerator::ExtractErrorToken(error_message);
	if (!error_token.empty()) {
		// Check if this looks like a misspelled keyword
		auto suggestions = HintGenerator::GetKeywordSuggestions(error_token);
		if (!suggestions.empty()) {
			string hint = HintGenerator::FormatKeywordHint(suggestions);
			formatted_message += "\n" + hint;
			extra_info["hint"] = hint;
			extra_info["suggestions"] = StringUtil::Join(suggestions, ",");
		}
	}

	return ParserException(extra_info, formatted_message);
}
} // namespace duckdb
