/*
	This file is part of solidity.

	solidity is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	solidity is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with solidity.  If not, see <http://www.gnu.org/licenses/>.
*/
// SPDX-License-Identifier: GPL-3.0
/**
 * @author Yoichi Hirai <yoichi@ethereum.org>
 * @date 2016
 * Unit tests for the SourceLocation class.
 */

#include <liblangutil/SourceLocation.h>
#include <liblangutil/DoxyStyleComments.h>

#include <test/Common.h>

#include <boost/test/unit_test.hpp>

#include <array>

using namespace std;
using namespace std::string_view_literals;

namespace solidity::langutil::test
{

BOOST_AUTO_TEST_SUITE(DoxyStyleCommentsIterator)

BOOST_AUTO_TEST_CASE(empty)
{
	auto const p = splitDoxyStyleComments("");
	auto current = p.begin();
	BOOST_CHECK(current == p.end());

	auto [key, value, ok] = *current;
	BOOST_CHECK(!ok);
	BOOST_CHECK_EQUAL(key, "");
	BOOST_CHECK_EQUAL(value, "");
}

BOOST_AUTO_TEST_CASE(single_single_line)
{
	auto const p = splitDoxyStyleComments("@greeting Hello World");
	auto current = p.begin();

	auto [key, value, ok] = *current;
	BOOST_CHECK(ok);
	BOOST_CHECK_EQUAL(key, "greeting");
	BOOST_CHECK_EQUAL(value, "Hello World");

	tie(key, value, ok) = *++current;
	BOOST_CHECK(ok);
	BOOST_CHECK_EQUAL(key, "");
	BOOST_CHECK_EQUAL(value, "");
	BOOST_CHECK(current == p.end());
}

BOOST_AUTO_TEST_CASE(key_empty)
{
	auto [key, value, ok] = *splitDoxyStyleComments("@ Some Value").begin();
	BOOST_CHECK(!ok);
	BOOST_CHECK_EQUAL(key, "");
	BOOST_CHECK_EQUAL(value, "");
}

BOOST_AUTO_TEST_CASE(key_with_at_symbol)
{
	auto [key, value, ok] = *splitDoxyStyleComments("@key-with-@ has a value").begin();
	BOOST_CHECK(ok);
	BOOST_CHECK_EQUAL(key, "key-with-@");
	BOOST_CHECK_EQUAL(value, "has a value");
}

BOOST_AUTO_TEST_CASE(value_empty)
{
	auto [key, value, ok] = *splitDoxyStyleComments("@x-key").begin();
	BOOST_CHECK(ok);
	BOOST_CHECK_EQUAL(key, "x-key");
	BOOST_CHECK_EQUAL(value, "");
}

BOOST_AUTO_TEST_CASE(value_with_at_symbol)
{
	auto [key, value, ok] = *splitDoxyStyleComments("@key some@here").begin();
	BOOST_CHECK(ok);
	BOOST_CHECK_EQUAL(key, "key");
	BOOST_CHECK_EQUAL(value, "some@here");
}

BOOST_AUTO_TEST_CASE(value_space_trimmed)
{
	auto [key, value, ok] = *splitDoxyStyleComments("@key  \t  Some \tText  \t  ").begin();
	BOOST_CHECK(ok);
	BOOST_CHECK_EQUAL(key, "key");
	BOOST_CHECK_EQUAL(value, "Some  \tText");
}

BOOST_AUTO_TEST_CASE(multiline_entries)
{
	auto const p = splitDoxyStyleComments(
		"@say-greeting Hello World\n"
		"@say-chat     Some more text with @'s up and until \"here\"!  \r\n"
		"@say-farewell Good bye."
	);
	auto current = p.begin();

	auto [key, value, ok] = *current;
	BOOST_CHECK(ok);
	BOOST_CHECK_EQUAL(key, "say-greeting");
	BOOST_CHECK_EQUAL(value, "Hello World");

	tie(key, value, ok) = *++current;
	BOOST_CHECK(ok);
	BOOST_CHECK_EQUAL(key, "say-chat");
	BOOST_CHECK_EQUAL(value, "Some more text with @'s up and until \"here\"!");

	tie(key, value, ok) = *++current;
	BOOST_CHECK(ok);
	BOOST_CHECK_EQUAL(key, "say-farewell");
	BOOST_CHECK_EQUAL(value, "Good bye.");

	tie(key, value, ok) = *++current;
	BOOST_CHECK(ok);
	BOOST_CHECK_EQUAL(key, "");
	BOOST_CHECK_EQUAL(value, "");
	BOOST_CHECK(current == p.end());
}

BOOST_AUTO_TEST_CASE(for_loop_single_line)
{
	auto const fields = array<tuple<string_view, string_view>, 3>{
		tuple{ "src"sv, "0:123:432"sv },
	};

	auto const inputText = "@src 0:123:432";

	size_t i = 0;
	for (auto const [key, value, ok]: splitDoxyStyleComments(inputText))
	{
		BOOST_CHECK(ok);
		BOOST_CHECK_EQUAL(key, get<0>(fields.at(i)));
		BOOST_CHECK_EQUAL(value, get<1>(fields.at(i)));
		++i;
	}
}
BOOST_AUTO_TEST_CASE(for_loop_multiline)
{
	auto const fields = array<tuple<string_view, string_view>, 3>{
		tuple{ "say-greeting"sv, "Hello World"sv },
		tuple{ "say-chat"sv, "Some more text with @'s up and until \"here\"!"sv },
		tuple{ "say-farewell"sv, "Good bye."sv }
	};

	auto const inputText = R"(
		@say-greeting Hello World
		@say-chat     Some more text with @'s up and until "here"!
		@say-farewell Good bye.
	)";

	size_t i = 0;
	for (auto const [key, value, ok]: splitDoxyStyleComments(inputText))
	{
		BOOST_CHECK(ok);
		BOOST_CHECK_EQUAL(key, get<0>(fields.at(i)));
		BOOST_CHECK_EQUAL(value, get<1>(fields.at(i)));
		++i;
	}
}

BOOST_AUTO_TEST_SUITE_END()

} // end namespaces
