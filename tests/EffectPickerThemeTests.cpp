#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Hosting.h>
#include <winrt/Windows.UI.Xaml.Markup.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.XamlTypeInfo.h>
#include <fstream>
#include <iostream>
#include <cassert>
#include <cmath>
#include <vector>

using namespace winrt;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;

struct ThemeTestApp : ApplicationT<ThemeTestApp, Markup::IXamlMetadataProvider> {
	Microsoft::UI::Xaml::XamlTypeInfo::XamlControlsXamlMetaDataProvider provider{nullptr};
	void InitializeResources() {
		provider = Microsoft::UI::Xaml::XamlTypeInfo::XamlControlsXamlMetaDataProvider();
		Microsoft::UI::Xaml::Controls::XamlControlsResources resources;
		resources.ControlsResourcesVersion(Microsoft::UI::Xaml::Controls::ControlsResourcesVersion::Version2);
		Resources().MergedDictionaries().Append(resources);
	}
	auto& Provider() {
		if (!provider) provider = Microsoft::UI::Xaml::XamlTypeInfo::XamlControlsXamlMetaDataProvider();
		return provider;
	}
	Markup::IXamlType GetXamlType(Interop::TypeName const& type) { return Provider().GetXamlType(type); }
	Markup::IXamlType GetXamlType(hstring const& name) { return Provider().GetXamlType(name); }
	com_array<Markup::XmlnsDefinition> GetXmlnsDefinitions() { return Provider().GetXmlnsDefinitions(); }
};

int main() {
	try {
		init_apartment(apartment_type::single_threaded);
		const auto app = make_self<ThemeTestApp>();
		std::cerr << "Initialize XAML\n";
		const auto manager = Hosting::WindowsXamlManager::InitializeForCurrentThread();
		std::cerr << "Initialize WinUI resources\n";
		app->InitializeResources();
		std::ifstream input("EffectPickerStyles.xaml", std::ios::binary);
		const std::string bytes{std::istreambuf_iterator<char>(input), {}};
		assert(!bytes.empty());
		std::cerr << "Load production styles\n";
		const auto styles = Markup::XamlReader::Load(to_hstring(bytes)).as<ResourceDictionary>();
		app->Resources().MergedDictionaries().Append(styles);
		Grid root;
		Button button;
		button.Style(styles.Lookup(box_value(L"EffectPickerButtonStyle")).as<Style>());
		button.Content(box_value(L"CuNNy"));
		root.Children().Append(button);
		Button letter;
		letter.Style(styles.Lookup(box_value(L"EffectPickerLetterButtonStyle")).as<Style>());
		letter.Content(box_value(L"A"));
		root.Children().Append(letter);
		Border pane, categoryPane, detailPane, selected;
		pane.Style(styles.Lookup(box_value(L"EffectPickerPaneStyle")).as<Style>());
		categoryPane.Style(styles.Lookup(box_value(L"EffectPickerCategoryPaneStyle")).as<Style>());
		detailPane.Style(styles.Lookup(box_value(L"EffectPickerDetailPaneStyle")).as<Style>());
		selected.Style(styles.Lookup(box_value(L"EffectPickerSelectedCategoryStyle")).as<Style>());
		root.Children().Append(pane); root.Children().Append(selected);
		root.Children().Append(categoryPane); root.Children().Append(detailPane);
		const auto mark = styles.Lookup(box_value(L"EffectPickerSelectionMark")).as<DataTemplate>().LoadContent().as<Border>();
		root.Children().Append(mark);
		FlyoutPresenter presenter;
		presenter.Style(styles.Lookup(box_value(L"EffectPickerPresenterStyle")).as<Style>());
		assert(presenter.Style().BasedOn());
		presenter.Content(root);
		std::vector<Windows::UI::Color> textColors;
		std::vector<Windows::UI::Color> panelColors;
		for (ElementTheme theme : {ElementTheme::Dark, ElementTheme::Light, ElementTheme::Dark}) {
			std::cerr << "Measure theme " << int(theme) << "\n";
			presenter.RequestedTheme(theme);
			presenter.Measure({828, 808}); presenter.Arrange({0, 0, 828, 808});
			assert(presenter.Background() && pane.Background() && selected.Background() && mark.Background());
			assert(presenter.BorderBrush() && button.Foreground());
			assert(presenter.CornerRadius().TopLeft > 0);
			assert(button.CornerRadius().TopLeft > 0);
			assert(letter.ActualWidth() == 18 && letter.ActualHeight() == 18);
			assert(letter.FontSize() == 10 && letter.UseSystemFocusVisuals());
			const auto letterRoot = Media::VisualTreeHelper::GetChild(letter, 0).as<Grid>();
			const auto letterContent = Media::VisualTreeHelper::GetChild(letterRoot, 0).as<ContentPresenter>();
			const auto foreground = letterContent.Foreground().as<Media::SolidColorBrush>().Color();
			assert(letterRoot.CornerRadius().TopLeft > 0);
			// Reproduce disabling a hovered/pressed item. Only the text fades;
			// the prior hover background must also return to transparent.
			for (const auto* prior : {L"PointerOver", L"Pressed"}) {
				assert(VisualStateManager::GoToState(letter, prior, false));
				letter.IsEnabled(false);
				assert(VisualStateManager::GoToState(letter, L"Disabled", false));
				assert(letterRoot.Background().as<Media::SolidColorBrush>().Color().A == 0);
				assert(letter.Opacity() == 1 && letterRoot.Opacity() == 1);
				assert(std::abs(letterContent.Opacity() - 0.3) < 0.000001);
				assert(letterContent.Foreground().as<Media::SolidColorBrush>().Color() == foreground);
				letter.IsEnabled(true);
				assert(VisualStateManager::GoToState(letter, L"Normal", false));
				assert(letterContent.Opacity() == 1);
			}
			const auto category = categoryPane.Background().as<Media::SolidColorBrush>();
			const auto list = pane.Background().as<Media::SolidColorBrush>();
			const auto details = detailPane.Background().as<Media::SolidColorBrush>();
			assert(category.Color() == list.Color() && list.Color() == details.Color());
			assert(category.Opacity() < list.Opacity() && list.Opacity() < details.Opacity());
			assert(category.Opacity() > 0 && details.Opacity() < 1);
			panelColors.push_back(list.Color());
			textColors.push_back(button.Foreground().as<Media::SolidColorBrush>().Color());
		}
		assert(textColors[0] != textColors[1] && textColors[0] == textColors[2]);
		assert(panelColors[0] != panelColors[1] && panelColors[0] == panelColors[2]);
		std::cout << "Flyout material: " << to_string(get_class_name(presenter.Background())) << "\n";
		manager.Close();
		std::cout << "Production picker resources load with WinUI 2 Version2: themed brushes, rounded controls, translucent panes, inherited flyout and 18-DIP alphabet buttons with text-only disabled opacity passed. No window shown.\n";
	} catch (hresult_error const& error) {
		std::cerr << "Theme resource error: " << to_string(error.message()) << " (" << std::hex << error.code().value << ")\n";
		return 1;
	}
}
