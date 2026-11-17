#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include <chrono>
#include <cstdio>

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;

namespace winrt::EapiGui::implementation
{
    MainWindow::MainWindow()
    {
        InitializeComponent();

        AppWindow().Resize({ 1000, 900 });

        m_timer = DispatcherQueue().CreateTimer();
        m_timer.Interval(std::chrono::seconds(2));
        m_timer.Tick([weak = get_weak()](auto&&, auto&&)
        {
            if (auto self = weak.get(); self && self->m_loaded)
                self->RefreshAsync(false);
        });

        Closed([this](auto&&, auto&&)
        {
            m_timer.Stop();
            // Unload on this thread once any running read has finished.
            std::scoped_lock lock(m_eapi->mutex);
            m_eapi->library.Unload();
        });

        // Try the default DLL names right away.
        RefreshAsync(true);
    }

    void MainWindow::LoadButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        RefreshAsync(true);
    }

    void MainWindow::DllPathBox_KeyDown(IInspectable const&, Input::KeyRoutedEventArgs const& e)
    {
        if (e.Key() == Windows::System::VirtualKey::Enter)
        {
            e.Handled(true);
            RefreshAsync(true);
        }
    }

    void MainWindow::BrowseButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        wchar_t file[MAX_PATH]{};
        OPENFILENAMEW ofn{};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = WindowHandle();
        ofn.lpstrFilter = L"EAPI library (*.dll)\0*.dll\0All files (*.*)\0*.*\0";
        ofn.lpstrFile = file;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrTitle = L"Select EAPI DLL";
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if (::GetOpenFileNameW(&ofn))
        {
            DllPathBox().Text(file);
            RefreshAsync(true);
        }
    }

    void MainWindow::CopyButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        Windows::ApplicationModel::DataTransfer::DataPackage package;
        package.SetText(eapi::ToText(m_sections));
        Windows::ApplicationModel::DataTransfer::Clipboard::SetContent(package);
        ShowStatus(InfoBarSeverity::Success, L"Copied", L"All values were copied to the clipboard.");
    }

    void MainWindow::AutoRefreshSwitch_Toggled(IInspectable const&, RoutedEventArgs const&)
    {
        if (AutoRefreshSwitch().IsOn())
            m_timer.Start();
        else
            m_timer.Stop();
    }

    winrt::fire_and_forget MainWindow::RefreshAsync(bool reload)
    {
        if (m_busy)
            co_return;
        m_busy = true;

        auto strongThis = get_strong();
        auto queue = DispatcherQueue();
        std::wstring const path = std::wstring(std::wstring_view(DllPathBox().Text()));
        auto state = m_eapi;

        BusyRing().IsActive(true);
        LoadButton().IsEnabled(false);

        bool loaded = false;
        std::wstring error;
        std::vector<eapi::Section> sections;

        co_await winrt::resume_background();
        {
            std::scoped_lock lock(state->mutex);
            loaded = state->library.IsLoaded();
            if (reload || !loaded)
                loaded = state->library.Load(path, error);
            if (loaded)
                sections = eapi::ReadAll(state->library);
        }
        co_await wil::resume_foreground(queue);

        m_busy = false;
        m_loaded = loaded;
        BusyRing().IsActive(false);
        LoadButton().IsEnabled(true);
        LoadButton().Content(box_value(loaded ? L"Reload" : L"Load"));
        CopyButton().IsEnabled(loaded);
        AutoRefreshSwitch().IsEnabled(loaded);
        if (!loaded)
        {
            AutoRefreshSwitch().IsOn(false);
            m_timer.Stop();
        }

        if (loaded)
        {
            if (reload)
            {
                StatusInfoBar().IsOpen(false);
                if (path.empty())
                    DllPathBox().Text(state->library.LoadedPath());
            }
            Render(sections);
            SYSTEMTIME now{};
            ::GetLocalTime(&now);
            wchar_t stamp[32]{};
            swprintf_s(stamp, L"Last update: %02u:%02u:%02u", now.wHour, now.wMinute, now.wSecond);
            LastUpdateText().Text(stamp);
        }
        else
        {
            Render({});
            LastUpdateText().Text(L"");
            ShowStatus(InfoBarSeverity::Error, L"EAPI library not loaded", hstring(error));
        }
    }

    void MainWindow::Render(std::vector<eapi::Section> const& sections)
    {
        m_sections = sections;
        auto panel = SectionsPanel();
        panel.Children().Clear();

        auto const cardStyle = FindStyle(L"CardStyle");
        auto const titleStyle = FindStyle(L"SectionTitleStyle");
        auto const nameStyle = FindStyle(L"NameTextStyle");
        auto const valueStyle = FindStyle(L"ValueTextStyle");
        auto const unsupportedStyle = FindStyle(L"NotSupportedTextStyle");
        auto const errorStyle = FindStyle(L"ErrorTextStyle");

        for (auto const& section : sections)
        {
            Border card;
            card.Style(cardStyle);

            StackPanel content;
            TextBlock title;
            title.Style(titleStyle);
            title.Text(section.title);
            content.Children().Append(title);

            Grid grid;
            ColumnDefinition nameColumn;
            nameColumn.Width(GridLengthHelper::FromPixels(280));
            ColumnDefinition valueColumn;
            valueColumn.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
            grid.ColumnDefinitions().Append(nameColumn);
            grid.ColumnDefinitions().Append(valueColumn);

            int row = 0;
            for (auto const& entry : section.entries)
            {
                RowDefinition rowDef;
                rowDef.Height(GridLengthHelper::Auto());
                grid.RowDefinitions().Append(rowDef);

                TextBlock name;
                name.Style(nameStyle);
                name.Text(entry.name);
                Grid::SetRow(name, row);
                Grid::SetColumn(name, 0);

                TextBlock value;
                switch (entry.state)
                {
                case eapi::EntryState::Ok:           value.Style(valueStyle); break;
                case eapi::EntryState::NotSupported: value.Style(unsupportedStyle); break;
                case eapi::EntryState::Error:        value.Style(errorStyle); break;
                }
                value.Text(entry.value.empty() ? L"(empty)" : entry.value);
                if (!entry.detail.empty())
                    ToolTipService::SetToolTip(value, box_value(entry.detail));
                Grid::SetRow(value, row);
                Grid::SetColumn(value, 1);

                grid.Children().Append(name);
                grid.Children().Append(value);
                ++row;
            }

            content.Children().Append(grid);
            card.Child(content);
            panel.Children().Append(card);
        }
    }

    void MainWindow::ShowStatus(InfoBarSeverity severity, hstring const& title, hstring const& message)
    {
        auto bar = StatusInfoBar();
        bar.Severity(severity);
        bar.Title(title);
        bar.Message(message);
        bar.IsOpen(true);
    }

    Style MainWindow::FindStyle(wchar_t const* key)
    {
        return RootGrid().Resources().Lookup(box_value(key)).as<Style>();
    }

    HWND MainWindow::WindowHandle()
    {
        HWND hwnd{};
        auto native = this->m_inner.as<::IWindowNative>();
        native->get_WindowHandle(&hwnd);
        return hwnd;
    }
}
