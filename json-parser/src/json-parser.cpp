#include "json-parser/json-parser.h"
#include "Base.h"
#include "utf8proc.h"

#include <array>
#include <format>
#include <initializer_list>
#include <iostream>
#include <span>
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>

#include <assert.h>

// OPTIMIZE:
// * Profile standard library RAII containers and search for replacements

struct PTable;

struct JSON_Token
{
	enum KIND : Rune
	{
		META_NIL = 0, // default
		META_EPS,
		META_START,
		META_END_OF_INPUT,

		T_null,
		T_true,
		T_false,

		T_comma     = ',',
		T_lbracket  = '[',
		T_rbracket  = ']',
		T_lbrace    = '{',
		T_rbrace    = '}',
		T_quote     = '"',
		T_zero      = '0',
		T_e         = 'e',
		T_E         = 'E',
		T_minus     = '-',
		T_plus      = '+',
		T_dot       = '.',
		T_colon     = ':',
		T_backslash = '\\',

		T_onenine = 0x1000, // data is relevant
		T_unicode,          // data is relevant
		T_escaped,          // data is relevant

		N_V = 0x10000,

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
		N_DIGIT,
		N_DIGITS,
		N_FRACTION,
		N_EXPONENT,
		N_SIGN,
	} _kind;

	// OPTIMIZE: Maybe string_view
	std::string _data;

	JSON_Token() = default;
	JSON_Token(Rune kind, std::string_view data = {}) : _kind((KIND)kind), _data(data)
	{
	}

	KIND
	kind() const
	{
		return _kind;
	}

	std::string_view
	data() const
	{
		switch (_kind)
		{
		case META_NIL:          unreachable("TOKEN_KIND");
		case META_EPS:          return "<EPS>";
		case META_START:        return "<S>";
		case META_END_OF_INPUT: return "<$>";

		case T_null: return "null";
		case T_true: return "true";
		case T_false: return "false";

		case T_comma:     return ",";
		case T_lbracket:  return "[";
		case T_rbracket:  return "]";
		case T_lbrace:    return "{";
		case T_rbrace:    return "}";
		case T_quote:     return "\"";
		case T_zero:      return "0";
		case T_e:         return "e";
		case T_E:         return "E";
		case T_minus:     return "-";
		case T_plus:      return "+";
		case T_dot:       return ".";
		case T_colon:     return ":";
		case T_backslash: return "\\";

		case T_unicode:
		case T_onenine:
		case T_escaped:
			return _data;

		case N_V: return "<V>";

		case N_OBJECT:  return "<OBJECT>";
		case N_MEMBERS: return "<MEMBERS>";
		case N_MEMBER:  return "<MEMBER>";

		case N_ARRAY:    return "<ARRAY>";
		case N_ELEMENTS: return "<ELEMENTS>";

		case N_STRING: return "<STRING>";
		case N_CHARS:  return "<CHARS>";
		case N_CHAR:   return "<CHAR>";

		case N_NUMBER:   return "<NUMBER>";
		case N_INTEGER:  return "<INTEGER>";
		case N_DIGIT:    return "<DIGIT>";
		case N_DIGITS:   return "<DIGITS>";
		case N_FRACTION: return "<FRACTION>";
		case N_EXPONENT: return "<EXPONENT>";
		case N_SIGN:     return "<SIGN>";
		}
	}

	bool
	is_terminal() const
	{
		switch (_kind)
		{
		case META_NIL:
		case META_EPS:
			unreachable("TOKEN_KIND");

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

		case META_END_OF_INPUT:
			return true;
		}

		return false;
	}

	bool
	is_equal(const JSON_Token& other) const
	{
		return is_terminal() == other.is_terminal()
			&& kind() == other.kind();
	}

	static void
	fill_ptable(PTable& ptable);
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
	add(JSON_Token::KIND nonterminal, JSON_Token::KIND terminal, std::initializer_list<JSON_Token> rhs)
	{
		assert(table[nonterminal].contains(terminal) == false);
		table[nonterminal][terminal] = Production{nonterminal, rhs};
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

PTable&
JSON_PTable()
{
	static PTable* ptable = nullptr;
	if (ptable == nullptr)
	{
		static PTable _table{};
		ptable = &_table;
		JSON_Token::fill_ptable(*ptable);
	}
	return *ptable;
}

struct Parser
{
	std::span<JSON_Token> _tokens;
	std::span<JSON_Token>::iterator _it;
	PTable& _ptable{JSON_PTable()};

	Parser() = default;
	Parser(std::span<JSON_Token> tokens) : _tokens(tokens), _it(_tokens.begin()), _ptable(JSON_PTable()) {}

	Result<J_JSON>
	parse()
	{
		std::stack<JSON_Token> stack{};
		stack.push(JSON_Token::META_START);

		while (stack.empty() == false)
		{
			JSON_Token input_terminal = *_it;
			if (input_terminal.is_equal(stack.top()))
			{
				stack.pop();
				_it++;
			}
			else if (stack.top().is_terminal())
			{
				return Error{"Unexpected terminal"};
			}
			else if (auto [production, err] = _ptable(stack.top(), input_terminal); err)
			{
				return err;
			}
			else
			{
				// do something with the production
				std::cout << std::format("{}\n", production);
				stack.pop();
				for (auto rhs = production.rhs.rbegin(); rhs != production.rhs.rend(); rhs++)
				{
				 	if (rhs->kind() != JSON_Token::META_EPS)
						stack.push(*rhs);
				}
			}
		}
		return {};
	}
};

struct Lexer
{
	std::string_view _string;

	enum STATE
	{
		STATE_0,

		STATE_T,
		STATE_TR,
		STATE_TRU,

		STATE_F,
		STATE_FA,
		STATE_FAL,
		STATE_FALS,

		STATE_N,
		STATE_NU,
		STATE_NUL,

		STATE_STRING,
		STATE_BACKSLASH,
	};

	std::stack<STATE> _state_stack;
	std::vector<JSON_Token> _tokens;

	Lexer() = default;
	Lexer(std::string_view string) : _string(string), _state_stack{}, _tokens{}
	{
		_state_stack.push(STATE_0);
	}

	Result<bool>
	try_to_parse_singlechar_terminal(Rune r)
	{
		if (_state_stack.top() != STATE_0)
			return false;

		if (::isgraph(r) == false) // skip whitespace
			return true;

		if (::isdigit(r) && r != '0')
		{
			char data[]{char(r)};
			_tokens.emplace_back(JSON_Token::T_onenine, data);
			return true;
		}

		_tokens.emplace_back(r);
		if (r == '"')
			_state_stack.push(STATE_STRING);

		return true;
	}

	Result<bool>
	try_to_parse_multichar_terminal(Rune r, std::span<const std::pair<STATE, Rune>> table, JSON_Token::KIND kind)
	{
		auto state = _state_stack.top();

		if (auto [first_state, expected] = table.front(); state == first_state)
		{
			if (r != expected) return false;

			auto [next_state, _] = table[1];
			_state_stack.push(next_state);
			return true;
		}

		if (auto [last_state, expected] = table.back(); state == last_state)
		{
			if (r != expected) return Error{"Unexpected terminal"};
			_state_stack.pop();
			_tokens.emplace_back(kind);
			return true;
		}

		for (size_t i = 1; i < table.size() - 1; i++)
		{
			auto [middle_state, expected] = table[i];
			auto [next_state, _] = table[i + 1];

			if (state == middle_state)
			{
				if (r != expected) return Error{"Unexpected terminal"};
				_state_stack.pop();
				_state_stack.push(next_state);
				return true;
			}
		}
		return false;
	}

	Result<bool>
	try_to_parse_true(Rune r)
	{
		std::pair<STATE, Rune> table[]{
			{STATE_0, 't'},
			{STATE_T, 'r'},
			{STATE_TR, 'u'},
			{STATE_TRU, 'e'},
		};

		return try_to_parse_multichar_terminal(r, table, JSON_Token::T_true);
	}

	Result<bool>
	try_to_parse_false(Rune r)
	{
		std::pair<STATE, Rune> table[]{
			{STATE_0, 'f'},
			{STATE_F, 'a'},
			{STATE_FA, 'l'},
			{STATE_FAL, 's'},
			{STATE_FALS, 'e'},
		};

		return try_to_parse_multichar_terminal(r, table, JSON_Token::T_false);
	}

	Result<bool>
	try_to_parse_null(Rune r)
	{
		std::pair<STATE, Rune> table[]{
			{STATE_0, 'n'},
			{STATE_N, 'u'},
			{STATE_NU, 'l'},
			{STATE_NUL, 'l'},
		};

		return try_to_parse_multichar_terminal(r, table, JSON_Token::T_null);
	}

	Result<bool>
	try_to_parse_character_in_string(Rune r)
	{
		if (_state_stack.top() != STATE_STRING) return false;

		if (r == '"')
		{
			_state_stack.pop();
			_tokens.emplace_back(r);
			return true;
		}

		if (r == '\\')
		{
			_state_stack.push(STATE_BACKSLASH);
			_tokens.emplace_back(r);
			return true;
		}

		char utf8[5]{};
		utf8proc_encode_char(r, (utf8proc_uint8_t*)utf8);
		_tokens.emplace_back(JSON_Token::T_unicode, utf8);
		return true;
	}

	Result<bool>
	try_to_parse_escaped_character(Rune r)
	{
		if (_state_stack.top() != STATE_BACKSLASH) return {};
		switch (r)
		{
		case '"':
		case '\\':
		case '/':
		case 'b':
		case 'f':
		case 'n':
		case 'r':
		case 't': {
			_state_stack.pop();
			char data[]{char(r)};
			_tokens.emplace_back(JSON_Token::T_escaped, data);
			return true;
		}

		case 'u':
			assert(false && "TODO");

		default:
			return Error{"Invalid escaped character"};
		}
	}

	Result<std::vector<JSON_Token>>
	lex()
	{
		for (auto [rune, err] : Utf8_Iterator{_string})
		{
			if (err)
				return err;

			if (auto [ok, err] = try_to_parse_null(rune); ok) continue;
			else if (err) return err;

			if (auto [ok, err] = try_to_parse_true(rune); ok) continue;
			else if (err) return err;

			if (auto [ok, err] = try_to_parse_false(rune); ok) continue;
			else if (err) return err;

			if (auto [ok, err] = try_to_parse_singlechar_terminal(rune); ok) continue;
			else if (err) return err;

			if (auto [ok, err] = try_to_parse_character_in_string(rune); ok) continue;
			else if (err) return err;

			if (auto [ok, err] = try_to_parse_escaped_character(rune); ok) continue;
			else if (err) return err;

			unreachable("Failed to parse character");
		}

		_tokens.emplace_back(JSON_Token::META_END_OF_INPUT);
		return std::move(_tokens);
	}
};

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

	auto [tokens, lex_err] = Lexer(json_string).lex();
	if (lex_err)
		return {J_JSON{}, lex_err.err.data()};

	auto [json, parse_err] = Parser(tokens).parse();
	if (parse_err)
		return {J_JSON{}, parse_err.err.data()};

	return {json};
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

void
JSON_Token::fill_ptable(PTable& ptable)
{
	ptable.add(META_START, T_lbracket, {N_V});
	ptable.add(META_START, T_lbrace, {N_V});
	ptable.add(META_START, T_quote, {N_V});
	ptable.add(META_START, T_zero, {N_V});
	ptable.add(META_START, T_onenine, {N_V});
	ptable.add(META_START, T_minus, {N_V});
	ptable.add(META_START, T_true, {N_V});
	ptable.add(META_START, T_false, {N_V});
	ptable.add(META_START, T_null, {N_V});

	ptable.add(N_V, T_lbracket, {N_ARRAY});
	ptable.add(N_V, T_lbrace, {N_OBJECT});
	ptable.add(N_V, T_quote, {N_STRING});
	ptable.add(N_V, T_zero, {N_NUMBER});
	ptable.add(N_V, T_onenine, {N_NUMBER});
	ptable.add(N_V, T_minus, {N_NUMBER});
	ptable.add(N_V, T_true, {T_true});
	ptable.add(N_V, T_false, {T_false});
	ptable.add(N_V, T_null, {T_null});

	ptable.add(N_OBJECT, T_lbrace, {T_lbrace, N_MEMBERS, T_rbrace});

	ptable.add(N_MEMBERS, T_comma, {T_comma, N_MEMBER, N_MEMBERS});
	ptable.add(N_MEMBERS, T_rbrace, {META_EPS});
	ptable.add(N_MEMBERS, T_quote, {N_MEMBER, N_MEMBERS});

	ptable.add(N_MEMBER, T_quote, {N_STRING, T_colon, N_V});

	ptable.add(N_ARRAY, T_lbracket, {T_lbracket, N_ELEMENTS, T_rbracket});

	ptable.add(N_ELEMENTS, T_comma, {T_comma, N_V, N_ELEMENTS});
	ptable.add(N_ELEMENTS, T_rbracket, {META_EPS});
	
	ptable.add(N_ELEMENTS, T_lbracket, {N_V, N_ELEMENTS});
	ptable.add(N_ELEMENTS, T_lbrace, {N_V, N_ELEMENTS});
	ptable.add(N_ELEMENTS, T_quote, {N_V, N_ELEMENTS});
	ptable.add(N_ELEMENTS, T_zero, {N_V, N_ELEMENTS});
	ptable.add(N_ELEMENTS, T_onenine, {N_V, N_ELEMENTS});
	ptable.add(N_ELEMENTS, T_minus, {N_V, N_ELEMENTS});
	ptable.add(N_ELEMENTS, T_true, {N_V, N_ELEMENTS});
	ptable.add(N_ELEMENTS, T_false, {N_V, N_ELEMENTS});
	ptable.add(N_ELEMENTS, T_null, {N_V, N_ELEMENTS});

	ptable.add(N_STRING, T_quote, {T_quote, N_CHARS, T_quote});

	ptable.add(N_CHARS, T_quote, {META_EPS});
	ptable.add(N_CHARS, T_unicode, {N_CHAR, N_CHARS});

	ptable.add(N_CHAR, T_unicode, {T_unicode});
	ptable.add(N_CHAR, T_backslash, {T_backslash, T_escaped});

	ptable.add(N_NUMBER, T_zero, {N_INTEGER, N_FRACTION, N_EXPONENT});
	ptable.add(N_NUMBER, T_onenine, {N_INTEGER, N_FRACTION, N_EXPONENT});
	ptable.add(N_NUMBER, T_minus, {T_minus, N_INTEGER, N_FRACTION, N_EXPONENT});

	ptable.add(N_INTEGER, T_zero, {T_zero});
	ptable.add(N_INTEGER, T_onenine, {T_onenine, N_DIGITS});

	ptable.add(N_DIGIT, T_zero, {T_zero});
	ptable.add(N_DIGIT, T_onenine, {T_onenine});

	ptable.add(N_DIGITS, T_comma, {META_EPS});
	ptable.add(N_DIGITS, T_rbracket, {META_EPS});
	ptable.add(N_DIGITS, T_rbrace, {META_EPS});
	ptable.add(N_DIGITS, T_zero, {N_DIGIT, N_DIGITS});
	ptable.add(N_DIGITS, T_onenine, {N_DIGIT, N_DIGITS});
	ptable.add(N_DIGITS, T_e, {META_EPS});
	ptable.add(N_DIGITS, T_E, {META_EPS});
	ptable.add(N_DIGITS, T_dot, {META_EPS});
	ptable.add(N_DIGITS, META_END_OF_INPUT, {META_EPS});

	ptable.add(N_FRACTION, T_comma, {META_EPS});
	ptable.add(N_FRACTION, T_rbracket, {META_EPS});
	ptable.add(N_FRACTION, T_rbrace, {META_EPS});
	ptable.add(N_FRACTION, T_dot, {T_dot, N_DIGIT, N_DIGITS});
	ptable.add(N_FRACTION, T_e, {META_EPS});
	ptable.add(N_FRACTION, T_E, {META_EPS});
	ptable.add(N_FRACTION, META_END_OF_INPUT, {META_EPS});

	ptable.add(N_EXPONENT, T_comma, {META_EPS});
	ptable.add(N_EXPONENT, T_rbracket, {META_EPS});
	ptable.add(N_EXPONENT, T_rbrace, {META_EPS});
	ptable.add(N_EXPONENT, T_e, {T_e, N_SIGN, N_DIGIT, N_DIGITS});
	ptable.add(N_EXPONENT, T_E, {T_E, N_SIGN, N_DIGIT, N_DIGITS});
	ptable.add(N_EXPONENT, META_END_OF_INPUT, {META_EPS});

	ptable.add(N_SIGN, T_zero, {META_EPS});
	ptable.add(N_SIGN, T_onenine, {META_EPS});
	ptable.add(N_SIGN, T_minus, {T_minus});
	ptable.add(N_SIGN, T_plus, {T_plus});
}
