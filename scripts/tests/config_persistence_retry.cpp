#include "ConfigPersistence.h"
#include <cassert>
#include <iostream>

using namespace Magpie;

// Run against an isolated test directory, never a user's Magpie configuration.
int wmain(int argc, wchar_t** argv) {
	assert(argc == 2);
	const auto root = std::filesystem::absolute(argv[1]) /
		(L"config-retry-" + std::to_wstring(GetCurrentProcessId()));
	std::filesystem::create_directories(root);
	const auto path = root / L"config.json";
	const auto temporary = root / L"config.json.tmp";
	const auto backup = root / L"config.json.bak";
	constexpr std::string_view first = R"({"setting":1,"profiles":[{"name":"original"}]})";
	constexpr std::string_view changed = R"({"setting":2,"profiles":[{"name":"new value"}]})";
	constexpr std::string_view latest = R"({"setting":3,"profiles":[{"name":"latest"}]})";
	ConfigSaveState state;
	assert(ConfigPersistence::WriteAtomic(path, first, 1, state));
	assert(state.savedRevision == 1 && ConfigPersistence::Read(path) == first);

	// A real Windows sharing violation leaves the prior configuration intact.
	HANDLE lock = CreateFileW(temporary.c_str(), GENERIC_READ | GENERIC_WRITE,
		0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	assert(lock != INVALID_HANDLE_VALUE);
	assert(!ConfigPersistence::WriteAtomic(path, changed, 2, state));
	assert(GetLastError() == ERROR_SHARING_VIOLATION);
	assert(state.savedRevision == 1 && ConfigPersistence::Read(path) == first);
	assert(CloseHandle(lock));

	// Retrying the same unsaved revision requires no unrelated setting edit.
	assert(ConfigPersistence::WriteAtomic(path, changed, 2, state));
	assert(state.savedRevision == 2 && ConfigPersistence::Read(path) == changed);
	assert(ConfigPersistence::Read(backup) == first);

	// A late async save must not overwrite a newer explicit retry.
	assert(ConfigPersistence::WriteAtomic(path, latest, 4, state));
	assert(ConfigPersistence::WriteAtomic(path, first, 3, state));
	assert(state.savedRevision == 4 && ConfigPersistence::Read(path) == latest);
	assert(ConfigPersistence::Read(backup) == changed);

	assert(!ConfigPersistence::WriteAtomic(path, "{broken", 5, state));
	assert(GetLastError() == ERROR_INVALID_DATA);
	assert(state.savedRevision == 4 && ConfigPersistence::Read(path) == latest);

	// Failed replacement also preserves the original and can be retried.
	assert(SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_READONLY));
	assert(!ConfigPersistence::WriteAtomic(path, changed, 6, state));
	assert(GetLastError() == ERROR_ACCESS_DENIED);
	assert(state.savedRevision == 4 && ConfigPersistence::Read(path) == latest);
	assert(SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_NORMAL));
	assert(ConfigPersistence::WriteAtomic(path, changed, 6, state));
	assert(state.savedRevision == 6 && ConfigPersistence::Read(path) == changed);

	// Recovery retains complete entries, never a partially written profile.
	const auto recovered = ConfigPersistence::RecoverPrefix(
		R"({"setting":9,"profiles":[{"name":"complete"},{"name":"unfinished)" );
	assert(ConfigPersistence::IsValid(recovered));
	rapidjson::Document doc;
	doc.Parse(recovered.c_str());
	assert(doc["setting"].GetInt() == 9);
	assert(doc["profiles"].Size() == 1);
	assert(std::string_view(doc["profiles"][0]["name"].GetString()) == "complete");
	assert(ConfigPersistence::RecoverPrefix(R"({"unfinished":")").empty());

	// Restoring damaged input keeps the last usable backup available.
	assert(ConfigPersistence::Read(backup) == latest);
	{
		std::ofstream damaged(path, std::ios::binary | std::ios::trunc);
		damaged << "{broken";
		assert(damaged.good());
	}
	assert(ConfigPersistence::WriteAtomic(path, recovered, 7, state));
	assert(ConfigPersistence::Read(backup) == latest);
	assert(ConfigPersistence::Read(path) == recovered);
	std::cout << "Config persistence: sharing failure, same-revision retry, stale save, invalid JSON, "
		"read-only replacement, partial recovery and backup preservation passed.\n";
}
