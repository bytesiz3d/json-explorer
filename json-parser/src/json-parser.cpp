#include "json-parser/json-parser.h"
#include "Base.h"

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

#include <utf8proc.h>

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
		T_colon     = ':',
		T_backslash = '\\',

		T_number = 0x1000,  // data is relevant
		T_string,           // data is relevant

		N_V = 0x10000,

		N_OBJECT,
		N_MEMBERS,
		N_MEMBER,

		N_ARRAY,
		N_ELEMENTS,
		N_MORE_ELEMENTS,
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
		case T_colon:     return ":";
		case T_backslash: return "\\";

		case T_number:
		case T_string:
			return _data;

		case N_V: return "<V>";

		case N_OBJECT:  return "<OBJECT>";
		case N_MEMBERS: return "<MEMBERS>";
		case N_MEMBER:  return "<MEMBER>";

		case N_ARRAY:    return "<ARRAY>";
		case N_ELEMENTS: return "<ELEMENTS>";
		case N_MORE_ELEMENTS: return "<MORE_ELEMENTS>";

		default:
			unreachable("Invalid token");
			return "";
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
		case T_colon:
		case T_backslash:

		case T_number:
		case T_string:

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

struct JSON_Builder
{
	struct Context
	{
		J_JSON json;
		std::vector<J_JSON> array_builder;
		std::vector<J_Pair> object_builder;
	};
	std::stack<Context> _context;

	JSON_Builder() : _context{}
	{
		_context.push(Context{});
	}

	J_JSON
	yield()
	{
		return _context.top().json;
	}

	void
	set_json(J_JSON json)
	{
		auto &ctx = _context.top();
		if (ctx.json.kind == J_JSON_ARRAY) // build array
		{
			ctx.array_builder.push_back(json);
		}
		else if (ctx.json.kind == J_JSON_OBJECT)
		{
			if (ctx.object_builder.back().key)
			{
				ctx.object_builder.back().value = json;
				ctx.object_builder.push_back({});
			}
			else
			{
				assert(json.kind == J_JSON_STRING);
				ctx.object_builder.back().key = json.as_string;
			}
		}
		else
		{
			ctx.json = json;
		}
	}

	void
	token(const JSON_Token &tkn)
	{
		switch (tkn.kind())
		{
		case JSON_Token::T_null:
			return set_json({.kind = J_JSON_NULL});

		case JSON_Token::T_true:
			return set_json({.kind = J_JSON_BOOL, .as_bool = true});

		case JSON_Token::T_false:
			return set_json({.kind = J_JSON_BOOL, .as_bool = false});

		case JSON_Token::T_number:
			return set_json({.kind = J_JSON_NUMBER, .as_number = ::atof(tkn.data().data())});

		case JSON_Token::T_string:
			return set_json({.kind = J_JSON_STRING, .as_string = ::strdup(tkn.data().data())});

		case JSON_Token::T_lbracket:
			return _context.push(Context{.json{J_JSON_ARRAY}});

		case JSON_Token::T_lbrace:
			_context.push(Context{.json{J_JSON_OBJECT}});
			return _context.top().object_builder.push_back({}); // dummy

		case JSON_Token::T_rbracket:
		case JSON_Token::T_rbrace: {
			auto last_ctx = std::move(_context.top());
			if (last_ctx.json.kind == J_JSON_ARRAY)
			{
				assert(last_ctx.object_builder.empty());
				size_t sz = last_ctx.array_builder.size() * sizeof(J_JSON);
				last_ctx.json.as_array = {
					.ptr = (J_JSON*)::malloc(sz),
					.count = last_ctx.array_builder.size()
				};
				::memcpy(last_ctx.json.as_array.ptr, last_ctx.array_builder.data(), sz);
			}
			else if (last_ctx.json.kind == J_JSON_OBJECT)
			{
				assert(last_ctx.array_builder.empty());
				assert(last_ctx.object_builder.back().key == nullptr);
				last_ctx.object_builder.pop_back();

				size_t sz = last_ctx.object_builder.size() * sizeof(J_Pair);
				last_ctx.json.as_object = {
					.pairs = (J_Pair*)::malloc(sz),
					.count = last_ctx.object_builder.size()
				};
				::memcpy(last_ctx.json.as_object.pairs, last_ctx.object_builder.data(), sz);
			}

			_context.pop();
			set_json(last_ctx.json);
		}

		default: break;
		}
	}
};

struct Parser
{
	std::span<JSON_Token> _tokens;
	std::span<JSON_Token>::iterator _it;
	PTable& _ptable{JSON_PTable()};

	Parser() = default;
	Parser(std::span<JSON_Token> tokens) : _tokens(tokens), _it(_tokens.begin()), _ptable(JSON_PTable()) {}

	Error
	advance_tokens_iterator()
	{
		if (_it == _tokens.end())
			return Error{"Incomplete"};
		_it++;
		return Error{};
	}

	Error
	finish_input()
	{
		if (_it->is_equal(JSON_Token::META_END_OF_INPUT))
			_it++;

		if (_it != _tokens.end())
			return Error{"Trailing characters"};

		return Error{};
	}

	Result<J_JSON>
	parse()
	{
		std::stack<JSON_Token> stack{};
		stack.emplace(JSON_Token::META_START);

		JSON_Builder builder{};

		while (stack.empty() == false)
		{
			JSON_Token input_terminal = *_it;
			if (input_terminal.is_equal(stack.top()))
			{
				stack.pop();
				builder.token(input_terminal);

				_it++;
				if (_it == _tokens.end()) return Error{"Incomplete"};
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
				stack.pop();
				for (auto rhs = production.rhs.rbegin(); rhs != production.rhs.rend(); rhs++)
				{
				 	if (rhs->kind() != JSON_Token::META_EPS)
						stack.push(*rhs);
				}
			}
		}

		if (_it->is_equal(JSON_Token::META_END_OF_INPUT) == false)
			return Error{"Trailing characters"};
		assert(_it + 1 == _tokens.end());

		return builder.yield();
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

		STATE_NUMBER_MINUS,
		STATE_NUMBER_INTEGER,
		STATE_NUMBER_INTEGER_LEADING_ZERO,
		STATE_NUMBER_FRACTION,
		STATE_NUMBER_FRACTION_DIGITS,
		STATE_NUMBER_EXPONENT,
		STATE_NUMBER_EXPONENT_SIGN,
		STATE_NUMBER_EXPONENT_DIGITS,

		STATE_u,
		STATE_uX,
		STATE_uXX,
		STATE_uXXX,
		STATE_uXXXX,

		STATE_STRING,
		STATE_BACKSLASH,
	};

	std::stack<STATE> _state_stack;
	std::vector<JSON_Token> _tokens;
	std::string _string_builder;
	std::string _number_builder;

	Lexer() = default;
	Lexer(std::string_view string) : _string(string), _state_stack{}, _tokens{}, _number_builder{}
	{
		_state_stack.push(STATE_0);
	}

	bool
	is_whitespace(Rune rune)
	{
		switch (rune)
		{
		case 0x20: case 0x0a: case 0x0d: case 0x09:
			return true;
		default:
			return false;
		}
	}

	bool
	is_digit(Rune rune)
	{
		return Rune('0') <= rune && rune <= Rune('9');
	}

	bool
	is_hexdigit(Rune rune)
	{
		return is_digit(rune) || (Rune('a') <= rune && rune <= Rune('f')) || (Rune('A') <= rune && rune <= Rune('F'));
	}

	bool
	is_singlechar_terminal(Rune rune)
	{
		switch (rune)
		{
		case JSON_Token::T_comma:
		case JSON_Token::T_lbracket:
		case JSON_Token::T_rbracket:
		case JSON_Token::T_lbrace:
		case JSON_Token::T_rbrace:
		case JSON_Token::T_colon:
		case JSON_Token::T_backslash:
			return true;
		default:
			return false;
		}
	}

	Result<bool>
	try_to_parse_singlechar_terminal(Rune rune)
	{
		if (_state_stack.top() != STATE_0)
			return false;

		if (is_whitespace(rune)) // skip whitespace
			return true;

		if (is_digit(rune))
			unreachable("Found a digit");

		if (rune == '"')
		{
			_state_stack.push(STATE_STRING);
			return true;
		}

		if (is_singlechar_terminal(rune))
		{
			_tokens.emplace_back(rune);
			return true;
		}

		return false;
	}

	Result<bool>
	try_to_parse_multichar_terminal(Rune rune, std::span<const std::pair<STATE, Rune>> table, JSON_Token::KIND kind)
	{
		auto state = _state_stack.top();

		if (auto [first_state, expected] = table.front(); state == first_state)
		{
			if (rune != expected) return false;

			auto [next_state, _] = table[1];
			_state_stack.push(next_state);
			return true;
		}

		if (auto [last_state, expected] = table.back(); state == last_state)
		{
			if (rune != expected)
				return Error{"Unexpected terminal"};
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
				if (rune != expected)
					return Error{"Unexpected terminal"};
				_state_stack.top() = next_state;
				return true;
			}
		}
		return false;
	}

	Result<bool>
	try_to_parse_true(Rune rune)
	{
		std::pair<STATE, Rune> table[]{
			{STATE_0, 't'},
			{STATE_T, 'r'},
			{STATE_TR, 'u'},
			{STATE_TRU, 'e'},
		};

		return try_to_parse_multichar_terminal(rune, table, JSON_Token::T_true);
	}

	Result<bool>
	try_to_parse_false(Rune rune)
	{
		std::pair<STATE, Rune> table[]{
			{STATE_0, 'f'},
			{STATE_F, 'a'},
			{STATE_FA, 'l'},
			{STATE_FAL, 's'},
			{STATE_FALS, 'e'},
		};

		return try_to_parse_multichar_terminal(rune, table, JSON_Token::T_false);
	}

	Result<bool>
	try_to_parse_null(Rune rune)
	{
		std::pair<STATE, Rune> table[]{
			{STATE_0, 'n'},
			{STATE_N, 'u'},
			{STATE_NU, 'l'},
			{STATE_NUL, 'l'},
		};

		return try_to_parse_multichar_terminal(rune, table, JSON_Token::T_null);
	}

	bool
	continue_parse_number(Rune rune)
	{
		_number_builder += char(rune);
		return true;
	}

	bool
	end_parse_number()
	{
		_state_stack.pop();
		_tokens.emplace_back(JSON_Token::T_number, std::move(_number_builder));
		_number_builder.clear();
		return false;
	}

	bool
	continue_parse_string(Rune rune)
	{
		_string_builder += char(rune);
		return true;
	}

	bool
	continue_parse_string(const char *str)
	{
		_string_builder += str;
		return true;
	}

	bool
	end_parse_string()
	{
		_state_stack.pop();
		_tokens.emplace_back(JSON_Token::T_string, std::move(_string_builder));
		_string_builder.clear();
		return true;
	}

	Result<bool>
	try_to_parse_number(Rune rune)
	{
		switch (auto state_top = _state_stack.top())
		{
		case STATE_0:
		{
			if (rune == '-')
			{
				_state_stack.push(STATE_NUMBER_MINUS);
				return continue_parse_number(rune);
			}

			if (is_digit(rune) == false) return false;

			_state_stack.push(rune == '0' ? STATE_NUMBER_INTEGER_LEADING_ZERO : STATE_NUMBER_INTEGER);
			return continue_parse_number(rune);
		}
		case STATE_NUMBER_MINUS: {
			if (is_digit(rune) == false) return Error{"Invalid character in number"};

			_state_stack.top() = rune == '0' ? STATE_NUMBER_INTEGER_LEADING_ZERO : STATE_NUMBER_INTEGER;
			return continue_parse_number(rune);
		}
		case STATE_NUMBER_INTEGER_LEADING_ZERO:
		case STATE_NUMBER_INTEGER: {
			if (state_top == STATE_NUMBER_INTEGER_LEADING_ZERO && is_digit(rune))
				return Error{"Leading zero"};

			if (rune == '.')
			{
				_state_stack.top() = STATE_NUMBER_FRACTION;
				return continue_parse_number(rune);
			}
			if (rune == 'e' || rune == 'E')
			{
				_state_stack.top() = STATE_NUMBER_EXPONENT;
				return continue_parse_number(rune);
			}

			return is_digit(rune) ? continue_parse_number(rune) : end_parse_number();
		}
		case STATE_NUMBER_FRACTION: {
			if (is_digit(rune) == false) return Error{"Invalid fraction"};

			_state_stack.top() = STATE_NUMBER_FRACTION_DIGITS;
			return continue_parse_number(rune);
		}
		case STATE_NUMBER_FRACTION_DIGITS: {
			if (rune == 'e' || rune == 'E')
			{
				_state_stack.top() = STATE_NUMBER_EXPONENT;
				return continue_parse_number(rune);
			}

			return is_digit(rune) ? continue_parse_number(rune) : end_parse_number();
		}
		case STATE_NUMBER_EXPONENT:
		case STATE_NUMBER_EXPONENT_SIGN: {
			if (state_top == STATE_NUMBER_EXPONENT && (rune == '-' || rune == '+'))
			{
				_state_stack.top() = STATE_NUMBER_EXPONENT_SIGN;
				return continue_parse_number(rune);
			}

			if (is_digit(rune) == false) return Error{"Invalid exponent"};

			_state_stack.top() = STATE_NUMBER_EXPONENT_DIGITS;
			return continue_parse_number(rune);
		}
		case STATE_NUMBER_EXPONENT_DIGITS: {
			return is_digit(rune) ? continue_parse_number(rune) : end_parse_number();
		}
		default:
			return false;
		}
	}

	Result<bool>
	try_to_parse_character_in_string(Rune rune)
	{
		if (_state_stack.top() != STATE_STRING) return false;

		if (rune == '"')
			return end_parse_string();

		if (Rune(0x20) <= rune && rune <= Rune(0x10ffff))
		{
			if (rune == '\\')
				_state_stack.push(STATE_BACKSLASH);

			char utf8[5]{};
			utf8proc_encode_char(rune, (utf8proc_uint8_t*)utf8);
			return continue_parse_string(utf8);
		}

		return Error{"Invalid unicode character"};
	}

	Result<bool>
	try_to_parse_escaped_character(Rune rune)
	{
		if (_state_stack.top() != STATE_BACKSLASH) return false;
		switch (rune)
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
			return continue_parse_string(rune);
		}

		case 'u': {
			_state_stack.top() = STATE_u;
			return continue_parse_string(rune);
		}

		default:
			return Error{"Invalid escaped character"};
		}
	}

	Result<bool>
	try_to_parse_escaped_unicode(Rune rune)
	{
		if (_state_stack.top() != STATE_u
			&& _state_stack.top() != STATE_uX
			&& _state_stack.top() != STATE_uXX
			&& _state_stack.top() != STATE_uXXX
		) return false;

		if (is_hexdigit(rune) == false)
			return Error{"Invalid escaped unicode"};

		_state_stack.top() = STATE(_state_stack.top() + 1);
		if (_state_stack.top() == STATE_uXXXX)
			_state_stack.pop();

		return continue_parse_string(rune);
	}

	Error
	try_to_parse(Rune rune)
	{
		if (auto [ok, err] = try_to_parse_character_in_string(rune); ok) return Error{};
		else if (err) return err;

		if (auto [ok, err] = try_to_parse_escaped_character(rune); ok) return Error{};
		else if (err) return err;

		if (auto [ok, err] = try_to_parse_escaped_unicode(rune); ok) return Error{};
		else if (err) return err;

		if (auto [ok, err] = try_to_parse_null(rune); ok) return Error{};
		else if (err) return err;

		if (auto [ok, err] = try_to_parse_true(rune); ok) return Error{};
		else if (err) return err;

		if (auto [ok, err] = try_to_parse_false(rune); ok) return Error{};
		else if (err) return err;

		if (auto [ok, err] = try_to_parse_number(rune); ok) return Error{};
		else if (err) return err;

		if (auto [ok, err] = try_to_parse_singlechar_terminal(rune); ok) return Error{};
		else if (err) return err;

		return Error{"Invalid character"};
	}

	Result<std::vector<JSON_Token>>
	lex()
	{
		for (auto [rune, err] : Utf8_Iterator{_string})
		{
			if (err)
				return err;

			if (auto parse_err = try_to_parse(rune))
				return parse_err;
		}

		try_to_parse(Rune(JSON_Token::META_END_OF_INPUT));
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

size_t
_j_dump(J_JSON json, char* dump)
{
	size_t size = 0;
	switch (json.kind)
	{
	case J_JSON_NULL:
		size += 4;
		if (dump)
			::memcpy((void*)dump, "null", size);
		return size;

	case J_JSON_BOOL:
		size += json.as_bool ? 4 : 5;
		if (dump)
			::memcpy((void*)dump, json.as_bool ? "true" : "false", size);
		return size;

	case J_JSON_NUMBER:
		size += ::snprintf(nullptr, 0, "%lf", json.as_number);
		if (dump)
			::snprintf(dump, size+1, "%lf", json.as_number);
		return size;

	case J_JSON_STRING:
		size += 1;
		if (dump)
			dump[0] = '"';

		size += ::strlen(json.as_string);
		if (dump)
			::memcpy((void*)&dump[1], json.as_string, size - 1);

		size += 1;
		if (dump)
			dump[size - 1] = '"';
		return size;

	case J_JSON_ARRAY:
		size += 1;
		if (dump)
			dump[0] = '[';

		for (auto it = json.as_array.ptr; it != json.as_array.ptr + json.as_array.count; it++)
		{
			if (it != json.as_array.ptr)
			{
				size += 1;
				if (dump)
					dump[size - 1] = ',';
			}

			size += _j_dump(*it, dump ? dump + size : nullptr);
		}

		size += 1;
		if (dump)
			dump[size - 1] = ']';
		return size;

	case J_JSON_OBJECT:
		size += 1;
		if (dump)
			dump[0] = '{';

		for (auto it = json.as_object.pairs; it != json.as_object.pairs + json.as_object.count; it++)
		{
			if (it != json.as_object.pairs)
			{
				size += 1;
				if (dump)
					dump[size - 1] = ',';
			}

			J_JSON key_json{.kind = J_JSON_STRING, .as_string = it->key};
			size += _j_dump(key_json, dump ? dump + size : nullptr);

			size += 1; // colon
			if (dump)
				dump[size - 1] = ':';

			size += _j_dump(it->value, dump ? dump + size : nullptr);
		}

		size += 1;
		if (dump)
			dump[size - 1] = '}';
		return size;
		
	default:
		unreachable("invalid type");
		return 0;
	}
}

const char*
j_dump(J_JSON json)
{
	// Find how much memory we need
	size_t size = _j_dump(json, nullptr);

	char* dump = (char*)::malloc(size + 1);
	_j_dump(json, dump);
	dump[size] = '\0';
	return dump;
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
	ptable.add(META_START, T_null, {N_V});
	ptable.add(META_START, T_true, {N_V});
	ptable.add(META_START, T_false, {N_V});
	ptable.add(META_START, T_number, {N_V});
	ptable.add(META_START, T_string, {N_V});
	ptable.add(META_START, T_lbracket, {N_V});
	ptable.add(META_START, T_lbrace, {N_V});

	ptable.add(N_V, T_null, {T_null});
	ptable.add(N_V, T_true, {T_true});
	ptable.add(N_V, T_false, {T_false});
	ptable.add(N_V, T_number, {T_number});
	ptable.add(N_V, T_string, {T_string});
	ptable.add(N_V, T_lbracket, {N_ARRAY});
	ptable.add(N_V, T_lbrace, {N_OBJECT});

	ptable.add(N_OBJECT, T_lbrace, {T_lbrace, N_MEMBERS, T_rbrace});

	ptable.add(N_MEMBERS, T_string, {N_MEMBER, N_MEMBERS});
	ptable.add(N_MEMBERS, T_comma, {T_comma, N_MEMBER, N_MEMBERS});
	ptable.add(N_MEMBERS, T_rbrace, {META_EPS});

	ptable.add(N_MEMBER, T_string, {T_string, T_colon, N_V});

	ptable.add(N_ARRAY, T_lbracket, {T_lbracket, N_ELEMENTS, T_rbracket});

	ptable.add(N_ELEMENTS, T_lbracket, {N_V, N_MORE_ELEMENTS});
	ptable.add(N_ELEMENTS, T_lbrace, {N_V, N_MORE_ELEMENTS});
	ptable.add(N_ELEMENTS, T_string, {N_V, N_MORE_ELEMENTS});
	ptable.add(N_ELEMENTS, T_number, {N_V, N_MORE_ELEMENTS});
	ptable.add(N_ELEMENTS, T_true, {N_V, N_MORE_ELEMENTS});
	ptable.add(N_ELEMENTS, T_false, {N_V, N_MORE_ELEMENTS});
	ptable.add(N_ELEMENTS, T_null, {N_V, N_MORE_ELEMENTS});
	ptable.add(N_ELEMENTS, T_rbracket, {META_EPS});

	ptable.add(N_MORE_ELEMENTS, T_rbracket, {META_EPS});
	ptable.add(N_MORE_ELEMENTS, T_comma, {T_comma, N_V, N_MORE_ELEMENTS});
}