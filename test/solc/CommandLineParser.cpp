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

/// Unit tests for solc/CommandLineParser.h

#include <solc/CommandLineParser.h>

#include <test/Common.h>
#include <test/libsolidity/util/SoltestErrors.h>

#include <libsolutil/CommonData.h>
#include <liblangutil/EVMVersion.h>
#include <libsolidity/interface/Version.h>

#include <boost/algorithm/string.hpp>
#include <boost/test/unit_test.hpp>

#include <map>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
using namespace solidity::frontend;
using namespace solidity::langutil;
using namespace solidity::util;
using namespace solidity::yul;

namespace
{

optional<CommandLineOptions> parseCommandLine(vector<string> const& commandLine, ostream& _stdout, ostream& _stderr)
{
	size_t argc = commandLine.size();
	vector<char const*> argv(commandLine.size() + 1);

	// argv[argc] typically contains NULL
	argv[argc] = nullptr;

	for (size_t i = 0; i < argc; ++i)
		argv[i] = commandLine[i].c_str();

	CommandLineParser cliParser(_stdout, _stderr);
	bool success = cliParser.parse(
		static_cast<int>(argc),
		argv.data(),
		/* interactiveTerminal*/ false
	);

	if (!success)
		return nullopt;
	else
		return cliParser.options();
}


} // namespace

namespace solidity::frontend::test
{

BOOST_AUTO_TEST_SUITE(CommandLineParserTest)

BOOST_AUTO_TEST_CASE(no_options)
{
	vector<string> commandLine = {"solc", "contract.sol"};

	CommandLineOptions expectedOptions;
	expectedOptions.sourceFilePaths = {"contract.sol"};
	expectedOptions.expectedExecutionsPerDeployment = 200;
	expectedOptions.initializeModelChecker = true;
	expectedOptions.modelCheckerSettings = {
		ModelCheckerContracts::Default(),
		ModelCheckerEngine::None(),
		ModelCheckerTargets::Default(),
		nullopt,
	};

	stringstream sout, serr;
	optional<CommandLineOptions> parsedOptions = parseCommandLine(commandLine, sout, serr);

	BOOST_TEST(sout.str() == "");
	BOOST_TEST(serr.str() == "");
	BOOST_REQUIRE(parsedOptions.has_value());
	BOOST_TEST((parsedOptions.value() == expectedOptions));
}

BOOST_AUTO_TEST_CASE(help)
{
	stringstream sout, serr;
	optional<CommandLineOptions> parsedOptions = parseCommandLine({"solc", "--help"}, sout, serr);

	BOOST_TEST(serr.str() == "");
	BOOST_TEST(boost::starts_with(sout.str(), "solc, the Solidity commandline compiler."));
	BOOST_TEST(sout.str().find("Usage: solc [options] [input_file...]") != string::npos);
	BOOST_TEST(!parsedOptions.has_value());
}

BOOST_AUTO_TEST_CASE(cli_mode_options)
{
	for (InputMode inputMode: {InputMode::Compiler, InputMode::CompilerWithASTImport})
	{
		vector<string> commandLine = {
			"solc",
			"contract.sol",             // Both modes do not care about file names, just about
			"/tmp/projects/token.sol",  // their content. They also both support stdin.
			"/home/user/lib/dex.sol",
			"file",
			"input.json",
			"-",
			"/tmp=/usr/lib/",
			"a:b=c/d",
			":contract.sol=",
			"--base-path=/home/user/",
			"--allow-paths=/tmp,/home,project,../contracts",
			"--ignore-missing",
			"--error-recovery",
			"--output-dir=/tmp/out",
			"--overwrite",
			"--evm-version=spuriousDragon",
			"--experimental-via-ir",
			"--revert-strings=strip",
			"--pretty-json",
			"--no-color",
			"--error-codes",
			"--libraries="
				"dir1/file1.sol:L=0x1234567890123456789012345678901234567890,"
				"dir2/file2.sol:L=0x1111122222333334444455555666667777788888",
			"--ast-compact-json", "--asm", "--asm-json", "--opcodes", "--bin", "--bin-runtime", "--abi",
			"--ir", "--ir-optimized", "--ewasm", "--hashes", "--userdoc", "--devdoc", "--metadata", "--storage-layout",
			"--gas",
			"--combined-json="
				"abi,metadata,bin,bin-runtime,opcodes,asm,storage-layout,generated-sources,generated-sources-runtime,"
				"srcmap,srcmap-runtime,function-debug,function-debug-runtime,hashes,devdoc,userdoc,ast",
			"--metadata-hash=swarm",
			"--metadata-literal",
			"--optimize",
			"--optimize-runs=1000",
			"--yul-optimizations=agf",
			"--model-checker-contracts=contract1.yul:A,contract2.yul:B",
			"--model-checker-engine=bmc",
			"--model-checker-targets=underflow,divByZero",
			"--model-checker-timeout=5",
		};

		if (inputMode == InputMode::CompilerWithASTImport)
			commandLine += vector<string>{
				"--import-ast",
			};

		CommandLineOptions expectedOptions;
		expectedOptions.inputMode = inputMode;
		expectedOptions.sourceFilePaths = {"contract.sol", "/tmp/projects/token.sol", "/home/user/lib/dex.sol", "file", "input.json"};
		expectedOptions.remappings = {
			{"", "/tmp", "/usr/lib/"},
			{"a", "b", "c/d"},
			{"", "contract.sol", ""},
		};
		expectedOptions.addStdin = true;
		expectedOptions.basePath = "/home/user/";
		expectedOptions.allowedDirectories = {"/tmp", "/home", "project", "../contracts", "", "c", "/usr/lib"};
		expectedOptions.ignoreMissingInputFiles = true;
		expectedOptions.errorRecovery = (inputMode == InputMode::Compiler);
		expectedOptions.evmVersion = EVMVersion::spuriousDragon();
		expectedOptions.experimentalViaIR = true;
		expectedOptions.revertStrings = RevertStrings::Strip;
		expectedOptions.outputDir = "/tmp/out";
		expectedOptions.overwriteFiles = true;
		expectedOptions.evmVersion = EVMVersion::spuriousDragon();
		expectedOptions.experimentalViaIR = true;
		expectedOptions.revertStrings = RevertStrings::Strip;
		expectedOptions.prettyJson = true;
		expectedOptions.coloredOutput = false;
		expectedOptions.withErrorIds = true;
		expectedOptions.libraries = {
			{"dir1/file1.sol:L", h160("1234567890123456789012345678901234567890")},
			{"dir2/file2.sol:L", h160("1111122222333334444455555666667777788888")},
		};
		expectedOptions.selectedOutputs = {
			true, true, true, true, true,
			true, true, true, true, true,
			true, true, true, true, true,
		};
		expectedOptions.estimateGas = true;
		expectedOptions.combinedJsonRequests = {
			true, true, true, true, true,
			true, true, true, true, true,
			true, true, true, true, true,
			true, true,
		};
		expectedOptions.metadataHash = CompilerStack::MetadataHash::Bzzr1;
		expectedOptions.metadataLiteral = true;
		expectedOptions.optimize = true;
		expectedOptions.expectedExecutionsPerDeployment = 1000;
		expectedOptions.yulOptimiserSteps = "agf";

		expectedOptions.initializeModelChecker = true;
		expectedOptions.modelCheckerSettings = {
			{{{"contract1.yul", {"A"}}, {"contract2.yul", {"B"}}}},
			{true, false},
			{{VerificationTargetType::Underflow, VerificationTargetType::DivByZero}},
			5,
		};

		stringstream sout, serr;
		optional<CommandLineOptions> parsedOptions = parseCommandLine(commandLine, sout, serr);

		BOOST_TEST(sout.str() == "");
		BOOST_TEST(serr.str() == "");
		BOOST_REQUIRE(parsedOptions.has_value());
		BOOST_TEST((parsedOptions.value() == expectedOptions));
	}
}

BOOST_AUTO_TEST_CASE(assembly_mode_options)
{
	static vector<tuple<vector<string>, AssemblyStack::Machine, AssemblyStack::Language>> const allowedCombinations = {
		{{"--machine=ewasm", "--yul-dialect=ewasm", "--assemble"}, AssemblyStack::Machine::Ewasm, AssemblyStack::Language::Ewasm},
		{{"--machine=ewasm", "--yul-dialect=ewasm", "--yul"}, AssemblyStack::Machine::Ewasm, AssemblyStack::Language::Ewasm},
		{{"--machine=ewasm", "--yul-dialect=ewasm", "--strict-assembly"}, AssemblyStack::Machine::Ewasm, AssemblyStack::Language::Ewasm},
		{{"--machine=ewasm", "--yul-dialect=evm", "--assemble"}, AssemblyStack::Machine::Ewasm, AssemblyStack::Language::StrictAssembly},
		{{"--machine=ewasm", "--yul-dialect=evm", "--yul"}, AssemblyStack::Machine::Ewasm, AssemblyStack::Language::StrictAssembly},
		{{"--machine=ewasm", "--yul-dialect=evm", "--strict-assembly"}, AssemblyStack::Machine::Ewasm, AssemblyStack::Language::StrictAssembly},
		{{"--machine=ewasm", "--strict-assembly"}, AssemblyStack::Machine::Ewasm, AssemblyStack::Language::Ewasm},
		{{"--machine=evm", "--yul-dialect=evm", "--assemble"}, AssemblyStack::Machine::EVM, AssemblyStack::Language::StrictAssembly},
		{{"--machine=evm", "--yul-dialect=evm", "--yul"}, AssemblyStack::Machine::EVM, AssemblyStack::Language::StrictAssembly},
		{{"--machine=evm", "--yul-dialect=evm", "--strict-assembly"}, AssemblyStack::Machine::EVM, AssemblyStack::Language::StrictAssembly},
		{{"--machine=evm", "--assemble"}, AssemblyStack::Machine::EVM, AssemblyStack::Language::Assembly},
		{{"--machine=evm", "--yul"}, AssemblyStack::Machine::EVM, AssemblyStack::Language::Yul},
		{{"--machine=evm", "--strict-assembly"}, AssemblyStack::Machine::EVM, AssemblyStack::Language::StrictAssembly},
		{{"--assemble"}, AssemblyStack::Machine::EVM, AssemblyStack::Language::Assembly},
		{{"--yul"}, AssemblyStack::Machine::EVM, AssemblyStack::Language::Yul},
		{{"--strict-assembly"}, AssemblyStack::Machine::EVM, AssemblyStack::Language::StrictAssembly},
	};

	for (auto const& [assemblyOptions, expectedMachine, expectedLanguage]: allowedCombinations)
	{
		vector<string> commandLine = {
			"solc",
			"contract.yul",
			"/tmp/projects/token.yul",
			"/home/user/lib/dex.yul",
			"file",
			"input.json",
			"-",
			"/tmp=/usr/lib/",
			"a:b=c/d",
			":contract.yul=",
			"--base-path=/home/user/",
			"--allow-paths=/tmp,/home,project,../contracts",
			"--ignore-missing",
			"--error-recovery",            // Ignored in assembly mode
			"--overwrite",
			"--evm-version=spuriousDragon",
			"--experimental-via-ir",       // Ignored in assembly mode
			"--revert-strings=strip",      // Accepted but has no effect in assembly mode
			"--pretty-json",
			"--no-color",
			"--error-codes",
			"--libraries="
				"dir1/file1.sol:L=0x1234567890123456789012345678901234567890,"
				"dir2/file2.sol:L=0x1111122222333334444455555666667777788888",
			"--metadata-hash=swarm",       // Ignored in assembly mode
			"--metadata-literal",          // Ignored in assembly mode
			"--model-checker-contracts="   // Ignored in assembly mode
				"contract1.yul:A,"
				"contract2.yul:B",
			"--model-checker-engine=bmc",  // Ignored in assembly mode
			"--model-checker-targets="     // Ignored in assembly mode
				"underflow,"
				"divByZero",
			"--model-checker-timeout=5",   // Ignored in assembly mode

			// Accepted but has no effect in assembly mode
			"--ast-compact-json", "--asm", "--asm-json", "--opcodes", "--bin", "--bin-runtime", "--abi",
			"--ir", "--ir-optimized", "--ewasm", "--hashes", "--userdoc", "--devdoc", "--metadata", "--storage-layout",
		};
		commandLine += assemblyOptions;
		if (expectedLanguage == AssemblyStack::Language::StrictAssembly || expectedLanguage == AssemblyStack::Language::Ewasm)
			commandLine += vector<string>{
				"--optimize",
				"--optimize-runs=1000",    // Ignored in assembly mode
				"--yul-optimizations=agf",
			};

		CommandLineOptions expectedOptions;
		expectedOptions.inputMode = InputMode::Assembler;

		expectedOptions.sourceFilePaths = {"contract.yul", "/tmp/projects/token.yul", "/home/user/lib/dex.yul", "file", "input.json"};
		expectedOptions.remappings = {
			{"", "/tmp", "/usr/lib/"},
			{"a", "b", "c/d"},
			{"", "contract.yul", ""},
		};
		expectedOptions.addStdin = true;
		expectedOptions.basePath = "/home/user/";
		expectedOptions.allowedDirectories = {"/tmp", "/home", "project", "../contracts", "", "c", "/usr/lib"};
		expectedOptions.ignoreMissingInputFiles = true;
		expectedOptions.overwriteFiles = true;
		expectedOptions.evmVersion = EVMVersion::spuriousDragon();
		expectedOptions.revertStrings = RevertStrings::Strip;
		expectedOptions.prettyJson = true;
		expectedOptions.coloredOutput = false;
		expectedOptions.withErrorIds = true;
		expectedOptions.targetMachine = expectedMachine;
		expectedOptions.inputAssemblyLanguage = expectedLanguage;
		expectedOptions.libraries = {
			{"dir1/file1.sol:L", h160("1234567890123456789012345678901234567890")},
			{"dir2/file2.sol:L", h160("1111122222333334444455555666667777788888")},
		};
		expectedOptions.selectedOutputs = {
			true, true, true, true, true,
			true, true, true, true, true,
			true, true, true, true, true,
		};
		if (expectedLanguage == AssemblyStack::Language::StrictAssembly || expectedLanguage == AssemblyStack::Language::Ewasm)
		{
			expectedOptions.optimize = true;
			expectedOptions.yulOptimiserSteps = "agf";
		}

		stringstream sout, serr;
		optional<CommandLineOptions> parsedOptions = parseCommandLine(commandLine, sout, serr);

		BOOST_TEST(sout.str() == "");
		BOOST_TEST(serr.str() == "Warning: Yul is still experimental. Please use the output with care.\n");
		BOOST_REQUIRE(parsedOptions.has_value());

		BOOST_TEST((parsedOptions.value() == expectedOptions));
	}
}

BOOST_AUTO_TEST_CASE(standard_json_mode_options)
{
	vector<string> commandLine = {
		"solc",
		"input.json",
		"--standard-json",
		"--base-path=/home/user/",
		"--allow-paths=/tmp,/home,project,../contracts",
		"--ignore-missing",
		"--error-recovery",                // Ignored in Standard JSON mode
		"--output-dir=/tmp/out",           // Accepted but has no effect in Standard JSON mode
		"--overwrite",                     // Accepted but has no effect in Standard JSON mode
		"--evm-version=spuriousDragon",    // Ignored in Standard JSON mode
		"--experimental-via-ir",           // Ignored in Standard JSON mode
		"--revert-strings=strip",          // Accepted but has no effect in Standard JSON mode
		"--pretty-json",                   // Accepted but has no effect in Standard JSON mode
		"--no-color",                      // Accepted but has no effect in Standard JSON mode
		"--error-codes",                   // Accepted but has no effect in Standard JSON mode
		"--libraries="                     // Ignored in Standard JSON mode
			"dir1/file1.sol:L=0x1234567890123456789012345678901234567890,"
			"dir2/file2.sol:L=0x1111122222333334444455555666667777788888",
		"--gas",                           // Accepted but has no effect in Standard JSON mode
		"--combined-json=abi,bin",         // Accepted but has no effect in Standard JSON mode
		"--metadata-hash=swarm",           // Ignored in Standard JSON mode
		"--metadata-literal",              // Ignored in Standard JSON mode
		"--optimize",                      // Ignored in Standard JSON mode
		"--optimize-runs=1000",            // Ignored in Standard JSON mode
		"--yul-optimizations=agf",
		"--model-checker-contracts="       // Ignored in Standard JSON mode
			"contract1.yul:A,"
			"contract2.yul:B",
		"--model-checker-engine=bmc",      // Ignored in Standard JSON mode
		"--model-checker-targets="         // Ignored in Standard JSON mode
			"underflow,"
			"divByZero",
		"--model-checker-timeout=5",       // Ignored in Standard JSON mode

		// Accepted but has no effect in Standard JSON mode
		"--ast-compact-json", "--asm", "--asm-json", "--opcodes", "--bin", "--bin-runtime", "--abi",
		"--ir", "--ir-optimized", "--ewasm", "--hashes", "--userdoc", "--devdoc", "--metadata", "--storage-layout",
	};

	CommandLineOptions expectedOptions;
	expectedOptions.inputMode = InputMode::StandardJson;
	expectedOptions.sourceFilePaths = {"input.json"};
	expectedOptions.basePath = "/home/user/";
	expectedOptions.allowedDirectories = {"/tmp", "/home", "project", "../contracts"};
	expectedOptions.ignoreMissingInputFiles = true;
	expectedOptions.outputDir = "/tmp/out";
	expectedOptions.overwriteFiles = true;
	expectedOptions.prettyJson = true;
	expectedOptions.coloredOutput = false;
	expectedOptions.withErrorIds = true;
	expectedOptions.revertStrings = RevertStrings::Strip;
	expectedOptions.selectedOutputs = {
		true, true, true, true, true,
		true, true, true, true, true,
		true, true, true, true, true,
	};
	expectedOptions.estimateGas = true;
	expectedOptions.combinedJsonRequests = CombinedJsonRequests{};
	expectedOptions.combinedJsonRequests->abi = true;
	expectedOptions.combinedJsonRequests->binary = true;

	stringstream sout, serr;
	optional<CommandLineOptions> parsedOptions = parseCommandLine(commandLine, sout, serr);

	BOOST_TEST(sout.str() == "");
	BOOST_TEST(serr.str() == "");
	BOOST_REQUIRE(parsedOptions.has_value());
	BOOST_TEST((parsedOptions.value() == expectedOptions));
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace solidity::frontend::test
