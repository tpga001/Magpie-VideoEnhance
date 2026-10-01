#include "pch.h"
#include "EffectCatalog.h"
#include "LocalizationService.h"
#include "Logger.h"
#include "StrHelper.h"
#include <rapidjson/document.h>

namespace Magpie {

const EffectCatalog& EffectCatalog::Get() {
	static const EffectCatalog catalog;
	return catalog;
}

const EffectPickerEntry* EffectCatalog::Find(std::wstring_view id) const {
	const auto it = std::ranges::find(_entries, id, &EffectPickerEntry::id);
	return it == _entries.end() ? nullptr : &*it;
}

EffectCatalog::EffectCatalog() {
	// Descriptions are embedded with the executable, never read from user settings.
	const HMODULE module = GetModuleHandle(nullptr);
	const bool chinese = std::wstring_view(LocalizationService::Get().Language()).starts_with(L"zh");
	HRSRC resource = FindResource(module,
		chinese ? L"EFFECT_CATALOG_ZH_HANS" : L"EFFECT_CATALOG_EN_US", RT_RCDATA);
	// Keep the picker usable if a localized resource is accidentally omitted.
	if (!resource && !chinese) {
		resource = FindResource(module, L"EFFECT_CATALOG_ZH_HANS", RT_RCDATA);
	}
	const HGLOBAL loaded = resource ? LoadResource(module, resource) : nullptr;
	const auto bytes = loaded ? static_cast<const char*>(LockResource(loaded)) : nullptr;
	if (!bytes) {
		Logger::Get().Warn("Effect catalog unavailable; using effect names and custom category");
		return;
	}
	rapidjson::Document document;
	document.Parse(bytes, SizeofResource(module, resource));
	if (document.HasParseError() || !document.IsObject()) return;
	auto text = [](const rapidjson::Value& value, const char* key) -> std::wstring {
		if (!value.IsObject()) return {};
		const auto it = value.FindMember(key);
		return it != value.MemberEnd() && it->value.IsString()
			? StrHelper::UTF8ToUTF16({ it->value.GetString(), it->value.GetStringLength() }) : std::wstring{};
	};
	const auto categories = document.FindMember("categories");
	if (categories != document.MemberEnd() && categories->value.IsArray()) {
		for (const auto& value : categories->value.GetArray()) {
			EffectPickerCategory category{ text(value, "id"), text(value, "name"), text(value, "description") };
			if (category.id.empty() || category.name.empty()) continue;
			const auto subs = value.FindMember("subcategories");
			if (subs != value.MemberEnd() && subs->value.IsArray()) {
				for (const auto& sub : subs->value.GetArray()) {
					category.subcategories.emplace_back(text(sub, "name"), text(sub, "description"));
				}
			}
			_categories.push_back(std::move(category));
		}
	}
	const auto effects = document.FindMember("effects");
	if (effects == document.MemberEnd() || !effects->value.IsArray()) return;
	for (const auto& value : effects->value.GetArray()) {
		EffectPickerEntry entry;
		entry.id = text(value, "id");
		entry.name = text(value, "name");
		if (entry.id.empty() || entry.name.empty()) continue;
		entry.category = text(value, "category");
		entry.subcategory = text(value, "subcategory");
		entry.summary = text(value, "summary");
		entry.details = text(value, "details");
		entry.recommendation = text(value, "recommendation");
		entry.firstTry = text(value, "level") == L"first_try";
		entry.searchText = NormalizeEffectSearch(text(value, "search"));
		auto readFamily = [&](const char* key) {
			const auto family = value.FindMember(key);
			if (family == value.MemberEnd() || !family->value.IsObject()) return EffectPickerFamily{};
			return EffectPickerFamily{text(family->value, "id"), text(family->value, "name"), text(family->value, "summary")};
		};
		entry.family = readFamily("family");
		entry.subfamily = readFamily("subfamily");
		const auto purposes = value.FindMember("purposes");
		if (purposes != value.MemberEnd() && purposes->value.IsArray()) {
			for (const auto& purpose : purposes->value.GetArray()) {
				if (purpose.IsString()) entry.purposes.push_back(StrHelper::UTF8ToUTF16(purpose.GetString()));
			}
		}
		_entries.push_back(std::move(entry));
	}
}

}
