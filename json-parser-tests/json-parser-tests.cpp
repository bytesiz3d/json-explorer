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
					CHECK_MESSAGE(!err, err);
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

TEST_SUITE("Manual")
{
	// REF: https://developer.spotify.com/documentation/web-api/reference/get-an-album
	TEST_CASE("Dump")
	{
		auto [obj, err] = j_parse(R"(
		{
		  "album_type": "compilation",
		  "total_tracks": 9,
		  "available_markets": [
			"CA",
			"BR",
			"IT"
		  ],
		  "external_urls": {
			"spotify": "string"
		  },
		  "href": "string",
		  "id": "2up3OPMp9Tb4dAKM2erWXQ",
		  "images": [
			{
			  "url": "https://i.scdn.co/image/ab67616d00001e02ff9ca10b55ce82ae553c8228",
			  "height": 300,
			  "width": 300
			}
		  ],
		  "name": "string",
		  "release_date": "1981-12",
		  "release_date_precision": "year",
		  "restrictions": {
			"reason": "market"
		  },
		  "type": "album",
		  "uri": "spotify:album:2up3OPMp9Tb4dAKM2erWXQ",
		  "copyrights": [
			{
			  "text": "string",
			  "type": "string"
			}
		  ],
		  "external_ids": {
			"isrc": "string",
			"ean": "string",
			"upc": "string"
		  },
		  "genres": [
			"Egg punk",
			"Noise rock"
		  ],
		  "label": "string",
		  "popularity": 0,
		  "artists": [
			{
			  "external_urls": {
				"spotify": "string"
			  },
			  "followers": {
				"href": "string",
				"total": 0
			  },
			  "genres": [
				"Prog rock",
				"Grunge"
			  ],
			  "href": "string",
			  "id": "string",
			  "images": [
				{
				  "url": "https://i.scdn.co/image/ab67616d00001e02ff9ca10b55ce82ae553c8228",
				  "height": 300,
				  "width": 300
				}
			  ],
			  "name": "string",
			  "popularity": 0,
			  "type": "artist",
			  "uri": "string"
			}
		  ],
		  "tracks": {
			"href": "https://api.spotify.com/v1/me/shows?offset=0&limit=20",
			"limit": 20,
			"next": "https://api.spotify.com/v1/me/shows?offset=1&limit=1",
			"offset": 0,
			"previous": "https://api.spotify.com/v1/me/shows?offset=1&limit=1",
			"total": 4,
			"items": [
			  {
				"artists": [
				  {
					"external_urls": {
					  "spotify": "string"
					},
					"href": "string",
					"id": "string",
					"name": "string",
					"type": "artist",
					"uri": "string"
				  }
				],
				"available_markets": [
				  "string"
				],
				"disc_number": 0,
				"duration_ms": 0,
				"explicit": false,
				"external_urls": {
				  "spotify": "string"
				},
				"href": "string",
				"id": "string",
				"is_playable": false,
				"linked_from": {
				  "external_urls": {
					"spotify": "string"
				  },
				  "href": "string",
				  "id": "string",
				  "type": "string",
				  "uri": "string"
				},
				"restrictions": {
				  "reason": "string"
				},
				"name": "string",
				"preview_url": "string",
				"track_number": 0,
				"type": "string",
				"uri": "string",
				"is_local": false
			  }
			]
		  }
		})");

		CHECK_MESSAGE(!err, err);
		::puts(j_dump(obj));
	}
}