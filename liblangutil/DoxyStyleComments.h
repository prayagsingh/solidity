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
#pragma once

#include <regex>
#include <string_view>
#include <tuple>

namespace solidity::langutil
{

/// Splits doxygen-style @key/value pairs from @p _text.
///
/// No validation is performed on key or value, except:
/// - Each line may contain a key/value pair.
/// - Keys must start with '@' (but are not exposed with their leading '@').
/// - Key key name currently may contain any character except a leading '@' and a trailing space (0x20).
/// - Values are space-trimmed on both sides are located on the right side of the key.
/// - Currently values CANNOT spam multiple lines, only single lines.
inline auto splitDoxyStyleComments(std::string_view _text)
{
	struct KeyValuePairsParser
	{
		explicit KeyValuePairsParser(std::string_view _text): m_text{_text} {}

		struct iterator
		{
			explicit iterator(std::string_view _text) noexcept: m_text{_text} { ++*this; }

			constexpr bool operator==(iterator const& _other) const noexcept
			{
				return m_text == _other.m_text && m_ok == _other.m_ok;
			}
			constexpr bool operator!=(iterator const& _other) const noexcept { return !(*this == _other); }

			constexpr auto operator*() const noexcept { return std::tuple{m_key, m_value, m_ok}; }

			constexpr iterator& invalidate() noexcept
			{
					m_ok = false;
					m_text = {};
					m_key = {};
					m_value = {};
					return *this;
			}

			// Consumes ONE @key/value pair.
			iterator& operator++()
			{
				if (m_text.empty())
					return invalidate();

				static std::regex const re = std::regex(
					R"~~~(^\s*@([^\s]+)\s*(.*)(\r\n|\n|$))~~~",
					std::regex_constants::ECMAScript | std::regex_constants::optimize
				);

				std::cmatch cm;
				if (!std::regex_search(m_text.data(), m_text.data() + m_text.size(), cm, re))
					return invalidate();

				m_key = std::string_view{cm[1].first, static_cast<size_t>(cm[1].length())};
				m_value = std::string_view{cm[2].first, static_cast<size_t>(cm[2].length())};
				while (!m_value.empty() && std::isspace(m_value.back()))
					m_value.remove_suffix(1);

				// consume line and consume consecutive newlines, empty lines, and leading spaces
				m_text.remove_prefix(static_cast<size_t>(cm[0].length()));
				while (!m_text.empty() && std::isspace(m_text.empty()))
					m_text.remove_prefix(1);

				m_ok = true;
				return *this;
			}

			std::string_view m_text;
			std::string_view m_key;
			std::string_view m_value;
			bool m_ok = false;
		};

		iterator begin() const noexcept { return iterator(m_text); }
		iterator end() const noexcept { return iterator({}); }

		std::string_view m_text;
	};
	return KeyValuePairsParser{_text};
}


} // end namespace
