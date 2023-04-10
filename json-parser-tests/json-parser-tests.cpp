#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <json-parser/json-parser.h>

#include <filesystem>
#include <fstream>

TEST_SUITE("Object")
{
	TEST_CASE("Must Accept")
	{
		for (const auto& f : std::filesystem::directory_iterator(TESTS_DIR))
		{
			if (f.is_regular_file() == false)
				continue;

			auto file = f.path().filename();
			if (file.extension() != ".json" || file.string().starts_with("y_object") == false)
				continue;

			SUBCASE(file.string().c_str())
			{
				std::ifstream ifs{f.path()};
				std::string file_content{std::istreambuf_iterator<char>{ifs}, std::istreambuf_iterator<char>{}};

				auto [object, err] = j_parse(file_content.c_str());
				bool is_empty = ::strcmp(err, "") == 0;
				CHECK_MESSAGE(is_empty, file.string(), file_content);
			}
		}
	}
}
