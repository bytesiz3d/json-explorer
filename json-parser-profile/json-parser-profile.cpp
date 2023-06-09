#include <json-parser/json-parser.h>
#include <fstream>

int
main(int argc, char const *argv[])
{
	std::ifstream ifs{PROFILE_CASE_PATH};
	std::string file_content{std::istreambuf_iterator<char>{ifs}, std::istreambuf_iterator<char>{}};

	system("pause");
	auto [json, err] = j_parse(file_content.c_str());

	return 0;
}