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
#include <libsolidity/interface/FileReader.h>

#include <liblangutil/Exceptions.h>

#include <libsolutil/CommonIO.h>
#include <libsolutil/Exceptions.h>

#include <boost/algorithm/string/predicate.hpp>

using solidity::frontend::ReadCallback;
using solidity::langutil::InternalCompilerError;
using solidity::util::errinfo_comment;
using solidity::util::readFileAsString;
using std::string;

namespace solidity::frontend
{

void FileReader::setBasePath(boost::filesystem::path const& _path)
{
	if (_path.empty())
		m_basePath = "";
	else
		m_basePath = normalizeCLIPathForVFS(_path);
}

void FileReader::setSource(boost::filesystem::path const& _path, SourceCode _source)
{
	boost::filesystem::path normalizedPath = normalizeCLIPathForVFS(_path);
	boost::filesystem::path prefix = (m_basePath.empty() ? normalizeCLIPathForVFS(".") : m_basePath);

	if (isPathPrefix(prefix, normalizedPath))
		normalizedPath = stripPathPrefix(prefix, normalizedPath);

	m_sourceCodes[normalizedPath.generic_string()] = std::move(_source);
}

void FileReader::setSources(StringMap _sources)
{
	m_sourceCodes = std::move(_sources);
}

ReadCallback::Result FileReader::readFile(string const& _kind, string const& _sourceUnitName)
{
	try
	{
		if (_kind != ReadCallback::kindString(ReadCallback::Kind::ReadFile))
			BOOST_THROW_EXCEPTION(InternalCompilerError() << errinfo_comment(
				"ReadFile callback used as callback kind " +
				_kind
			));
		string strippedSourceUnitName = _sourceUnitName;
		if (strippedSourceUnitName.find("file://") == 0)
			strippedSourceUnitName.erase(0, 7);

		auto canonicalPath = boost::filesystem::weakly_canonical(m_basePath / strippedSourceUnitName);
		bool isAllowed = false;
		for (auto const& allowedDir: m_allowedDirectories)
		{
			// If dir is a prefix of boostPath, we are fine.
			if (
				std::distance(allowedDir.begin(), allowedDir.end()) <= std::distance(canonicalPath.begin(), canonicalPath.end()) &&
				std::equal(allowedDir.begin(), allowedDir.end(), canonicalPath.begin())
			)
			{
				isAllowed = true;
				break;
			}
		}
		if (!isAllowed)
			return ReadCallback::Result{false, "File outside of allowed directories."};

		if (!boost::filesystem::exists(canonicalPath))
			return ReadCallback::Result{false, "File not found."};

		if (!boost::filesystem::is_regular_file(canonicalPath))
			return ReadCallback::Result{false, "Not a valid file."};

		// NOTE: we ignore the FileNotFound exception as we manually check above
		auto contents = readFileAsString(canonicalPath.string());
		m_sourceCodes[_sourceUnitName] = contents;
		return ReadCallback::Result{true, contents};
	}
	catch (util::Exception const& _exception)
	{
		return ReadCallback::Result{false, "Exception in read callback: " + boost::diagnostic_information(_exception)};
	}
	catch (...)
	{
		return ReadCallback::Result{false, "Unknown exception in read callback."};
	}
}

boost::filesystem::path FileReader::normalizeCLIPathForVFS(boost::filesystem::path const& _path)
{
	// NOTE: boost path preserves certain differences that are ignored by its operator ==.
	// E.g. "a//b" vs "a/b" or "a/b/" vs "a/b/.". lexically_normal() does remove these differences.
	boost::filesystem::path normalizedPath =  boost::filesystem::absolute(_path).lexically_normal();

	// lexically_normal() will not squash paths like "/../../" into "/". We have to do it manually.
	boost::filesystem::path dotDotPrefix = absoluteDotDotPrefix(normalizedPath);

	// If the path is on the same drive as the working dir, for portability we prefer not to
	// include the root name. Do do this only for non-UNC paths - my experiments show that when
	// the working dir is an UNC path, / does not not actually refer to the root of the UNC path.
	boost::filesystem::path normalizedRootPath = normalizedPath.root_path();
	if (
		!boost::starts_with(normalizedPath.root_name().string(), "//") &&
		!boost::starts_with(normalizedPath.root_name().string(), "\\\\")
	)
	{
		boost::filesystem::path workingDirRootPath = boost::filesystem::canonical(boost::filesystem::current_path()).root_path();
		if (normalizedPath.root_name() == workingDirRootPath)
			normalizedRootPath = "/";
	}

	boost::filesystem::path normalizedPathNoDotDot = normalizedPath;
	if (dotDotPrefix.empty())
		normalizedPathNoDotDot = normalizedRootPath / normalizedPath.relative_path();
	else
		normalizedPathNoDotDot = normalizedRootPath / normalizedPath.lexically_relative(normalizedPath.root_path() / dotDotPrefix);
	solAssert(!hasDotDotSegments(normalizedPathNoDotDot), "");

	// NOTE: On Windows lexically_normal() converts all separators to forward slashes. Convert them back.
	// Separators do not affect path comparison but remain in internal representation returned by native().
	normalizedPathNoDotDot = normalizedPathNoDotDot.generic_string();

	// For some reason boost considers "/." different than "/" even though for other directories
	// the trailing dot is ignored.
	if (normalizedPathNoDotDot == "/.")
		return "/";

	return normalizedPathNoDotDot;
}

bool FileReader::isPathPrefix(boost::filesystem::path _prefix, boost::filesystem::path const& _path)
{
	solAssert(!_prefix.empty() && !_path.empty(), "");
	solAssert(_prefix.is_absolute() && _path.is_absolute(), "");
	solAssert(_prefix == _prefix.lexically_normal() && _path == _path.lexically_normal(), "");
	solAssert(!hasDotDotSegments(_prefix) && !hasDotDotSegments(_path), "");
	solAssert(!boost::starts_with(_prefix.root_name().string(), "\\\\"), "");
	solAssert(!boost::starts_with(_path.root_name().string(), "\\\\"), "");

	// If a boost path ends with / (i.e. represents a directory), filename() is a dot.
	// Compare lengths before stripping the dot so that /a/b/c.sol/ is not a prefix of /a/b/c.sol.
	long prefixSegmentCount = std::distance(_prefix.begin(), _prefix.end());
	long pathSegmentCount = std::distance(_path.begin(), _path.end());
	if (prefixSegmentCount > pathSegmentCount)
		return false;

	// This ensures that both /a/b and /a/b/ is a prefix of /a/b/c.sol
	if (_prefix.filename() == ".")
		_prefix.remove_filename();

	// NOTE: This compares only as many segments are there are in _prefix and ignores the rest.
	return std::equal(_prefix.begin(), _prefix.end(), _path.begin());
}

boost::filesystem::path FileReader::stripPathPrefix(boost::filesystem::path _prefix, boost::filesystem::path const& _path)
{
	solAssert(isPathPrefix(_prefix, _path), "");

	boost::filesystem::path strippedPath = _path.lexically_relative(_prefix);
	solAssert(strippedPath.empty() || *strippedPath.begin() != "..", "");

	return strippedPath;
}

boost::filesystem::path FileReader::absoluteDotDotPrefix(boost::filesystem::path const& _path)
{
	solAssert(_path.is_absolute(), "");

	boost::filesystem::path _pathWithoutRoot = _path.relative_path();
	boost::filesystem::path prefix;
	for (boost::filesystem::path const& segment: _pathWithoutRoot)
		if (segment.filename_is_dot_dot())
			prefix /= segment;

	return prefix;
}

bool FileReader::hasDotDotSegments(boost::filesystem::path const& _path)
{
	for (boost::filesystem::path const& segment: _path)
		if (segment.filename_is_dot_dot())
			return true;

	return false;
}

}
