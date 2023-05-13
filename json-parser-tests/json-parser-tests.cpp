#define DOCTEST_CONFIG_TREAT_CHAR_STAR_AS_STRING
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <json-parser/json-parser.h>

#include <filesystem>
#include <fstream>

namespace J_JSON_Tests
{
	enum TEST_EXPECT
	{
		EXPECT_PASS,
		EXPECT_FAIL,
	};

	void
	iterate(const char* prefix, TEST_EXPECT expect)
	{
		for (const auto& f: std::filesystem::directory_iterator(TESTS_DIR))
		{
			if (f.is_regular_file() == false)
				continue;

			auto file = f.path().filename();
			if (file.extension() != ".json" || file.string().starts_with(prefix) == false)
				continue;

			SUBCASE(file.string().c_str())
			{
				std::ifstream ifs{f.path()};
				std::string file_content{std::istreambuf_iterator<char>{ifs}, std::istreambuf_iterator<char>{}};

				auto [json, err] = j_parse(file_content.c_str());
				if (expect == EXPECT_PASS)
					CHECK_MESSAGE(!err, std::string_view(err));
				else if (expect == EXPECT_FAIL)
				{
					CAPTURE(file.string().c_str());
					CAPTURE(file_content.c_str());
					CHECK_FALSE(!err);
				}
			}
		}
	}
}

TEST_SUITE("Must Accept")
{
	TEST_CASE("y_array")
	{
		J_JSON_Tests::iterate("y_array", J_JSON_Tests::EXPECT_PASS);
	}
	TEST_CASE("y_number")
	{
		J_JSON_Tests::iterate("y_number", J_JSON_Tests::EXPECT_PASS);
	}
	TEST_CASE("y_object")
	{
		J_JSON_Tests::iterate("y_object", J_JSON_Tests::EXPECT_PASS);
	}
	TEST_CASE("y_string")
	{
		J_JSON_Tests::iterate("y_string", J_JSON_Tests::EXPECT_PASS);
	}
	TEST_CASE("y_structure")
	{
		J_JSON_Tests::iterate("y_structure", J_JSON_Tests::EXPECT_PASS);
	}
}

TEST_SUITE("Must Reject")
{
	TEST_CASE("n_array")
	{
		J_JSON_Tests::iterate("n_array", J_JSON_Tests::EXPECT_FAIL);
	}
	TEST_CASE("n_number")
	{
		J_JSON_Tests::iterate("n_number", J_JSON_Tests::EXPECT_FAIL);
	}
	TEST_CASE("n_object")
	{
		J_JSON_Tests::iterate("n_object", J_JSON_Tests::EXPECT_FAIL);
	}
	TEST_CASE("n_string")
	{
		J_JSON_Tests::iterate("n_string", J_JSON_Tests::EXPECT_FAIL);
	}
	TEST_CASE("n_structure")
	{
		J_JSON_Tests::iterate("n_structure", J_JSON_Tests::EXPECT_FAIL);
	}
}