#include "ConfigLocations.h"
#include <cassert>
#include <fstream>
#include <iostream>

struct Win32Helper {
	static bool DirExists(const wchar_t*) noexcept;
	static bool CreateDir(const std::wstring&, bool) noexcept;
};

// Extracted from the production implementation by Run-ConfigMigrationTests.ps1.
#include "ConfigDirectoryHelpers.inc"

static void Native(const std::filesystem::path& path) {
	assert(path.native().find(L'/') == std::wstring::npos);
}

static void Verify(const Magpie::ConfigLocations::Selection& selected) {
	assert(!selected.error);
	Native(selected.source);
	Native(selected.destination);
	// Match startup: create directories before the first settings save.
	assert(Win32Helper::CreateDir(selected.destination.parent_path().native(), true));
	assert(std::filesystem::is_directory(selected.destination.parent_path()));
	assert(Win32Helper::CreateDir(selected.destination.parent_path().native(), true));
}

int wmain(int argc, wchar_t** argv) {
	assert(argc == 2);
	const auto root = std::filesystem::absolute(argv[1]) /
		(L"config-paths 中文 " + std::to_wstring(GetCurrentProcessId()));
	const auto exe = root / L"app";
	const auto user = root / L"Local AppData";
	std::filesystem::create_directories(exe);
	std::filesystem::create_directories(user);
	using namespace Magpie::ConfigLocations;

	// A clean user has no Magpie/config/v4e parents yet.
	auto selected = Select(exe, user);
	assert(selected.source.empty());
	Verify(selected);
	assert(selected.destination.native() == user.native() + L"\\Magpie\\config\\v4e\\config.json");

	// An upstream v4 file must be discovered through a native path and retained.
	const auto legacy = user / L"Magpie" / L"config" / L"v4" / L"config.json";
	std::filesystem::create_directories(legacy.parent_path());
	{ std::ofstream file(legacy); file << "{}"; }
	selected = Select(exe, user);
	assert(selected.source == legacy);
	Verify(selected);
	assert(std::filesystem::is_regular_file(legacy));

	// Normalize root inputs as well, including legacy candidates.
	selected = Select(exe.generic_wstring(), user.generic_wstring());
	assert(selected.source.native() == legacy.native());
	Verify(selected);
	std::filesystem::rename(legacy, legacy.native() + L".bak");
	selected = Select(exe.generic_wstring(), user.generic_wstring());
	assert(selected.source.native() == legacy.native() + L".bak");
	Verify(selected);

	const auto portableLegacy = exe / L"config" / L"config.json";
	std::filesystem::create_directories(portableLegacy.parent_path());
	{ std::ofstream file(portableLegacy); file << "{}"; }
	selected = Select(exe.generic_wstring(), user.generic_wstring());
	assert(selected.portable && selected.source.native() == portableLegacy.native());
	Verify(selected);
	assert(selected.destination.native() == exe.native() + L"\\config\\v4e\\config.json");
	{ std::ofstream file(selected.destination); file << "{}"; }
	selected = Select(exe.generic_wstring(), user.generic_wstring());
	assert(selected.portable && selected.source == selected.destination);
	Verify(selected);

	// Network roots retain their UNC prefix; no network access is needed here.
	const auto network = Directory(L"//server/share/app", L"//server/share/user", false);
	assert(network.native() == L"\\\\server\\share\\user\\Magpie\\config\\v4e");
	std::cout << "Native configuration paths: clean startup, existing directories, v4 import/backup, portable mode and Unicode roots passed.\n";
}
