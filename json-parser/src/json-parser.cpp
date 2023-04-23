#include "json-parser/json-parser.h"
#include <array>
#include <assert.h>
#include <format>
#include <iostream>
#include <span>
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>

// OPTIMIZE:
// * Profile standard library RAII containers and search for replacements

struct IToken
{
	using Kind = size_t;

	virtual Kind
	kind() const = 0;

	virtual std::string_view
	data() const = 0;

	virtual bool
	is_terminal() const = 0;

	virtual bool
	is_equal(IToken* other) const = 0;
};

template<>
struct std::formatter<IToken*> : std::formatter<std::string>
{
	auto
	format(IToken* tkn, format_context& ctx)
	{
		return std::format_to(ctx.out(), "{}", tkn->data());
	}
};

template<>
struct std::formatter<std::span<IToken* const>> : std::formatter<std::string>
{
	auto
	format(const auto& span, format_context& ctx)
	{
		std::format_to(ctx.out(), "[");
		for (const auto& tkn : span)
			std::format_to(ctx.out(), " {}", tkn);

		return std::format_to(ctx.out(), " ]");
	}
};

struct Production
{
	IToken* lhs;
	std::vector<IToken*> rhs;
	std::string_view error;

	constexpr static Production
	Error(std::string_view message)
	{
		Production error{};
		error.error = message;
		return error;
	}

	bool
	is_error() const
	{
		return error.empty() == false;
	}
};

template<>
struct std::formatter<Production> : std::formatter<std::string>
{
	auto
	format(const Production& production, format_context& ctx)
	{
		std::span<IToken* const> span(production.rhs.begin(), production.rhs.end());
		return std::format_to(ctx.out(), "{} -> {}", production.lhs, span);
	}
};

struct PTable
{
	std::unordered_map<IToken::Kind, std::unordered_map<IToken::Kind, Production>> table;

	void
	add(IToken::Kind nonterminal, IToken::Kind terminal, const Production& production)
	{
		table[nonterminal][terminal] = production;
	}

	Production
	operator()(IToken::Kind nonterminal, IToken::Kind terminal) const
	{
		assert(table.contains(nonterminal));

		auto& productions = table.at(nonterminal);

		if (productions.contains(terminal) == false)
			return Production::Error("Unexpected terminal");

		return productions.at(terminal);
	}

	Production
	operator()(IToken* nonterminal, IToken* terminal) const
	{
		return this->operator()(nonterminal->kind(), terminal->kind());
	}
};

struct LL_Parse_Error
{
	std::string_view err;
	explicit operator bool() { return err.empty() == false; }
	bool operator==(bool v) { return bool(*this) == v; }
	bool operator!=(bool v) { return bool(*this) != v; }
};

template<typename Fn, typename... Args>
inline static LL_Parse_Error
ll_parse(std::span<IToken*> tokens, const PTable& parsing_table, Fn&& fn, Args&&... args)
{
	std::stack<IToken*> stack{};
	auto it = tokens.begin();

	while (stack.empty() == false)
	{
		IToken* input = *it;

		if (input == stack.top())
		{
			stack.pop();
			it++;
		}
		else if (stack.top()->is_terminal())
		{
			return {"Unexpected terminal"};
		}
		else if (auto production = parsing_table(stack.top(), input); production.is_error())
		{
			return {production.error};
		}
		else
		{
			fn(production, args...);
			stack.pop();
			for (auto rhs = production.rhs.rbegin(); rhs != production.rhs.rend(); rhs++)
				stack.push(*rhs);
		}
	}
	return {};
}


struct JSON_Token : IToken
{
	enum KIND : IToken::Kind
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

		T_onenine, // data is relevant
		T_unicode, // data is relevant

		N_START = 0x100,
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

	virtual Kind
	kind() const
	{
		return IToken::Kind{_kind};
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

		case T_unicode:
		case T_onenine: return _data;

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
		case T_onenine:
		case T_e:
		case T_E:
		case T_minus:
		case T_plus:
		case T_dot:
		case T_colon:
		case T_unicode:
			return true;

		default: return false;
		}
	}

	virtual bool
	is_equal(IToken* other) const
	{
		JSON_Token* j = (JSON_Token*)other;

		if (is_terminal() != j->is_terminal())
			return false;

		if (kind() != j->kind())
			return false;

		if (is_terminal() == false)
			return true;

		switch (kind())
		{
		case T_onenine:
		case T_unicode:
			return data() == j->data();

		default: return true;
		}
	}
};

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

inline static std::span<IToken*>
JSON_lex(std::string_view json_string)
{
	assert(false && "TODO");
	return {};
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
	auto parse_err = ll_parse(JSON_lex(json_string), JSON_PTable(), [](const Production& production) {
		std::cout << std::format("{}", production) << std::endl;
	});

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
