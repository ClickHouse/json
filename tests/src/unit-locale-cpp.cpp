//     __ _____ _____ _____
//  __|  |   __|     |   | |  JSON for Modern C++ (supporting code)
// |  |  |__   |  |  | | | |  version 3.12.0
// |_____|_____|_____|_|___|  https://github.com/nlohmann/json
//
// SPDX-FileCopyrightText: 2013 - 2025 Niels Lohmann <https://nlohmann.me>
// SPDX-License-Identifier: MIT

#include "doctest_compatibility.h"

#define JSON_TESTS_PRIVATE
#include <nlohmann/json.hpp>
using nlohmann::json;

#include <clocale>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

/// A `basic_json` whose floating-point type is neither IEEE-754 single nor double precision
/// is serialized with `snprintf` instead of the built-in Grisu2 conversion.
using json_long_double = nlohmann::basic_json<std::map, std::vector, std::string, bool,
      std::int64_t, std::uint64_t, long double>;

struct ParserImpl final: public nlohmann::json_sax<json>
{
    bool null() override
    {
        return true;
    }
    bool boolean(bool /*val*/) override
    {
        return true;
    }
    bool number_integer(json::number_integer_t /*val*/) override
    {
        return true;
    }
    bool number_unsigned(json::number_unsigned_t /*val*/) override
    {
        return true;
    }
    bool number_float(json::number_float_t /*val*/, const json::string_t& s) override
    {
        float_string_copy = s;
        return true;
    }
    bool string(json::string_t& /*val*/) override
    {
        return true;
    }
    bool binary(json::binary_t& /*val*/) override
    {
        return true;
    }
    bool start_object(std::size_t /*val*/) override
    {
        return true;
    }
    bool key(json::string_t& /*val*/) override
    {
        return true;
    }
    bool end_object() override
    {
        return true;
    }
    bool start_array(std::size_t /*val*/) override
    {
        return true;
    }
    bool end_array() override
    {
        return true;
    }
    bool parse_error(std::size_t /*val*/, const std::string& /*val*/, const nlohmann::detail::exception& /*val*/) override
    {
        return false;
    }

    ~ParserImpl() override;

    ParserImpl()
        : float_string_copy("not set")
    {}

    ParserImpl(const ParserImpl& other)
        : float_string_copy(other.float_string_copy)
    {}

    ParserImpl(ParserImpl&& other) noexcept
        : float_string_copy(std::move(other.float_string_copy))
    {}

    ParserImpl& operator=(const ParserImpl& other)
    {
        if (this != &other)
        {
            float_string_copy = other.float_string_copy;
        }
        return *this;
    }

    ParserImpl& operator=(ParserImpl&& other) noexcept
    {
        if (this != &other)
        {
            float_string_copy = std::move(other.float_string_copy);
        }
        return *this;
    }

    json::string_t float_string_copy;
};

ParserImpl::~ParserImpl() = default;

TEST_CASE("locale-dependent test (LC_NUMERIC=C)")
{
    WARN_MESSAGE(std::setlocale(LC_NUMERIC, "C") != nullptr, "could not set locale");

    SECTION("check if locale is properly set")
    {
        std::array<char, 6> buffer = {};
        CHECK(std::snprintf(buffer.data(), buffer.size(), "%.2f", 12.34) == 5); // NOLINT(cppcoreguidelines-pro-type-vararg,hicpp-vararg)
        CHECK(std::string(buffer.data()) == "12.34");
    }

    SECTION("parsing")
    {
        CHECK(json::parse("12.34").dump() == "12.34");
    }

    SECTION("SAX parsing")
    {
        ParserImpl sax {};
        json::sax_parse( "12.34", &sax );
        CHECK(sax.float_string_copy == "12.34");
    }
}

TEST_CASE("locale-dependent test (LC_NUMERIC=de_DE)")
{
    if (std::setlocale(LC_NUMERIC, "de_DE") != nullptr)
    {
        SECTION("check if locale is properly set")
        {
            std::array<char, 6> buffer = {};
            CHECK(std::snprintf(buffer.data(), buffer.size(), "%.2f", 12.34) == 5); // NOLINT(cppcoreguidelines-pro-type-vararg,hicpp-vararg)
            CHECK(std::string(buffer.data()) == "12,34");
        }

        SECTION("parsing")
        {
            CHECK(json::parse("12.34").dump() == "12.34");
        }

        SECTION("SAX parsing")
        {
            ParserImpl sax{};
            json::sax_parse("12.34", &sax);
            CHECK(sax.float_string_copy == "12.34");
        }
    }
    else
    {
        MESSAGE("locale de_DE is not usable");
    }
}

TEST_CASE("numbers do not depend on the locale")
{
    // `ps_AF` is the interesting one: its decimal point is U+066B, which UTF-8 spells with
    // two bytes, so a conversion that assumes a single-byte radix character mangles it
    for (const char* locale_name : {"C", "de_DE", "de_DE.UTF-8", "ps_AF", "ps_AF.UTF-8"})
    {
        if (std::setlocale(LC_NUMERIC, locale_name) == nullptr)
        {
            MESSAGE("locale " << std::string(locale_name) << " is not usable");
            continue;
        }

        CAPTURE(locale_name);

        SECTION("parsing")
        {
            CHECK(json::parse("12.34").get<double>() == 12.34);
            CHECK(json::parse("-1.25e3").get<double>() == -1250.0);
            CHECK(json::parse("1.0e1").get<double>() == 10.0);
            CHECK(json::parse("0.1").get<double>() == 0.1);
            CHECK(json::parse("0.30000000000000004").get<double>() == 0.30000000000000004);
        }

        SECTION("serialization")
        {
            CHECK(json::parse("12.34").dump() == "12.34");
            CHECK(json(12.34).dump() == "12.34");
            CHECK(json(-1250.0).dump() == "-1250.0");
        }

        SECTION("SAX parsing")
        {
            // the text of a floating-point number reaches the consumer verbatim
            ParserImpl sax {};
            json::sax_parse("12.34", &sax);
            CHECK(sax.float_string_copy == "12.34");
        }

        SECTION("a floating-point type that is serialized with snprintf")
        {
            CHECK(json_long_double::parse("12.34").get<long double>() == 12.34L);
            CHECK(json_long_double::parse("-1.25e3").get<long double>() == -1250.0L);

            // only values that a long double holds exactly have a short representation
            CHECK(json_long_double(0.5L).dump() == "0.5");
            CHECK(json_long_double(12.5L).dump() == "12.5");
            CHECK(json_long_double(-1250.0L).dump() == "-1250.0");
            CHECK(json_long_double::parse("0.5").dump() == "0.5");
        }

        SECTION("JSON pointers with numeric reference tokens")
        {
            json array;
            array["/0"_json_pointer] = 42;
            CHECK(array.is_array());
            CHECK(array[0] == 42);
        }
    }

    std::setlocale(LC_NUMERIC, "C");
}
