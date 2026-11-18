#pragma once

#include "MainWindow.g.h"
#include "EapiLibrary.h"
#include "EapiReader.h"

#include <functional>
#include <memory>
#include <mutex>

namespace winrt::EapiGui::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        using IInspectable = winrt::Windows::Foundation::IInspectable;
        using RoutedEventArgs = Microsoft::UI::Xaml::RoutedEventArgs;

        MainWindow();

        // Menu
        void SearchDll_Click(IInspectable const&, RoutedEventArgs const&);
        void LoadDefault_Click(IInspectable const&, RoutedEventArgs const&);
        void Copy_Click(IInspectable const&, RoutedEventArgs const&);
        void Exit_Click(IInspectable const&, RoutedEventArgs const&);
        void Refresh_Click(IInspectable const&, RoutedEventArgs const&);
        void AutoRefresh_Click(IInspectable const&, RoutedEventArgs const&);
        void NavView_SelectionChanged(Microsoft::UI::Xaml::Controls::NavigationView const&,
                                      Microsoft::UI::Xaml::Controls::NavigationViewSelectionChangedEventArgs const&);

        // Pages
        void BacklightIdBox_SelectionChanged(IInspectable const&, Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const&);
        void BacklightRead_Click(IInspectable const&, RoutedEventArgs const&);
        winrt::fire_and_forget BacklightApply_Click(IInspectable, RoutedEventArgs);
        void GpioRefresh_Click(IInspectable const&, RoutedEventArgs const&);
        winrt::fire_and_forget WatchdogStart_Click(IInspectable, RoutedEventArgs);
        winrt::fire_and_forget WatchdogTrigger_Click(IInspectable, RoutedEventArgs);
        winrt::fire_and_forget WatchdogStop_Click(IInspectable, RoutedEventArgs);
        winrt::fire_and_forget StorageRead_Click(IInspectable, RoutedEventArgs);
        winrt::fire_and_forget StorageWrite_Click(IInspectable, RoutedEventArgs);
        winrt::fire_and_forget I2CProbe_Click(IInspectable, RoutedEventArgs);
        winrt::fire_and_forget I2CRead_Click(IInspectable, RoutedEventArgs);
        winrt::fire_and_forget I2CWrite_Click(IInspectable, RoutedEventArgs);
        winrt::fire_and_forget I2CWriteRead_Click(IInspectable, RoutedEventArgs);

    private:
        // Shared with background threads; the mutex serializes all EAPI calls.
        struct EapiState
        {
            std::mutex mutex;
            eapi::Library library;
        };
        using EapiAction = std::function<eapi::Status(eapi::Library&)>;

        winrt::fire_and_forget RefreshAsync(bool reload);
        // Runs `action` on a background thread under the EAPI lock and reports its status in the InfoBar.
        winrt::Windows::Foundation::IAsyncOperation<eapi::Status> RunAsync(winrt::hstring title, EapiAction action, bool reportSuccess = true);
        winrt::Windows::Foundation::IAsyncOperation<bool> ConfirmAsync(winrt::hstring title, winrt::hstring text);

        void RefreshPage();
        winrt::fire_and_forget ReadBacklightAsync();
        winrt::fire_and_forget ReadGpioAsync();
        winrt::fire_and_forget SetGpioAsync(eapi::GpioPin pin, bool direction, bool value);
        void RenderGpio(std::vector<eapi::GpioPin> const& pins);

        // I2C form values; false (and an InfoBar error) when the address or command is invalid.
        bool ReadI2CForm(eapi::Id& bus, std::uint32_t& addr, std::uint32_t& cmd);

        void Render(std::vector<eapi::Section> const& sections);
        std::wstring SectionSummary(std::wstring_view title) const;
        void ShowStatus(Microsoft::UI::Xaml::Controls::InfoBarSeverity severity, winrt::hstring const& title, winrt::hstring const& message);
        Microsoft::UI::Xaml::Style FindStyle(wchar_t const* key);
        HWND WindowHandle();

        std::shared_ptr<EapiState> m_eapi = std::make_shared<EapiState>();
        std::wstring m_dllPath; // empty = try the default DLL names
        winrt::hstring m_page{ L"Overview" };
        std::vector<eapi::Section> m_sections;
        Microsoft::UI::Dispatching::DispatcherQueueTimer m_timer{ nullptr };
        bool m_busy{ false };
        bool m_reloadPending{ false };
        bool m_loaded{ false };
    };
}

namespace winrt::EapiGui::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
