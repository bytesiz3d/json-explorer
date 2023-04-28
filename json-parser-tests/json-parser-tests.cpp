#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <json-parser/json-parser.h>

#include <filesystem>
#include <fstream>

namespace J_JSON_Tests
{
	void
	iterate(const char* prefix)
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

				auto [object, err] = j_parse(file_content.c_str());
				CHECK_MESSAGE(!err, file.string(), ": ", std::string(err), "\n", file_content);
			}
		}
	}
}

TEST_SUITE("Must Accept")
{
	TEST_CASE("Array")
	{
		J_JSON_Tests::iterate("y_array");
	}
	TEST_CASE("Number")
	{
		J_JSON_Tests::iterate("y_number");
	}
	TEST_CASE("Object")
	{
		J_JSON_Tests::iterate("y_object");
	}
	TEST_CASE("String")
	{
		J_JSON_Tests::iterate("y_string");
	}
	TEST_CASE("Structure")
	{
		J_JSON_Tests::iterate("y_structure");
	}
}
