#pragma once
#include "ScalingModeItem.g.h"
#include "Event.h"
#include "ScalingModesService.h"

namespace Magpie {
struct ScalingMode;
}

namespace winrt::Magpie::implementation {

struct ScalingModeEffectItem;

struct ScalingModeItem : ScalingModeItemT<ScalingModeItem>,
                         wil::notify_property_changed_base<ScalingModeItem> {
	ScalingModeItem(uint32_t index, bool isInitialExpanded, bool shouldAutoRename);

	void AddEffect(const hstring& fullName);
	bool CanAddEffect(const hstring& fullName) const noexcept;
	hstring EffectAddProblem(const hstring& fullName) const;

	bool IsInitialExpanded() const noexcept {
		return _isInitialExpanded;
	}

	hstring Name() const noexcept;

	void Name(const hstring& value) noexcept;

	hstring Description() const noexcept;

	bool HasUnkownEffects() const noexcept;
	bool HasNameConflict() const noexcept;
	hstring RenameProblem() const noexcept { return _renameProblem; }
	bool HasRenameProblem() const noexcept { return !_renameProblem.empty(); }

	IObservableVector<IInspectable> Effects() const noexcept {
		return _effects;
	}

	hstring RenameText() const noexcept {
		return _renameText;
	}

	void RenameText(const hstring& value) noexcept;

	bool IsRenameButtonEnabled() const noexcept {
		return _isRenameButtonEnabled;
	}

	void RenameFlyout_Opening();

	void RenameTextBox_KeyDown(IInspectable const&, Input::KeyRoutedEventArgs const& args);

	int RenameTextBoxSelectionStart() {
		return _renameText.size();
	}

	void RenameButton_Click();

	bool TakeAutoRenameRequest() noexcept;

	bool CanDrag() const noexcept;

	void Duplicate();

	bool CanReorderEffects() const noexcept;

	void Remove();
	void Detach() noexcept;
	void PrepareForRemoval(uint32_t index) noexcept;
	void RefreshAfterRemoval();

	IVector<IInspectable> LinkedProfiles() const noexcept {
		return _linkedProfiles;
	}

	bool IsInUse() const noexcept {
		return _linkedProfiles.Size() > 0;
	}

private:
	void _Index(uint32_t value) noexcept;

	bool _IsRemoved() const noexcept;

	void _ScalingModesService_Added(::Magpie::EffectAddedWay);

	void _ScalingModesService_Moved(uint32_t fromIndex, uint32_t toIndex);

	void _ScalingModesService_NamesChanged();

	void _Effects_VectorChanged(IObservableVector<IInspectable> const&, IVectorChangedEventArgs const& args);

	void _ScalingModeEffectItem_Removed(uint32_t index);

	com_ptr<ScalingModeEffectItem> _CreateScalingModeEffectItem(uint32_t scalingModeIdx, uint32_t effectIdx);

	void _RefreshEffectDragState();

	::Magpie::ScalingMode& _Data() noexcept;
	const ::Magpie::ScalingMode& _Data() const noexcept;

	uint32_t _index = 0;
	IObservableVector<IInspectable> _effects{ nullptr };
	
	uint32_t _movingFromIdx = std::numeric_limits<uint32_t>::max();

	::Magpie::Event<::Magpie::EffectAddedWay>::EventRevoker _scalingModeAddedRevoker;
	::Magpie::Event<uint32_t, uint32_t>::EventRevoker _scalingModeMovedRevoker;
	::Magpie::Event<>::EventRevoker _scalingModeNamesChangedRevoker;
	IObservableVector<IInspectable>::VectorChanged_revoker _effectsChangedRevoker;

	hstring _renameText;
	hstring _renameProblem;
	std::wstring_view _trimedRenameText;
	
	IVector<IInspectable> _linkedProfiles{ nullptr };

	bool _isMovingEffects = true;
	bool _isRenameButtonEnabled = false;
	bool _isInitialExpanded = false;
	bool _shouldAutoRename = false;
};

}
