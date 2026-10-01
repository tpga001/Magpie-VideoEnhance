#pragma once
#include "Event.h"
#include <rapidjson/prettywriter.h>
#include <rapidjson/document.h>

namespace Magpie {

struct ScalingMode;

enum class EffectAddedWay {
	Add,
	Duplicate,
	Import
};

class ScalingModesService {
public:
	static ScalingModesService& Get() noexcept {
		static ScalingModesService instance;
		return instance;
	}

	ScalingModesService(const ScalingModesService&) = delete;
	ScalingModesService(ScalingModesService&&) = delete;

	ScalingMode& GetScalingMode(uint32_t idx);

	uint32_t GetScalingModeCount();

	// copyFrom < 0 表示新建空缩放配置
	void AddScalingMode(std::wstring_view name, int copyFrom);
	bool RenameScalingMode(uint32_t index, std::wstring_view name);
	bool CanUseName(std::wstring_view name, uint32_t exceptIndex) const noexcept;
	bool HasDuplicateNames() const noexcept;
	bool HasNameConflict(uint32_t index) const noexcept;

	void RemoveScalingMode(uint32_t index);

	bool MoveScalingMode(uint32_t fromIndex, uint32_t toIndex);

	void ResetScalingModes();

	// 不能使用 rapidjson::Writer 类型，因为 PrettyWriter 没有重写 Writer 中的方法
	// 不合理的 API 设计
	void Export(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer) const noexcept;

	static void Export(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer,
		const std::vector<ScalingMode>& modes);

	bool Import(const rapidjson::GenericObject<true, rapidjson::Value>& root, bool loadingSettings,
		uint32_t* renamedCount = nullptr) noexcept;

	Event<EffectAddedWay> ScalingModeAdded;
	// Prepare view-model indices without changing UI collections or invoking bindings.
	Event<uint32_t> ScalingModeRemoving;
	Event<uint32_t> ScalingModeRemoved;
	Event<> ScalingModeNamesChanged;
	Event<uint32_t, uint32_t> ScalingModeMoved;
	Event<> ScalingModesReset;
	Event<uint32_t, uint32_t> EffectParametersChanged;

private:
	ScalingModesService() = default;
};

}
