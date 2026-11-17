#pragma once

#include "MainWindow.g.h"
#include "EapiLibrary.h"
#include "EapiReader.h"

#include <memory>
#include <mutex>

namespace winrt::EapiGui::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();

        void LoadButton_Click(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void BrowseButton_Click(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void CopyButton_Click(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void AutoRefreshSwitch_Toggled(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void DllPathBox_KeyDown(winrt::Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const&);

    private:
        // Shared with the background reader thread; the mutex serializes all EAPI calls.
        struct EapiState
        {
            std::mutex mutex;
            eapi::Library library;
        };

        winrt::fire_and_forget RefreshAsync(bool reload);
        void Render(std::vector<eapi::Section> const& sections);
        void ShowStatus(Microsoft::UI::Xaml::Controls::InfoBarSeverity severity, winrt::hstring const& title, winrt::hstring const& message);
        Microsoft::UI::Xaml::Style FindStyle(wchar_t const* key);
        HWND WindowHandle();

        std::shared_ptr<EapiState> m_eapi = std::make_shared<EapiState>();
        std::vector<eapi::Section> m_sections;
        Microsoft::UI::Dispatching::DispatcherQueueTimer m_timer{ nullptr };
        bool m_busy{ false };
        bool m_loaded{ false };
    };
}

namespace winrt::EapiGui::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
