#pragma once
#include "EffectPickerModel.h"

namespace Magpie {

class EffectCatalog {
public:
	static const EffectCatalog& Get();
	const EffectPickerEntry* Find(std::wstring_view id) const;
	const std::vector<EffectPickerCategory>& Categories() const { return _categories; }
private:
	EffectCatalog();
	std::vector<EffectPickerEntry> _entries;
	std::vector<EffectPickerCategory> _categories;
};

}
