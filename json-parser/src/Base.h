#pragma once

#include <string_view>
#include <format>
#include <span>

#include <utf8proc.h>

#define unreachable(msg) assert(!msg)

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

struct Utf8_Iterator
{
	const utf8proc_uint8_t* base;
	const utf8proc_uint8_t* it;
	const utf8proc_ssize_t size;

	inline Utf8_Iterator(std::string_view str) : base((utf8proc_uint8_t*)str.data()), it(base), size(str.size())
	{}

	inline Utf8_Iterator
	begin()
	{
		it = base;
		return *this;
	}

	inline Utf8_Iterator
	end()
	{
		it = base + size;
		return *this;
	}


	inline Utf8_Iterator
	operator++()
	{
		Rune _;
		auto bytes_read = utf8proc_iterate(it, size, &_);
		if (bytes_read < 0)
			return end();

		it += bytes_read;
		return *this;
	}

	inline Result<Rune>
	operator*()
	{
		Rune r{};
		auto errcode = utf8proc_iterate(it, size, &r);
		if (errcode < 0)
			return Error{utf8proc_errmsg(errcode)};
		return r;
	}

	inline bool
	operator!=(Utf8_Iterator other)
	{
		return it != other.it;
	}
};

template<typename T>
struct std::formatter<std::span<T>> : std::formatter<std::string>
{
	auto
	format(const auto& span, format_context& ctx)
	{
		// std::format_to(ctx.out(), "--[");
		for (auto tkn : span)
			std::format_to(ctx.out(), " {}", tkn);
		// std::format_to(ctx.out(), " ]--");

		return ctx.out();
	}
};