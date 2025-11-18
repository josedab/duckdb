#include "duckdb/parser/hint_generator.hpp"
#include "catch.hpp"

using namespace duckdb;

TEST_CASE("Test HintGenerator keyword suggestions", "[hint_generator]") {
	// Test common keyword typos
	SECTION("FRON should suggest FROM") {
		auto suggestions = HintGenerator::GetKeywordSuggestions("FRON");
		REQUIRE(!suggestions.empty());
		REQUIRE(suggestions[0] == "FROM");
	}

	SECTION("SELEC should suggest SELECT") {
		auto suggestions = HintGenerator::GetKeywordSuggestions("SELEC");
		REQUIRE(!suggestions.empty());
		REQUIRE(suggestions[0] == "SELECT");
	}

	SECTION("WEHRE should suggest WHERE") {
		auto suggestions = HintGenerator::GetKeywordSuggestions("WEHRE");
		REQUIRE(!suggestions.empty());
		REQUIRE(suggestions[0] == "WHERE");
	}

	SECTION("GROPU should suggest GROUP") {
		auto suggestions = HintGenerator::GetKeywordSuggestions("GROPU");
		REQUIRE(!suggestions.empty());
		REQUIRE(suggestions[0] == "GROUP");
	}

	SECTION("ORDR should suggest ORDER") {
		auto suggestions = HintGenerator::GetKeywordSuggestions("ORDR");
		REQUIRE(!suggestions.empty());
		REQUIRE(suggestions[0] == "ORDER");
	}

	SECTION("ISERT should suggest INSERT") {
		auto suggestions = HintGenerator::GetKeywordSuggestions("ISERT");
		REQUIRE(!suggestions.empty());
		REQUIRE(suggestions[0] == "INSERT");
	}

	SECTION("DELTE should suggest DELETE") {
		auto suggestions = HintGenerator::GetKeywordSuggestions("DELTE");
		REQUIRE(!suggestions.empty());
		REQUIRE(suggestions[0] == "DELETE");
	}

	SECTION("INNR should suggest INNER") {
		auto suggestions = HintGenerator::GetKeywordSuggestions("INNR");
		REQUIRE(!suggestions.empty());
		REQUIRE(suggestions[0] == "INNER");
	}

	SECTION("Case insensitivity - lowercase input") {
		auto suggestions = HintGenerator::GetKeywordSuggestions("fron");
		REQUIRE(!suggestions.empty());
		REQUIRE(suggestions[0] == "FROM");
	}

	SECTION("No suggestions for completely unrelated words") {
		auto suggestions = HintGenerator::GetKeywordSuggestions("xyzabc123");
		REQUIRE(suggestions.empty());
	}
}

TEST_CASE("Test HintGenerator identifier suggestions", "[hint_generator]") {
	vector<string> candidates = {"customer_id", "customer_name", "customer_email", "order_id", "amount"};

	SECTION("Similar column names") {
		auto suggestions = HintGenerator::GetSuggestions("cusomer_id", candidates);
		REQUIRE(!suggestions.empty());
		REQUIRE(suggestions[0] == "customer_id");
	}

	SECTION("Typo in middle of name") {
		auto suggestions = HintGenerator::GetSuggestions("custmer_name", candidates);
		REQUIRE(!suggestions.empty());
		REQUIRE(suggestions[0] == "customer_name");
	}

	SECTION("Multiple similar matches") {
		auto suggestions = HintGenerator::GetSuggestions("customer", candidates);
		REQUIRE(suggestions.size() >= 2);
		// Should suggest multiple customer-related columns
	}

	SECTION("No suggestions for very different names") {
		auto suggestions = HintGenerator::GetSuggestions("xyz123", candidates);
		REQUIRE(suggestions.empty());
	}

	SECTION("Empty candidates list") {
		vector<string> empty_candidates;
		auto suggestions = HintGenerator::GetSuggestions("customer_id", empty_candidates);
		REQUIRE(suggestions.empty());
	}
}

TEST_CASE("Test HintGenerator hint formatting", "[hint_generator]") {
	SECTION("Single suggestion formatting") {
		vector<string> suggestions = {"FROM"};
		string hint = HintGenerator::FormatKeywordHint(suggestions);
		REQUIRE(hint == "Did you mean FROM?");
	}

	SECTION("Two suggestions formatting") {
		vector<string> suggestions = {"FROM", "FORM"};
		string hint = HintGenerator::FormatKeywordHint(suggestions);
		REQUIRE(hint == "Did you mean: FROM or FORM?");
	}

	SECTION("Three suggestions formatting") {
		vector<string> suggestions = {"FROM", "FORM", "FOR"};
		string hint = HintGenerator::FormatKeywordHint(suggestions);
		REQUIRE(hint == "Did you mean: FROM, FORM or FOR?");
	}

	SECTION("Empty suggestions") {
		vector<string> suggestions;
		string hint = HintGenerator::FormatKeywordHint(suggestions);
		REQUIRE(hint.empty());
	}

	SECTION("Identifier hint formatting") {
		vector<string> suggestions = {"customer_id"};
		string hint = HintGenerator::FormatIdentifierHint(suggestions);
		REQUIRE(hint == "Did you mean \"customer_id\"?");
	}

	SECTION("Multiple identifier hints") {
		vector<string> suggestions = {"customer_id", "customer_name"};
		string hint = HintGenerator::FormatIdentifierHint(suggestions);
		REQUIRE(hint == "Did you mean: \"customer_id\" or \"customer_name\"?");
	}
}

TEST_CASE("Test HintGenerator error token extraction", "[hint_generator]") {
	SECTION("Extract from 'at or near' pattern") {
		string error = "syntax error at or near \"FRON\"";
		string token = HintGenerator::ExtractErrorToken(error);
		REQUIRE(token == "FRON");
	}

	SECTION("Extract from 'near' pattern") {
		string error = "syntax error near \"SELECT\"";
		string token = HintGenerator::ExtractErrorToken(error);
		REQUIRE(token == "SELECT");
	}

	SECTION("No token found") {
		string error = "some other error message";
		string token = HintGenerator::ExtractErrorToken(error);
		REQUIRE(token.empty());
	}

	SECTION("Extract longer token") {
		string error = "syntax error at or near \"CUSTOMER_ID\"";
		string token = HintGenerator::ExtractErrorToken(error);
		REQUIRE(token == "CUSTOMER_ID");
	}
}

TEST_CASE("Test HintGenerator potential keyword typo detection", "[hint_generator]") {
	SECTION("FRON is a potential typo") {
		REQUIRE(HintGenerator::IsPotentialKeywordTypo("FRON"));
	}

	SECTION("SELEC is a potential typo") {
		REQUIRE(HintGenerator::IsPotentialKeywordTypo("SELEC"));
	}

	SECTION("Random string is not a typo") {
		REQUIRE_FALSE(HintGenerator::IsPotentialKeywordTypo("xyzabc123"));
	}

	SECTION("Very short strings") {
		// Very short strings may or may not match
		// This tests the threshold behavior
		auto suggestions = HintGenerator::GetKeywordSuggestions("X");
		// Just verify it doesn't crash
	}
}

TEST_CASE("Test HintGenerator common keywords list", "[hint_generator]") {
	auto keywords = HintGenerator::GetCommonKeywords();

	SECTION("Keywords list is not empty") {
		REQUIRE(!keywords.empty());
	}

	SECTION("Contains common SQL keywords") {
		bool has_select = false, has_from = false, has_where = false;
		for (const auto &kw : keywords) {
			if (kw == "SELECT") has_select = true;
			if (kw == "FROM") has_from = true;
			if (kw == "WHERE") has_where = true;
		}
		REQUIRE(has_select);
		REQUIRE(has_from);
		REQUIRE(has_where);
	}
}
