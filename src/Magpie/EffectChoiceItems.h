#pragma once
#include <winrt/Windows.Foundation.Collections.h>

namespace Magpie {

inline winrt::Windows::Foundation::Collections::IObservableVector<winrt::hstring>
MakeEffectChoiceItems(std::vector<winrt::hstring> labels) {
	// Unlike single_threaded_vector<hstring>, this collection also exposes the
	// IInspectable collection interfaces required by XAML ItemsSource.
	return winrt::single_threaded_observable_vector(std::move(labels));
}

}
