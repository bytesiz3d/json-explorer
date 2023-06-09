#include "json-parser/json-parser.h"

#include <array>
#include <initializer_list>
#include <iostream>
#include <span>
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>

#include <assert.h>

#include <utf8proc.h>

#include <tracy/Tracy.hpp>

// OPTIMIZE:
// * Profile standard library RAII containers and search for replacements
#define unreachable(msg) assert(!msg)

struct PTable;

struct String_View
{
	const char* ptr;
	size_t count;

	constexpr String_View(const char* ptr)
		: ptr(ptr), count(std::char_traits<char>::length(ptr))
	{
	}

	String_View() = default;

	String_View(const char* ptr, size_t count)
		: ptr(ptr), count(count)
	{
	}

	const char*
	cstr_new()
	{
		char* str = (char*)::malloc(count + 1);
		::memcpy((void*)str, ptr, count);
		str[count] = '\0';
		return str;
	}
};

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

	Result(Error e) : val{}, err(e)
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
		N_MORE_MEMBERS,

		N_ARRAY,
		N_ELEMENTS,
		N_MORE_ELEMENTS,
	} _kind;

	String_View _data;

	JSON_Token() = default;
	JSON_Token(Rune kind, String_View data = {}) : _kind((KIND)kind), _data(data)
	{
	}
	JSON_Token(Rune kind, std::string_view data) : _kind((KIND)kind), _data{data.data(), data.size()}
	{
	}

	KIND
	kind() const
	{
		return _kind;
	}

	String_View
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
		case N_MORE_MEMBERS:  return "<MORE_MEMBERS>";

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
		return is_terminal() == other.is_terminal() && kind() == other.kind();
	}

	static void
	fill_ptable(PTable& ptable);
};

struct Production
{
	JSON_Token lhs;
	std::vector<JSON_Token> rhs;
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
		ZoneScoped;

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
		auto& ctx = _context.top();
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
	token(const JSON_Token& tkn)
	{
		switch (tkn.kind())
		{
		case JSON_Token::T_null:
			return set_json({.kind = J_JSON_NULL});

		case JSON_Token::T_true:
			return set_json({.kind = J_JSON_BOOL, .as_bool = true});

		case JSON_Token::T_false:
			return set_json({.kind = J_JSON_BOOL, .as_bool = false});

		case JSON_Token::T_number: {
			const char* tmp_str = tkn.data().cstr_new();
			set_json({.kind = J_JSON_NUMBER, .as_number = ::atof(tmp_str)});
			return ::free((void*)tmp_str);
		}

		case JSON_Token::T_string:
			return set_json({.kind = J_JSON_STRING, .as_string = tkn.data().cstr_new()});

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

	Result<J_JSON>
	parse()
	{
		ZoneScoped;

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
	String_View _terminal_builder;

	Lexer() = default;
	Lexer(std::string_view string) : _string(string), _state_stack{}, _tokens{}, _terminal_builder{}
	{
		_state_stack.push(STATE_0);
	}

	inline bool
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

	inline bool
	is_digit(Rune rune)
	{
		return Rune('0') <= rune && rune <= Rune('9');
	}

	inline bool
	is_hexdigit(Rune rune)
	{
		return is_digit(rune) || (Rune('a') <= rune && rune <= Rune('f')) || (Rune('A') <= rune && rune <= Rune('F'));
	}

	inline bool
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

	inline Result<bool>
	try_to_scan_singlechar_terminal(String_View str, Rune rune)
	{
		ZoneScoped;

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

	inline Result<bool>
	try_to_scan_multichar_terminal(String_View str, Rune rune, std::span<const std::pair<STATE, Rune>> table, JSON_Token::KIND kind)
	{
		ZoneScoped;

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

	inline Result<bool>
	try_to_scan_true(String_View str, Rune rune)
	{
		std::pair<STATE, Rune> table[]{
			{STATE_0, 't'},
			{STATE_T, 'r'},
			{STATE_TR, 'u'},
			{STATE_TRU, 'e'},
		};

		return try_to_scan_multichar_terminal(str, rune, table, JSON_Token::T_true);
	}

	inline Result<bool>
	try_to_scan_false(String_View str, Rune rune)
	{
		std::pair<STATE, Rune> table[]{
			{STATE_0, 'f'},
			{STATE_F, 'a'},
			{STATE_FA, 'l'},
			{STATE_FAL, 's'},
			{STATE_FALS, 'e'},
		};

		return try_to_scan_multichar_terminal(str, rune, table, JSON_Token::T_false);
	}

	inline Result<bool>
	try_to_scan_null(String_View str, Rune rune)
	{
		std::pair<STATE, Rune> table[]{
			{STATE_0, 'n'},
			{STATE_N, 'u'},
			{STATE_NU, 'l'},
			{STATE_NUL, 'l'},
		};

		return try_to_scan_multichar_terminal(str, rune, table, JSON_Token::T_null);
	}

	inline bool
	continue_scan_number(String_View str)
	{
		ZoneScoped;
		if (_terminal_builder.ptr == nullptr)
			_terminal_builder.ptr = str.ptr;

		_terminal_builder.count++;
		return true;
	}

	inline bool
	end_scan_number()
	{
		ZoneScoped;
		_state_stack.pop();
		_tokens.emplace_back(JSON_Token::T_number, _terminal_builder);
		_terminal_builder = {};
		return false;
	}

	inline bool
	continue_scan_string(String_View view)
	{
		ZoneScoped;
		if (_terminal_builder.ptr == nullptr)
			_terminal_builder.ptr = view.ptr;

		_terminal_builder.count += view.count;
		return true;
	}

	inline bool
	end_scan_string()
	{
		ZoneScoped;
		_state_stack.pop();
		_tokens.emplace_back(JSON_Token::T_string, _terminal_builder);
		_terminal_builder = {};
		return true;
	}

	inline Result<bool>
	try_to_scan_number(String_View str, Rune rune)
	{
		ZoneScoped;
		switch (auto state_top = _state_stack.top())
		{
		case STATE_0: {
			if (rune == '-')
			{
				_state_stack.push(STATE_NUMBER_MINUS);
				return continue_scan_number(str);
			}

			if (is_digit(rune) == false) return false;

			_state_stack.push(rune == '0' ? STATE_NUMBER_INTEGER_LEADING_ZERO : STATE_NUMBER_INTEGER);
			return continue_scan_number(str);
		}
		case STATE_NUMBER_MINUS: {
			if (is_digit(rune) == false) return Error{"Invalid character in number"};

			_state_stack.top() = rune == '0' ? STATE_NUMBER_INTEGER_LEADING_ZERO : STATE_NUMBER_INTEGER;
			return continue_scan_number(str);
		}
		case STATE_NUMBER_INTEGER_LEADING_ZERO:
		case STATE_NUMBER_INTEGER: {
			if (state_top == STATE_NUMBER_INTEGER_LEADING_ZERO && is_digit(rune))
				return Error{"Leading zero"};

			if (rune == '.')
			{
				_state_stack.top() = STATE_NUMBER_FRACTION;
				return continue_scan_number(str);
			}
			if (rune == 'e' || rune == 'E')
			{
				_state_stack.top() = STATE_NUMBER_EXPONENT;
				return continue_scan_number(str);
			}

			return is_digit(rune) ? continue_scan_number(str) : end_scan_number();
		}
		case STATE_NUMBER_FRACTION: {
			if (is_digit(rune) == false) return Error{"Invalid fraction"};

			_state_stack.top() = STATE_NUMBER_FRACTION_DIGITS;
			return continue_scan_number(str);
		}
		case STATE_NUMBER_FRACTION_DIGITS: {
			if (rune == 'e' || rune == 'E')
			{
				_state_stack.top() = STATE_NUMBER_EXPONENT;
				return continue_scan_number(str);
			}

			return is_digit(rune) ? continue_scan_number(str) : end_scan_number();
		}
		case STATE_NUMBER_EXPONENT:
		case STATE_NUMBER_EXPONENT_SIGN: {
			if (state_top == STATE_NUMBER_EXPONENT && (rune == '-' || rune == '+'))
			{
				_state_stack.top() = STATE_NUMBER_EXPONENT_SIGN;
				return continue_scan_number(str);
			}

			if (is_digit(rune) == false) return Error{"Invalid exponent"};

			_state_stack.top() = STATE_NUMBER_EXPONENT_DIGITS;
			return continue_scan_number(str);
		}
		case STATE_NUMBER_EXPONENT_DIGITS: {
			return is_digit(rune) ? continue_scan_number(str) : end_scan_number();
		}
		default:
			return false;
		}
	}

	inline Result<bool>
	try_to_scan_character_in_string(String_View str, Rune rune)
	{
		ZoneScoped;
		if (_state_stack.top() != STATE_STRING) return false;

		if (rune == '"')
			return end_scan_string();

		if (Rune(0x20) <= rune && rune <= Rune(0x10ffff))
		{
			if (rune == '\\')
				_state_stack.push(STATE_BACKSLASH);

			return continue_scan_string(str);
		}

		return Error{"Invalid unicode character"};
	}

	inline Result<bool>
	try_to_scan_escaped_character(String_View str, Rune rune)
	{
		ZoneScoped;
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
			return continue_scan_string(str);
		}

		case 'u': {
			_state_stack.top() = STATE_u;
			return continue_scan_string(str);
		}

		default:
			return Error{"Invalid escaped character"};
		}
	}

	inline Result<bool>
	try_to_scan_escaped_unicode(String_View str, Rune rune)
	{
		ZoneScoped;
		switch (_state_stack.top())
		{
		case STATE_u:
		case STATE_uX:
		case STATE_uXX:
		case STATE_uXXX:
			if (is_hexdigit(rune) == false) return Error{"Invalid escaped unicode"};

			_state_stack.top() = STATE(_state_stack.top() + 1);
			if (_state_stack.top() == STATE_uXXXX)
				_state_stack.pop();

			return continue_scan_string(str);

		default:
			return false;
		}
	}

	inline Error
	try_to_scan(String_View str, Rune rune)
	{
		ZoneScoped;

		if (auto [ok, err] = try_to_scan_character_in_string(str, rune); ok) return Error{};
		else if (err) return err;

		if (auto [ok, err] = try_to_scan_escaped_character(str, rune); ok) return Error{};
		else if (err) return err;

		if (auto [ok, err] = try_to_scan_escaped_unicode(str, rune); ok) return Error{};
		else if (err) return err;

		if (auto [ok, err] = try_to_scan_null(str, rune); ok) return Error{};
		else if (err) return err;

		if (auto [ok, err] = try_to_scan_true(str, rune); ok) return Error{};
		else if (err) return err;

		if (auto [ok, err] = try_to_scan_false(str, rune); ok) return Error{};
		else if (err) return err;

		if (auto [ok, err] = try_to_scan_number(str, rune); ok) return Error{};
		else if (err) return err;

		if (auto [ok, err] = try_to_scan_singlechar_terminal(str, rune); ok) return Error{};
		else if (err) return err;

		return Error{"Invalid character"};
	}

	inline Error
	end_input()
	{
		try_to_scan({}, JSON_Token::META_END_OF_INPUT);
		_tokens.emplace_back(JSON_Token::META_END_OF_INPUT);
		return Error{};
	}

	Result<std::vector<JSON_Token>>
	lex()
	{
		ZoneScoped;

		const utf8proc_uint8_t* BASE = (const utf8proc_uint8_t*)_string.data();
		const utf8proc_ssize_t SIZE = _string.size();

		const utf8proc_uint8_t* it = BASE;
		while (it < BASE + SIZE)
		{
			FrameMark;

			Rune rune{};
			
			auto rune_size = utf8proc_iterate(it, SIZE - (it - BASE), &rune); 
			if (rune_size < 0)
				return Error{utf8proc_errmsg(rune_size)};

			if (auto parse_err = try_to_scan({(char*)it, (size_t)rune_size}, rune))
				return parse_err;

			it += rune_size;
		}

		end_input();
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
	ZoneScoped;

	auto table = JSON_PTable();

	auto [tokens, lex_err] = Lexer(json_string).lex();
	if (lex_err)
		return {J_JSON{}, lex_err.err.data()};

	auto [json, parse_err] = Parser(tokens).parse();
	if (parse_err)
		return {J_JSON{}, parse_err.err.data()};

	return {json};
}

void
j_free(J_JSON json)
{
	switch (json.kind)
	{
	case J_JSON_NULL:
	case J_JSON_BOOL:
	case J_JSON_NUMBER:
		return;

	case J_JSON_STRING:
		return ::free((void*)json.as_string);

	case J_JSON_ARRAY:
		for (auto it = json.as_array.ptr; it != json.as_array.ptr + json.as_array.count; it++)
			j_free(*it);

		return ::free((void*)json.as_array.ptr);

	case J_JSON_OBJECT:

		for (auto it = json.as_object.pairs; it != json.as_object.pairs + json.as_object.count; it++)
		{
			::free((void*)it->key);
			j_free(it->value);
		}

		return ::free((void*)json.as_object.pairs);

	default:
		unreachable("invalid kind");
	}
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
		size += ::snprintf(nullptr, 0, "%.16lg", json.as_number);
		if (dump)
			::snprintf(dump, size + 1, "%.16lg", json.as_number);
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
		unreachable("invalid kind");
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
	return json.as_bool;
}

J_Number
j_get_J_Number(J_JSON json)
{
	assert(json.kind == J_JSON_NUMBER);
	return json.as_number;
}

J_String
j_get_J_String(J_JSON json)
{
	assert(json.kind == J_JSON_STRING);
	return json.as_string;
}

J_Array
j_get_J_Array(J_JSON json)
{
	assert(json.kind == J_JSON_ARRAY);
	return json.as_array;
}

J_Object
j_get_J_Object(J_JSON json)
{
	assert(json.kind == J_JSON_OBJECT);
	return json.as_object;
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

	// Object
	ptable.add(N_OBJECT, T_lbrace, {T_lbrace, N_MEMBERS, T_rbrace});

	ptable.add(N_MEMBERS, T_string, {T_string, T_colon, N_V, N_MORE_MEMBERS});
	ptable.add(N_MEMBERS, T_rbrace, {META_EPS});

	ptable.add(N_MORE_MEMBERS, T_rbrace, {META_EPS});
	ptable.add(N_MORE_MEMBERS, T_comma, {T_comma, T_string, T_colon, N_V, N_MORE_MEMBERS});

	// Array
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
