#include "duckdb/parser/hint_generator.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/parser/keyword_helper.hpp"

namespace duckdb {

// Common SQL keywords that users are likely to misspell
// This list focuses on frequently used keywords that have common typos
static const vector<string> COMMON_KEYWORDS = {
    // Data Manipulation
    "SELECT", "FROM", "WHERE", "INSERT", "UPDATE", "DELETE", "INTO", "VALUES",
    "SET", "AND", "OR", "NOT", "IN", "IS", "NULL", "LIKE", "BETWEEN",

    // Joins
    "JOIN", "INNER", "LEFT", "RIGHT", "OUTER", "FULL", "CROSS", "ON", "USING",

    // Grouping and Sorting
    "GROUP", "BY", "HAVING", "ORDER", "ASC", "DESC", "LIMIT", "OFFSET",

    // Set Operations
    "UNION", "INTERSECT", "EXCEPT", "ALL", "DISTINCT",

    // Data Definition
    "CREATE", "ALTER", "DROP", "TABLE", "INDEX", "VIEW", "DATABASE", "SCHEMA",
    "COLUMN", "CONSTRAINT", "PRIMARY", "KEY", "FOREIGN", "REFERENCES", "UNIQUE",
    "DEFAULT", "CHECK", "CASCADE",

    // Data Types
    "INTEGER", "INT", "BIGINT", "SMALLINT", "TINYINT", "DECIMAL", "NUMERIC",
    "FLOAT", "DOUBLE", "REAL", "VARCHAR", "CHAR", "TEXT", "BOOLEAN", "BOOL",
    "DATE", "TIME", "TIMESTAMP", "INTERVAL", "BLOB", "JSON", "UUID",

    // Functions
    "COUNT", "SUM", "AVG", "MIN", "MAX", "COALESCE", "NULLIF", "CAST", "AS",

    // Subqueries and CTEs
    "WITH", "RECURSIVE", "EXISTS", "ANY", "SOME",

    // Conditional
    "CASE", "WHEN", "THEN", "ELSE", "END", "IF",

    // Other common keywords
    "TRUE", "FALSE", "OVER", "PARTITION", "WINDOW", "ROWS", "RANGE",
    "PRECEDING", "FOLLOWING", "CURRENT", "ROW", "UNBOUNDED", "FILTER",
    "QUALIFY", "PIVOT", "UNPIVOT", "RETURNING", "CONFLICT", "NOTHING",
    "REPLACE", "IGNORE", "TEMPORARY", "TEMP", "EXPLAIN", "ANALYZE", "VERBOSE"
};

vector<string> HintGenerator::GetCommonKeywords() {
	return COMMON_KEYWORDS;
}

vector<string> HintGenerator::GetKeywordSuggestions(const string &token, idx_t max_suggestions) {
	// Convert token to uppercase for comparison
	string upper_token = StringUtil::Upper(token);

	// Use Jaro-Winkler to find similar keywords
	auto suggestions = StringUtil::TopNJaroWinkler(COMMON_KEYWORDS, upper_token, max_suggestions, KEYWORD_SIMILARITY_THRESHOLD);

	return suggestions;
}

vector<string> HintGenerator::GetSuggestions(const string &name, const vector<string> &candidates,
                                             idx_t max_suggestions, double threshold) {
	if (candidates.empty()) {
		return {};
	}

	return StringUtil::TopNJaroWinkler(candidates, name, max_suggestions, threshold);
}

string HintGenerator::FormatKeywordHint(const vector<string> &suggestions) {
	if (suggestions.empty()) {
		return "";
	}

	if (suggestions.size() == 1) {
		return "Did you mean " + suggestions[0] + "?";
	}

	string result = "Did you mean: ";
	for (idx_t i = 0; i < suggestions.size(); i++) {
		if (i > 0) {
			if (i == suggestions.size() - 1) {
				result += " or ";
			} else {
				result += ", ";
			}
		}
		result += suggestions[i];
	}
	result += "?";

	return result;
}

string HintGenerator::FormatIdentifierHint(const vector<string> &suggestions) {
	if (suggestions.empty()) {
		return "";
	}

	if (suggestions.size() == 1) {
		return "Did you mean \"" + suggestions[0] + "\"?";
	}

	string result = "Did you mean: ";
	for (idx_t i = 0; i < suggestions.size(); i++) {
		if (i > 0) {
			if (i == suggestions.size() - 1) {
				result += " or ";
			} else {
				result += ", ";
			}
		}
		result += "\"" + suggestions[i] + "\"";
	}
	result += "?";

	return result;
}

bool HintGenerator::IsPotentialKeywordTypo(const string &token) {
	auto suggestions = GetKeywordSuggestions(token, 1);
	return !suggestions.empty();
}

string HintGenerator::ExtractErrorToken(const string &error_message) {
	// Look for pattern: at or near "TOKEN"
	const string pattern1 = "at or near \"";
	size_t pos = error_message.find(pattern1);
	if (pos != string::npos) {
		size_t start = pos + pattern1.length();
		size_t end = error_message.find("\"", start);
		if (end != string::npos) {
			return error_message.substr(start, end - start);
		}
	}

	// Look for pattern: near "TOKEN"
	const string pattern2 = "near \"";
	pos = error_message.find(pattern2);
	if (pos != string::npos) {
		size_t start = pos + pattern2.length();
		size_t end = error_message.find("\"", start);
		if (end != string::npos) {
			return error_message.substr(start, end - start);
		}
	}

	return "";
}

} // namespace duckdb
