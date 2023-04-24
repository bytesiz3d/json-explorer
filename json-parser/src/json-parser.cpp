#include "json-parser/json-parser.h"

#include <array>
#include <format>
#include <iostream>
#include <span>
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>

#include <assert.h>
#include <utf8proc.h>

// OPTIMIZE:
// * Profile standard library RAII containers and search for replacements

struct Error
{
	std::string_view err;

	explicit operator bool() { return err.empty() == false; }

	bool
	operator==(bool v)
	{
		return bool(*this) == v;
	}

	bool
	operator!=(bool v)
	{
		return bool(*this) != v;
	}
};

template<typename T>
struct Result
{
	T val;
	Error err;

	Result(Error e) : err(e)
	{
	}


	template<typename... TArgs>
	Result(TArgs&&... args) : val(std::forward<TArgs>(args)...), err{}
	{
	}

	Result(const Result&) = delete;

	Result(Result&&) = default;

	Result&
	operator=(const Result&) = delete;

	Result&
	operator=(Result&&) = default;

	~Result() = default;
};


using Rune = utf8proc_int32_t;

Result<Rune>
utf8_read(std::string_view str, std::string_view::const_iterator& it)
{
	Rune r{};
	auto ptr = (utf8proc_uint8_t*)(&*it);
	auto len = str.end() - it;

	auto bytes_read = utf8proc_iterate(ptr, len, &r);
	if (bytes_read < 0)
		return Error{utf8proc_errmsg(bytes_read)};

	it += bytes_read;
	return r;
}

template<typename T>
struct std::formatter<std::span<T>> : std::formatter<std::string>
{
	auto
	format(const auto& span, format_context& ctx)
	{
		std::format_to(ctx.out(), "[");
		for (auto tkn : span)
			std::format_to(ctx.out(), " {}", tkn);

		return std::format_to(ctx.out(), " ]");
	}
};

struct JSON_Token
{
	enum KIND : Rune
	{
		META_NIL = 0, // default
		META_EPS,

		T_null,
		T_true,
		T_false,

		T_comma = ',',
		T_lbracket = '[',
		T_rbracket = ']',
		T_lbrace = '{',
		T_rbrace = '}',
		T_quote = '"',
		T_zero = '0',
		T_e = 'e',
		T_E = 'E',
		T_minus = '-',
		T_plus = '+',
		T_dot = '.',
		T_colon = ':',
		T_backslash = '\\',

		T_onenine = 0x100, // data is relevant
		T_unicode,         // data is relevant
		T_escaped,         // data is relevant

		N_START = 0x1000,
		N_END_OF_INPUT,

		N_V,

		N_OBJECT,
		N_MEMBERS,
		N_MEMBER,

		N_ARRAY,
		N_ELEMENTS,

		N_STRING,
		N_CHARS,
		N_CHAR,

		N_NUMBER,
		N_INTEGER,
		N_DIGITS,
		N_FRACTION,
		N_EXPONENT,
		N_SIGN,
	} _kind;

	// OPTIMIZE: Maybe string_view
	std::string _data;

	JSON_Token() = default;
	JSON_Token(KIND kind, std::string_view data = {}) : _kind((KIND)kind), _data(data)
	{
	}

	virtual KIND
	kind() const
	{
		return _kind;
	}

	virtual std::string_view
	data() const
	{
		switch (_kind)
		{
		case META_NIL:
		case META_EPS: assert(false && "unreachable");

		case T_null: return "null";
		case T_true: return "true";
		case T_false: return "false";

		case T_comma: return "'";
		case T_lbracket: return "[";
		case T_rbracket: return "]";
		case T_lbrace: return "{";
		case T_rbrace: return "}";
		case T_quote: return "\"";
		case T_zero: return "0";
		case T_e: return "e";
		case T_E: return "E";
		case T_minus: return "-";
		case T_plus: return "+";
		case T_dot: return ".";
		case T_colon: return ":";
		case T_backslash: return "\\";

		case T_unicode:
		case T_onenine:
		case T_escaped:
			return _data;

		case N_START: return "<S>";
		case N_END_OF_INPUT: return "<$>";

		case N_V: return "<V>";

		case N_OBJECT: return "<OBJECT>";
		case N_MEMBERS: return "<MEMBERS>";
		case N_MEMBER: return "<MEMBER>";

		case N_ARRAY: return "<ARRAY>";
		case N_ELEMENTS: return "<ELEMENTS>";

		case N_STRING: return "<STRING>";
		case N_CHARS: return "<CHARS>";
		case N_CHAR: return "<CHAR>";

		case N_NUMBER: return "<NUMBER>";
		case N_INTEGER: return "<INTEGER>";
		case N_DIGITS: return "<DIGITS>";
		case N_FRACTION: return "<FRACTION>";
		case N_EXPONENT: return "<EXPONENT>";
		case N_SIGN: return "<SIGN>";
		}
	}

	virtual bool
	is_terminal() const
	{
		switch (_kind)
		{
		case META_NIL:
		case META_EPS:
			assert(false && "UNREACHABLE");

		case T_null:
		case T_true:
		case T_false:
		case T_comma:
		case T_lbracket:
		case T_rbracket:
		case T_lbrace:
		case T_rbrace:
		case T_quote:
		case T_zero:
		case T_e:
		case T_E:
		case T_minus:
		case T_plus:
		case T_dot:
		case T_colon:
		case T_backslash:

		case T_onenine:
		case T_unicode:
		case T_escaped:
			return true;
		}

		return false;
	}

	virtual bool
	is_equal(const JSON_Token& other) const
	{
		if (is_terminal() != other.is_terminal())
			return false;

		if (kind() != other.kind())
			return false;

		if (is_terminal() == false)
			return true;

		switch (kind())
		{
		case T_onenine:
		case T_unicode:
		case T_escaped:
			return data() == other.data();

		default: return true;
		}
	}
};

template<>
struct std::formatter<JSON_Token> : std::formatter<std::string>
{
	auto
	format(const JSON_Token& tkn, format_context& ctx)
	{
		return std::format_to(ctx.out(), "{}", tkn.data());
	}
};

struct Production
{
	JSON_Token lhs;
	std::vector<JSON_Token> rhs;
};

template<>
struct std::formatter<Production> : std::formatter<std::string>
{
	auto
	format(const Production& production, format_context& ctx)
	{
		std::span<const JSON_Token> span(production.rhs.begin(), production.rhs.end());
		return std::format_to(ctx.out(), "{} -> {}", production.lhs, span);
	}
};

struct PTable
{
	std::unordered_map<JSON_Token::KIND, std::unordered_map<JSON_Token::KIND, Production>> table;

	void
	add(JSON_Token::KIND nonterminal, JSON_Token::KIND terminal, const Production& production)
	{
		table[nonterminal][terminal] = production;
	}

	Result<Production>
	operator()(JSON_Token::KIND nonterminal, JSON_Token::KIND terminal) const
	{
		assert(table.contains(nonterminal));

		auto& productions = table.at(nonterminal);

		if (productions.contains(terminal) == false)
			return Error{"Unexpected terminal"};

		return productions.at(terminal);
	}

	Result<Production>
	operator()(const JSON_Token& nonterminal, const JSON_Token& terminal) const
	{
		return this->operator()(nonterminal.kind(), terminal.kind());
	}
};

inline static Error
_parse(std::span<JSON_Token> tokens, const PTable& parsing_table)
{
	std::stack<JSON_Token> stack{};
	auto it = tokens.begin();

	while (stack.empty() == false)
	{
		JSON_Token input = *it;

		if (input.is_equal(stack.top()))
		{
			stack.pop();
			it++;
		}
		else if (stack.top().is_terminal())
		{
			return {"Unexpected terminal"};
		}
		else if (auto [production, err] = parsing_table(stack.top(), input); err)
		{
			return err;
		}
		else
		{
			// do something with the production
			stack.pop();
			for (auto rhs = production.rhs.rbegin(); rhs != production.rhs.rend(); rhs++)
				stack.push(*rhs);
		}
	}

	return {};
}

inline static PTable&
JSON_PTable()
{
	// Lazy init table
	static PTable* _table = nullptr;
	if (_table != nullptr)
		return *_table;

	static PTable table{};
	_table = &table;

	// TODO: Fill table
	assert(false && "TODO");
	return *_table;
}

inline static Result<std::vector<JSON_Token>>
JSON_lex(std::string_view json_string)
{
	std::vector<JSON_Token> tokens{};

	for (auto it = json_string.begin(); it != json_string.end();)
	{
		auto [rune, err] = utf8_read(json_string, it);
		if (err)
			return err;

		assert(false && "TODO");
	}

	return tokens;
}

#pragma section("API")

J_Version
j_version()
{
	return {0, 1, 0};
}

J_Parse_Result
j_parse(const char* json_string)
{
	auto table = JSON_PTable();

	auto [json_tokens, tokens_err] = JSON_lex(json_string);
	if (tokens_err)
		return {J_JSON{}, tokens_err.err.data()};

	auto parse_err = _parse(json_tokens, table);
	if (parse_err)
		return {J_JSON{}, parse_err.err.data()};

	return {J_JSON{}, "TODO"};
}

J_Bool
j_get_J_Bool(J_JSON json)
{
	assert(json.kind == J_JSON_BOOL);
	return {};
}

J_Number
j_get_J_Number(J_JSON json)
{
	assert(json.kind == J_JSON_NUMBER);
	return {};
}

J_String
j_get_J_String(J_JSON json)
{
	assert(json.kind == J_JSON_STRING);
	return {};
}

J_Array
j_get_J_Array(J_JSON json)
{
	assert(json.kind == J_JSON_ARRAY);
	return {};
}

J_Object
j_get_J_Object(J_JSON json)
{
	assert(json.kind == J_JSON_OBJECT);
	return {};
}
