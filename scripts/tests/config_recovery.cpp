#include "ConfigRecovery.h"
#include <cassert>
#include <iostream>
using namespace Magpie;
using namespace Magpie::ConfigRecovery;

static std::string Current(std::string_view fields) {
	return R"({"experimentalDlssnrSettingsVersion":2,"experimentalDlssSrSettingsVersion":1,"experimentalDepthRemovalVersion":1,)" +
		std::string(fields) + "}";
}
static Plan PrepareTest(std::string_view source, std::string_view backup = {}) {
	return Prepare(source, backup, "Recovered group");
}
static void Put(const std::filesystem::path& path, std::string_view text) {
	std::ofstream file(path, std::ios::binary | std::ios::trunc);
	file << text; assert(file.good());
}
int wmain(int argc, wchar_t** argv) {
	if (argc == 3 && std::wstring_view(argv[1]) == L"--inspect") {
		auto plan = PrepareTest(ConfigPersistence::Read(argv[2]));
		std::cout << "Read-only configuration inspection: kind=" << static_cast<int>(plan.kind)
			<< " repairedFields=" << plan.fields.size() << "\n";
		for (const auto& field : plan.fields) std::cout << field << '\n';
		return 0;
	}
	assert(argc == 2);
	const auto root = std::filesystem::absolute(argv[1]) / (L"recovery-" + std::to_wstring(GetCurrentProcessId()));
	std::filesystem::create_directories(root);
	const auto config = root / L"config.json";
	const auto valid = Current(R"("language":"zh-hans","shortcuts":{"scale":123},"scalingModes":[{"name":"custom","effects":[{"name":"user-installed-effect","parameters":{"strength":0.7}}]},{"name":"draft"}],"profiles":[{"scalingMode":0},{"name":"app","scalingMode":1}])");
	auto untouched = PrepareTest(valid);
	assert(untouched.kind == Kind::None && untouched.fields.empty());
	const auto withBom = PrepareTest(std::string("\xEF\xBB\xBF") + valid);
	assert(withBom.kind == Kind::None && withBom.document["scalingModes"].Size() == 2);
	assert(untouched.document["scalingModes"].Size() == 2);
	assert(!untouched.document["scalingModes"][1].HasMember("effects"));
	// Missing optional SDKs and legitimate unfinished groups do not trigger recovery.
	assert(std::string_view(untouched.document["scalingModes"][0]["effects"][0]["name"].GetString()) == "user-installed-effect");
	assert(PrepareTest(Current(R"("scalingModes":[],"profiles":[{"scalingMode":-1}])")).kind == Kind::None);
	for (const char* target : { "0", "1", "14.5", "80", "360.5", "1000" }) {
		for (int syncMode = 0; syncMode <= 2; ++syncMode) {
			auto plan = PrepareTest(Current(std::string(R"("scalingModes":[],"frontEdgeSync":true,"frontEdgeSyncFrameRate":)") +
				target + ",\"frameSyncMode\":" + std::to_string(syncMode)));
			assert(plan.kind == Kind::None && plan.document["frameSyncMode"].GetInt() == syncMode);
			assert(plan.document.HasMember("frontEdgeSyncFrameRate"));
		}
	}
	for (const char* value : { "-1", "3", "1.5", "\"Reflex\"", "null" }) {
		auto plan = PrepareTest(Current(std::string(R"("scalingModes":[],"frontEdgeSync":false,"frontEdgeSyncFrameRate":0,"frameSyncMode":)") + value));
		assert(plan.kind == Kind::Repaired && !plan.document.HasMember("frameSyncMode"));
		assert(!plan.document["frontEdgeSync"].GetBool() && plan.document["frontEdgeSyncFrameRate"].GetDouble() == 0);
		assert(PrepareTest(Serialize(plan.document)).kind == Kind::None);
	}
	// Repair only damaged fields, while preserving profile-to-group indices.
	auto repaired = PrepareTest(Current(R"("scalingModes":[42,{"name":"keep","effects":[{"name":"Lanczos","scalingType":99,"scale":{"x":-2,"y":1},"parameters":{"ok":0.5,"overflow":1e50,"bad":"text"}}]}],"profiles":[{"scalingMode":1,"customCursorScaling":1e50},{"scalingMode":1}],"minFrameRate":-5)") );
	assert(repaired.kind == Kind::Repaired && !repaired.defaultModes);
	assert(repaired.document["scalingModes"].Size() == 2);
	assert(repaired.document["profiles"][0]["scalingMode"].GetInt() == 1);
	assert(repaired.document["profiles"][1]["scalingMode"].GetInt() == 1);
	assert(!repaired.document.HasMember("minFrameRate"));
	auto& effect = repaired.document["scalingModes"][1]["effects"][0];
	assert(!effect.HasMember("scalingType") && !effect.HasMember("scale"));
	assert(effect["parameters"].MemberCount() == 1 && effect["parameters"].HasMember("ok"));
	assert(PrepareTest(Serialize(repaired.document)).kind == Kind::None);
	const ParameterRules rules = [](std::string_view effect, std::string_view parameter) -> std::optional<ParameterRule> {
		if (effect == "installed" && parameter == "choice") return ParameterRule{0, 4, 2, true, {0, 2, 4}};
		return std::nullopt;
	};
	for (const char* value : { "99", "1", "2.5" }) {
		auto semantic = Prepare(Current(std::string(R"("scalingModes":[{"name":"keep","effects":[{"name":"installed","parameters":{"choice":)") +
			value + R"(,"extension":100}}]}])"), {}, "Recovered", rules);
		assert(semantic.kind == Kind::Repaired);
		assert(semantic.document["scalingModes"][0]["effects"][0]["parameters"]["choice"].GetFloat() == 2);
		assert(semantic.document["scalingModes"][0]["effects"][0]["parameters"]["extension"].GetInt() == 100);
		assert(Prepare(Serialize(semantic.document), {}, "Recovered", rules).kind == Kind::None);
	}
	auto selection = PrepareTest(Current(R"("scalingModes":[{"name":"keep"}],"profiles":[{"scalingMode":99},{"scalingMode":99}])"));
	assert(selection.document["profiles"][0]["scalingMode"].GetInt() == 0);
	assert(!selection.document["profiles"][1].HasMember("scalingMode"));
	auto missing = PrepareTest(Current(R"("scalingModes":"bad","profiles":[{"scalingMode":3},{"scalingMode":2}])"));
	assert(missing.defaultModes && missing.document["profiles"][0]["scalingMode"].GetInt() == 0);
	assert(missing.document["profiles"][1]["scalingMode"].GetInt() == -1);
	auto fromBackup = PrepareTest("{broken", valid);
	assert(fromBackup.kind == Kind::Backup && fromBackup.document["scalingModes"].Size() == 2);
	auto partial = PrepareTest(R"({"language":"zh-hans","scalingModes":[{"name":"complete"},{"name":"unfinished)" );
	assert(partial.kind == Kind::Partial && partial.document["scalingModes"].Size() == 2);
	assert(PrepareTest("{broken", "also broken").kind == Kind::Defaults);
	assert(PrepareTest(R"({"scalingModes":[{"name":"old"}]})").kind == Kind::None);
	const auto chain = PrepareTest(Current(R"("scalingModes":[{"name":"mixed","effects":[{"name":"keep-a","parameters":{"gain":0.5}},42,{"name":"keep-b"},{}]}],"profiles":[{"scalingMode":0}])"));
	const auto& effects = chain.document["scalingModes"][0]["effects"];
	assert(effects.Size() == 4 && effects[1]["recoveryInvalid"].GetBool() && effects[3]["recoveryInvalid"].GetBool());
	assert(effects[0]["parameters"]["gain"].GetFloat() == 0.5f);
	assert(std::string_view(effects[2]["name"].GetString()) == "keep-b");
	assert(std::string_view(effects[1]["recoveryOriginal"].GetString()) == "42");
	assert(PrepareTest(Serialize(chain.document)).kind == Kind::None);
	const auto truncatedChain = PrepareTest(R"({"scalingModes":[{"name":"retain","effects":[{"name":"keep"},{"name":"cut)" );
	assert(truncatedChain.kind == Kind::Partial);
	const auto& truncatedEffects = truncatedChain.document["scalingModes"][0]["effects"];
	assert(truncatedEffects.Size() == 2 && std::string_view(truncatedEffects[0]["name"].GetString()) == "keep");
	assert(truncatedEffects[1]["recoveryInvalid"].GetBool());
	for (const auto damaged : { R"({"scalingModes":[{"name":"cut)", R"({"language":"zh-hans","scalingModes":[{"name":"cut)" }) {
		const auto firstBroken = PrepareTest(damaged);
		assert(firstBroken.kind == Kind::Partial && !firstBroken.defaultModes);
		assert(firstBroken.document["scalingModes"].Size() == 1);
		assert(firstBroken.document["scalingModes"][0]["effects"][0]["recoveryInvalid"].GetBool());
	}
	const auto closedEffects = PrepareTest(R"({"scalingModes":[{"name":"retain","effects":[{"name":"keep"}])" );
	assert(closedEffects.document["scalingModes"][0]["effects"].Size() == 1);
	const auto wrongArray = PrepareTest(Current(R"("scalingModes":[{"name":"bad","effects":"broken"},{"name":"single","effects":{"name":"keep"}}])"));
	assert(wrongArray.document["scalingModes"][0]["effects"][0]["recoveryInvalid"].GetBool());
	assert(std::string_view(wrongArray.document["scalingModes"][1]["effects"][0]["name"].GetString()) == "keep");
	// Real files: exact original is immutable, and each input gets its own record.
	const std::string damaged = "{broken";
	Put(config, damaged);
	const auto files = FilesFor(config, damaged);
	assert(Preserve(config, damaged, files));
	assert(ConfigPersistence::Read(files.original) == damaged);
	const auto timestamp = std::filesystem::last_write_time(files.original);
	ConfigSaveState cache;
	assert(ConfigPersistence::WriteAtomic(files.result, valid, 1, cache));
	assert(SetFileAttributesW(config.c_str(), FILE_ATTRIBUTE_READONLY));
	ConfigSaveState save;
	assert(!ConfigPersistence::WriteAtomic(config, valid, 1, save));
	assert(GetLastError() == ERROR_ACCESS_DENIED);
	assert(ConfigPersistence::Read(config) == damaged && ConfigPersistence::Read(files.result) == valid);
	assert(SetFileAttributesW(config.c_str(), FILE_ATTRIBUTE_NORMAL));
	// A new process can retry from the stored result without generating new defaults.
	assert(Preserve(config, damaged, files));
	assert(std::filesystem::last_write_time(files.original) == timestamp);
	assert(PrepareTest(ConfigPersistence::Read(files.result)).kind == Kind::None);
	ConfigSaveState retry;
	assert(ConfigPersistence::WriteAtomic(config, ConfigPersistence::Read(files.result), 1, retry));
	assert(ConfigPersistence::Read(config) == valid);
	assert(FilesFor(config, damaged + " ").original != files.original);
	// A hash/path collision or edited original is rejected using exact bytes.
	Put(config, "different");
	assert(!Preserve(config, "different", files));
	assert(GetLastError() == ERROR_INVALID_DATA && ConfigPersistence::Read(files.original) == damaged);
	std::cout << "Configuration recovery: selective repairs, drafts, profile indices, backup/partial/default recovery, "
		"migration detection, immutable originals and persistence retry passed.\n";
}
