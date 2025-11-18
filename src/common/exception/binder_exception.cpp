#include "duckdb/common/exception/binder_exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/function/function.hpp"
#include "duckdb/parser/hint_generator.hpp"

namespace duckdb {

BinderException::BinderException(const string &msg) : Exception(ExceptionType::BINDER, msg) {
}

BinderException::BinderException(const unordered_map<string, string> &extra_info, const string &msg)
    : Exception(extra_info, ExceptionType::BINDER, msg) {
}

BinderException BinderException::ColumnNotFound(const string &name, const vector<string> &similar_bindings,
                                                QueryErrorContext context) {
	auto extra_info = Exception::InitializeExtraInfo("COLUMN_NOT_FOUND", context.query_location);
	string candidate_str = StringUtil::CandidatesMessage(similar_bindings, "Candidate bindings");
	extra_info["name"] = name;
	if (!similar_bindings.empty()) {
		extra_info["candidates"] = StringUtil::Join(similar_bindings, ",");
		return BinderException(extra_info, StringUtil::Format("Referenced column \"%s\" not found in FROM clause!%s",
		                                                      name, candidate_str));
	} else {
		return BinderException(
		    extra_info,
		    StringUtil::Format("Referenced column \"%s\" was not found because the FROM clause is missing", name));
	}
}

BinderException BinderException::NoMatchingFunction(const string &catalog_name, const string &schema_name,
                                                    const string &name, const vector<LogicalType> &arguments,
                                                    const vector<string> &candidates) {
	auto extra_info = Exception::InitializeExtraInfo("NO_MATCHING_FUNCTION", optional_idx());
	// no matching function was found, throw an error
	string call_str = Function::CallToString(catalog_name, schema_name, name, arguments);
	string candidate_str;
	for (auto &candidate : candidates) {
		candidate_str += "\t" + candidate + "\n";
	}
	extra_info["name"] = name;
	if (!catalog_name.empty()) {
		extra_info["catalog"] = catalog_name;
	}
	if (!schema_name.empty()) {
		extra_info["schema"] = schema_name;
	}
	extra_info["call"] = call_str;
	if (!candidates.empty()) {
		extra_info["candidates"] = StringUtil::Join(candidates, ",");
	}
	return BinderException(
	    extra_info,
	    StringUtil::Format("No function matches the given name and argument types '%s'. You might need to add "
	                       "explicit type casts.\n\tCandidate functions:\n%s",
	                       call_str, candidate_str));
}

BinderException BinderException::Unsupported(ParsedExpression &expr, const string &message) {
	auto extra_info = Exception::InitializeExtraInfo("UNSUPPORTED", expr.GetQueryLocation());
	return BinderException(extra_info, message);
}

BinderException BinderException::TableNotFound(const string &name, const vector<string> &similar_tables,
                                               QueryErrorContext context) {
	auto extra_info = Exception::InitializeExtraInfo("TABLE_NOT_FOUND", context.query_location);
	extra_info["name"] = name;

	string message;
	if (similar_tables.empty()) {
		message = StringUtil::Format("Table \"%s\" does not exist", name);
	} else {
		// Get best matches using fuzzy matching
		auto suggestions = HintGenerator::GetSuggestions(name, similar_tables);
		string hint = HintGenerator::FormatIdentifierHint(suggestions);

		extra_info["candidates"] = StringUtil::Join(suggestions, ",");

		message = StringUtil::Format("Table \"%s\" does not exist!\n%s", name, hint);
	}

	return BinderException(extra_info, message);
}

BinderException BinderException::AmbiguousReference(const string &name, const vector<string> &tables,
                                                    QueryErrorContext context) {
	auto extra_info = Exception::InitializeExtraInfo("AMBIGUOUS_REFERENCE", context.query_location);
	extra_info["name"] = name;

	string table_list = StringUtil::Join(tables, ", ");
	extra_info["tables"] = table_list;

	string message = StringUtil::Format(
	    "Ambiguous reference to column \"%s\". Column is present in multiple tables: %s. "
	    "Qualify the column with a table name to resolve the ambiguity.",
	    name, table_list);

	return BinderException(extra_info, message);
}

} // namespace duckdb
