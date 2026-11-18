#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using winrt::Windows::Foundation::IAsyncOperation;
using eapi::EAPI_STATUS_SUCCESS;

namespace
{
    // NumberBox value clamped to [lo, hi]; an empty box (NaN) counts as lo.
    std::uint32_t ToU32(double v, std::uint32_t lo = 0, std::uint32_t hi = 0xFFFFFFFF)
    {
        return std::isnan(v) ? lo : static_cast<std::uint32_t>(std::clamp(v, double(lo), double(hi)));
    }

    constexpr std::uint32_t MaxTransfer = 65536; // bytes per storage / I2C read in the GUI

    constexpr wchar_t AppVersion[] = L"1.0.0"; // keep in sync with app.manifest
    constexpr wchar_t EapiSpecVersion[] = L"PICMG EAPI R1.0";
    constexpr wchar_t EapiSpecUrl[] = L"https://www.picmg.org/wp-content/uploads/COM_EAPI_R1_0.pdf";
    // Same text as LICENSE in the repository root.
    constexpr wchar_t LicenseText[] =
        L"MIT License\n\n"
        L"Copyright (c) 2026 Johannes Feulner\n\n"
        L"Permission is hereby granted, free of charge, to any person obtaining a copy of this software and "
        L"associated documentation files (the \"Software\"), to deal in the Software without restriction, including "
        L"without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell "
        L"copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the "
        L"following conditions:\n\n"
        L"The above copyright notice and this permission notice shall be included in all copies or substantial "
        L"portions of the Software.\n\n"
        L"THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT "
        L"LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO "
        L"EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER "
        L"IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE "
        L"USE OR OTHER DEALINGS IN THE SOFTWARE.";
}

namespace winrt::EapiGui::implementation
{
    MainWindow::MainWindow()
    {
        InitializeComponent();
        ExtendsContentIntoTitleBar(true);
        SetTitleBar(AppTitleBar());
        AppWindow().Resize({ 1100, 900 });

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
            // Unload on this thread once any running EAPI call has finished.
            std::scoped_lock lock(m_eapi->mutex);
            m_eapi->library.Unload();
        });

        NavView().SelectedItem(NavView().MenuItems().GetAt(0));

        // Try the default DLL names right away.
        RefreshAsync(true);
    }

    // ---- Menu -------------------------------------------------------------------

    void MainWindow::SearchDll_Click(IInspectable const&, RoutedEventArgs const&)
    {
        wchar_t file[MAX_PATH]{};
        OPENFILENAMEW ofn{};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = WindowHandle();
        ofn.lpstrFilter = L"EAPI library (*.dll)\0*.dll\0All files (*.*)\0*.*\0";
        ofn.lpstrFile = file;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrTitle = L"Search for EAPI.dll";
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if (::GetOpenFileNameW(&ofn))
        {
            m_dllPath = file;
            RefreshAsync(true);
        }
    }

    void MainWindow::LoadDefault_Click(IInspectable const&, RoutedEventArgs const&)
    {
        m_dllPath.clear();
        RefreshAsync(true);
    }

    void MainWindow::Copy_Click(IInspectable const&, RoutedEventArgs const&)
    {
        Windows::ApplicationModel::DataTransfer::DataPackage package;
        package.SetText(eapi::ToText(m_sections));
        Windows::ApplicationModel::DataTransfer::Clipboard::SetContent(package);
        ShowStatus(InfoBarSeverity::Success, L"Copied", L"All overview values were copied to the clipboard.");
    }

    void MainWindow::Exit_Click(IInspectable const&, RoutedEventArgs const&)
    {
        Close();
    }

    void MainWindow::Refresh_Click(IInspectable const&, RoutedEventArgs const&)
    {
        RefreshAsync(false);
        RefreshPage();
    }

    void MainWindow::AutoRefresh_Click(IInspectable const&, RoutedEventArgs const&)
    {
        if (AutoRefreshMenuItem().IsChecked())
            m_timer.Start();
        else
            m_timer.Stop();
    }

    winrt::fire_and_forget MainWindow::About_Click(IInspectable, RoutedEventArgs)
    {
        auto strongThis = get_strong();
        auto line = [](hstring const& label, hstring const& value)
        {
            TextBlock text;
            text.Text(label + L": " + value);
            text.IsTextSelectionEnabled(true);
            return text;
        };

        // The loaded DLL reports its own spec version; the overview has already read it.
        std::wstring libSpec;
        for (auto const& s : m_sections)
            for (auto const& e : s.entries)
                if (e.name == L"EAPI specification version")
                    libSpec = e.value;

        StackPanel content;
        content.Spacing(6);
        content.Children().Append(line(L"EapiGui version", AppVersion));
        content.Children().Append(line(L"Supported EAPI specification", EapiSpecVersion));
        HyperlinkButton specLink;
        specLink.Content(box_value(L"Open the EAPI specification (PDF, picmg.org)"));
        specLink.Padding({ 0, 0, 0, 0 });
        specLink.Click({ this, &MainWindow::Spec_Click });
        content.Children().Append(specLink);
        content.Children().Append(line(L"Loaded EAPI library reports",
                                       libSpec.empty() ? hstring(L"no library loaded") : hstring(L"EAPI " + libSpec)));

        TextBlock license;
        license.Text(LicenseText);
        license.TextWrapping(TextWrapping::Wrap);
        license.IsTextSelectionEnabled(true);
        license.Style(FindStyle(L"HintTextStyle"));
        ScrollViewer licenseScroll;
        licenseScroll.Content(license);
        licenseScroll.MaxHeight(220);
        licenseScroll.Margin({ 0, 12, 0, 0 });
        content.Children().Append(licenseScroll);

        ContentDialog dialog;
        dialog.XamlRoot(Content().XamlRoot());
        dialog.Style(Application::Current().Resources().Lookup(box_value(L"DefaultContentDialogStyle")).as<Microsoft::UI::Xaml::Style>());
        dialog.Title(box_value(L"About EapiGui"));
        dialog.Content(content);
        dialog.CloseButtonText(L"Close");
        dialog.DefaultButton(ContentDialogButton::Close);
        co_await dialog.ShowAsync();
    }

    void MainWindow::Spec_Click(IInspectable const&, RoutedEventArgs const&)
    {
        // ShellExecute instead of Launcher: works in unpackaged apps without package identity.
        auto const result = reinterpret_cast<INT_PTR>(::ShellExecuteW(WindowHandle(), L"open", EapiSpecUrl, nullptr, nullptr, SW_SHOWNORMAL));
        if (result <= 32)
            ShowStatus(InfoBarSeverity::Error, L"Cannot open the EAPI specification", hstring(EapiSpecUrl));
    }

    void MainWindow::NavView_SelectionChanged(NavigationView const&, NavigationViewSelectionChangedEventArgs const& e)
    {
        if (!e.SelectedItemContainer())
            return;
        m_page = unbox_value<hstring>(e.SelectedItemContainer().Tag());
        StatusInfoBar().IsOpen(false); // its message belongs to the previous page
        for (auto const& child : PagesHost().Children())
        {
            auto page = child.as<FrameworkElement>();
            page.Visibility(unbox_value<hstring>(page.Tag()) == m_page ? Visibility::Visible : Visibility::Collapsed);
        }
        RefreshPage();
    }

    void MainWindow::RefreshPage()
    {
        if (!m_loaded)
            return;
        if (m_page == L"Backlight")
            ReadBacklightAsync();
        else if (m_page == L"Gpio")
            ReadGpioAsync();
    }

    // ---- Loading and overview ---------------------------------------------------

    winrt::fire_and_forget MainWindow::RefreshAsync(bool reload)
    {
        if (m_busy)
        {
            // Do not drop a load request because an auto refresh is still running.
            m_reloadPending |= reload;
            co_return;
        }
        m_busy = true;

        auto strongThis = get_strong();
        auto queue = DispatcherQueue();
        std::wstring const path = m_dllPath;
        auto state = m_eapi;
        BusyRing().IsActive(true);

        bool loaded = false;
        std::wstring error, loadedPath;
        std::vector<eapi::Section> sections;

        co_await winrt::resume_background();
        {
            std::scoped_lock lock(state->mutex);
            loaded = state->library.IsLoaded();
            if (reload || !loaded)
                loaded = state->library.Load(path, error);
            if (loaded)
            {
                sections = eapi::ReadAll(state->library);
                loadedPath = state->library.LoadedPath();
            }
        }
        co_await wil::resume_foreground(queue);

        m_busy = false;
        m_loaded = loaded;
        BusyRing().IsActive(false);
        CopyMenuItem().IsEnabled(loaded);
        RefreshMenuItem().IsEnabled(loaded);
        AutoRefreshMenuItem().IsEnabled(loaded);
        if (!loaded)
        {
            AutoRefreshMenuItem().IsChecked(false);
            m_timer.Stop();
        }

        Render(sections);
        if (loaded)
        {
            AppTitleBar().Subtitle(std::filesystem::path(loadedPath).filename().wstring());
            ToolTipService::SetToolTip(AppTitleBar(), box_value(loadedPath));
            SYSTEMTIME now{};
            ::GetLocalTime(&now);
            wchar_t stamp[32]{};
            swprintf_s(stamp, L"Updated %02u:%02u:%02u", now.wHour, now.wMinute, now.wSecond);
            LastUpdateText().Text(stamp);
            if (reload)
            {
                StatusInfoBar().IsOpen(false);
                RefreshPage();
            }
        }
        else
        {
            AppTitleBar().Subtitle(L"No library loaded");
            LastUpdateText().Text(L"");
            ShowStatus(InfoBarSeverity::Error, L"EAPI library not loaded", hstring(error));
        }

        if (std::exchange(m_reloadPending, false))
            RefreshAsync(true);
    }

    void MainWindow::Render(std::vector<eapi::Section> const& sections)
    {
        m_sections = sections;
        EmptyState().Visibility(sections.empty() ? Visibility::Visible : Visibility::Collapsed);
        WatchdogCapsText().Text(SectionSummary(L"Watchdog"));
        StorageCapsText().Text(SectionSummary(L"User Storage (EEPROM)"));
        I2CCapsText().Text(SectionSummary(L"I2C Buses"));

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

    std::wstring MainWindow::SectionSummary(std::wstring_view title) const
    {
        std::wstring text;
        for (auto const& s : m_sections)
            if (s.title == title)
                for (auto const& e : s.entries)
                    text += (text.empty() ? L"" : L"   ·   ") + e.name + L": " + e.value;
        return text;
    }

    // ---- EAPI actions -----------------------------------------------------------

    IAsyncOperation<eapi::Status> MainWindow::RunAsync(hstring title, EapiAction action, bool reportSuccess)
    {
        auto strongThis = get_strong();
        auto queue = DispatcherQueue();
        auto state = m_eapi;

        co_await winrt::resume_background();
        eapi::Status status = eapi::EAPI_STATUS_NOT_INITIALIZED;
        {
            std::scoped_lock lock(state->mutex);
            if (state->library.IsLoaded())
                status = action(state->library);
        }
        co_await wil::resume_foreground(queue);

        if (status == eapi::EAPI_STATUS_UNSUPPORTED)
            ShowStatus(InfoBarSeverity::Warning, title, L"Not supported by this EAPI library.");
        else if (status != EAPI_STATUS_SUCCESS)
            ShowStatus(InfoBarSeverity::Error, title, hstring(L"Failed: " + eapi::StatusToString(status)));
        else if (reportSuccess)
            ShowStatus(InfoBarSeverity::Success, title, L"Done.");
        co_return status;
    }

    IAsyncOperation<bool> MainWindow::ConfirmAsync(hstring title, hstring text)
    {
        auto strongThis = get_strong();
        ContentDialog dialog;
        dialog.XamlRoot(Content().XamlRoot());
        dialog.Style(Application::Current().Resources().Lookup(box_value(L"DefaultContentDialogStyle")).as<Microsoft::UI::Xaml::Style>());
        dialog.Title(box_value(title));
        dialog.Content(box_value(text));
        dialog.PrimaryButtonText(L"Continue");
        dialog.CloseButtonText(L"Cancel");
        dialog.DefaultButton(ContentDialogButton::Close);
        co_return co_await dialog.ShowAsync() == ContentDialogResult::Primary;
    }

    // ---- Backlight ----------------------------------------------------------------

    void MainWindow::BacklightIdBox_SelectionChanged(IInspectable const&, SelectionChangedEventArgs const&)
    {
        if (m_loaded) // also fires during InitializeComponent
            ReadBacklightAsync();
    }

    void MainWindow::BacklightRead_Click(IInspectable const&, RoutedEventArgs const&)
    {
        ReadBacklightAsync();
    }

    winrt::fire_and_forget MainWindow::ReadBacklightAsync()
    {
        auto strongThis = get_strong();
        auto const id = static_cast<eapi::Id>(std::max(0, BacklightIdBox().SelectedIndex()));
        std::uint32_t enable = 0, bright = 0;
        eapi::Status enableSt = 0, brightSt = 0;
        co_await RunAsync(L"Read backlight", [&](eapi::Library& lib)
        {
            enableSt = eapi::Call(lib.VgaGetBacklightEnable, id, &enable);
            brightSt = eapi::Call(lib.VgaGetBacklightBrightness, id, &bright);
            return enableSt != EAPI_STATUS_SUCCESS ? enableSt : brightSt;
        }, false);
        if (enableSt == EAPI_STATUS_SUCCESS)
            BacklightEnableSwitch().IsOn(enable == eapi::BACKLIGHT_SET_ON);
        if (brightSt == EAPI_STATUS_SUCCESS)
            BacklightSlider().Value(bright);
    }

    winrt::fire_and_forget MainWindow::BacklightApply_Click(IInspectable, RoutedEventArgs)
    {
        auto strongThis = get_strong();
        auto const id = static_cast<eapi::Id>(std::max(0, BacklightIdBox().SelectedIndex()));
        std::uint32_t const enable = BacklightEnableSwitch().IsOn() ? eapi::BACKLIGHT_SET_ON : eapi::BACKLIGHT_SET_OFF;
        std::uint32_t const bright = ToU32(BacklightSlider().Value(), 0, eapi::BACKLIGHT_BRIGHTEST);
        co_await RunAsync(L"Set backlight", [=](eapi::Library& lib)
        {
            eapi::Status const brightSt = eapi::Call(lib.VgaSetBacklightBrightness, id, bright);
            eapi::Status const enableSt = eapi::Call(lib.VgaSetBacklightEnable, id, enable);
            return brightSt != EAPI_STATUS_SUCCESS ? brightSt : enableSt;
        });
    }

    // ---- GPIO -------------------------------------------------------------------

    void MainWindow::GpioRefresh_Click(IInspectable const&, RoutedEventArgs const&)
    {
        ReadGpioAsync();
    }

    winrt::fire_and_forget MainWindow::ReadGpioAsync()
    {
        auto strongThis = get_strong();
        std::vector<eapi::GpioPin> pins;
        co_await RunAsync(L"Read GPIO", [&](eapi::Library& lib) { return eapi::ReadGpio(lib, pins); }, false);
        RenderGpio(pins);
    }

    winrt::fire_and_forget MainWindow::SetGpioAsync(eapi::GpioPin pin, bool direction, bool value)
    {
        auto strongThis = get_strong();
        std::uint32_t const bits = value ? pin.bit : 0; // direction: 1 = input; level: 1 = high
        co_await RunAsync(hstring(L"GPIO " + std::to_wstring(pin.number)), [=](eapi::Library& lib)
        {
            return direction ? eapi::Call(lib.GPIOSetDirection, pin.id, pin.bit, bits)
                             : eapi::Call(lib.GPIOSetLevel, pin.id, pin.bit, bits);
        });
        ReadGpioAsync(); // show what the hardware actually took
    }

    void MainWindow::RenderGpio(std::vector<eapi::GpioPin> const& pins)
    {
        auto grid = GpioGrid();
        grid.Children().Clear();
        grid.RowDefinitions().Clear();
        auto const nameStyle = FindStyle(L"NameTextStyle");
        auto const hintStyle = FindStyle(L"HintTextStyle");
        auto add = [&](FrameworkElement const& element, int row, int column)
        {
            Grid::SetRow(element, row);
            Grid::SetColumn(element, column);
            grid.Children().Append(element);
        };

        if (pins.empty())
        {
            TextBlock text;
            text.Style(hintStyle);
            text.Text(L"No GPIO pins available.");
            Grid::SetColumnSpan(text, 4);
            add(text, 0, 0);
            return;
        }

        int row = 0;
        for (auto const& pin : pins)
        {
            RowDefinition rowDef;
            rowDef.Height(GridLengthHelper::Auto());
            grid.RowDefinitions().Append(rowDef);
            bool const dirOk = pin.directionStatus == EAPI_STATUS_SUCCESS;
            bool const levelOk = pin.levelStatus == EAPI_STATUS_SUCCESS;

            TextBlock name;
            name.Style(nameStyle);
            name.Text(L"GPIO " + std::to_wstring(pin.number));

            // Values are set before the handlers are attached, so building the list writes nothing.
            ComboBox direction;
            direction.HorizontalAlignment(HorizontalAlignment::Stretch);
            direction.VerticalAlignment(VerticalAlignment::Center);
            direction.Items().Append(box_value(L"Input"));
            direction.Items().Append(box_value(L"Output"));
            if (dirOk)
                direction.SelectedIndex(pin.isInput ? 0 : 1);
            direction.IsEnabled(dirOk && pin.canInput && pin.canOutput);
            direction.SelectionChanged([weak = get_weak(), pin](IInspectable const& sender, auto&&)
            {
                if (auto self = weak.get())
                    self->SetGpioAsync(pin, true, sender.as<ComboBox>().SelectedIndex() == 0);
            });

            ToggleSwitch level;
            level.OnContent(box_value(L"High"));
            level.OffContent(box_value(L"Low"));
            level.IsOn(levelOk && pin.isHigh);
            level.IsEnabled(levelOk && dirOk && !pin.isInput);
            level.Toggled([weak = get_weak(), pin](IInspectable const& sender, auto&&)
            {
                if (auto self = weak.get())
                    self->SetGpioAsync(pin, false, sender.as<ToggleSwitch>().IsOn());
            });

            TextBlock caps;
            caps.Style(hintStyle);
            caps.VerticalAlignment(VerticalAlignment::Center);
            caps.Text(!dirOk && !levelOk ? hstring(L"Error: " + eapi::StatusToString(pin.directionStatus))
                      : pin.canInput && pin.canOutput ? L"Input / output"
                      : pin.canInput ? L"Input only" : L"Output only");

            add(name, row, 0);
            add(direction, row, 1);
            add(level, row, 2);
            add(caps, row, 3);
            ++row;
        }
    }

    // ---- Watchdog -----------------------------------------------------------------

    winrt::fire_and_forget MainWindow::WatchdogStart_Click(IInspectable, RoutedEventArgs)
    {
        auto strongThis = get_strong();
        std::uint32_t const delay = ToU32(WatchdogDelayBox().Value());
        std::uint32_t const eventTimeout = ToU32(WatchdogEventBox().Value());
        std::uint32_t const resetTimeout = ToU32(WatchdogResetBox().Value());
        if (!co_await ConfirmAsync(L"Start watchdog?",
                L"The system is reset unless the watchdog is triggered or stopped within the timeouts."))
            co_return;
        co_await RunAsync(L"Start watchdog", [=](eapi::Library& lib)
        {
            return eapi::Call(lib.WDogStart, delay, eventTimeout, resetTimeout);
        });
    }

    winrt::fire_and_forget MainWindow::WatchdogTrigger_Click(IInspectable, RoutedEventArgs)
    {
        auto strongThis = get_strong();
        co_await RunAsync(L"Trigger watchdog", [](eapi::Library& lib) { return eapi::Call(lib.WDogTrigger); });
    }

    winrt::fire_and_forget MainWindow::WatchdogStop_Click(IInspectable, RoutedEventArgs)
    {
        auto strongThis = get_strong();
        co_await RunAsync(L"Stop watchdog", [](eapi::Library& lib) { return eapi::Call(lib.WDogStop); });
    }

    // ---- Storage ------------------------------------------------------------------

    winrt::fire_and_forget MainWindow::StorageRead_Click(IInspectable, RoutedEventArgs)
    {
        auto strongThis = get_strong();
        std::uint32_t const offset = ToU32(StorageOffsetBox().Value());
        std::uint32_t const length = ToU32(StorageLengthBox().Value(), 1, MaxTransfer);
        std::vector<std::uint8_t> data(length);
        if (co_await RunAsync(L"Read storage", [&](eapi::Library& lib)
            {
                return eapi::Call(lib.StorageAreaRead, eapi::ID_STORAGE_STD, offset, data.data(), length, length);
            }) == EAPI_STATUS_SUCCESS)
            StorageOutputBox().Text(eapi::HexDump(data, offset));
    }

    winrt::fire_and_forget MainWindow::StorageWrite_Click(IInspectable, RoutedEventArgs)
    {
        auto strongThis = get_strong();
        auto data = eapi::ParseHexBytes(StorageWriteBox().Text());
        if (!data || data->empty())
        {
            ShowStatus(InfoBarSeverity::Error, L"Write storage", L"Enter the data as hex bytes, e.g. 01 02 FF.");
            co_return;
        }
        std::uint32_t const offset = ToU32(StorageOffsetBox().Value());
        auto const size = static_cast<std::uint32_t>(data->size());
        if (!co_await ConfirmAsync(L"Write user storage?",
                hstring(std::to_wstring(size) + L" bytes are written at offset " + std::to_wstring(offset) +
                        L". Existing data there is overwritten.")))
            co_return;
        co_await RunAsync(L"Write storage", [&](eapi::Library& lib)
        {
            return eapi::Call(lib.StorageAreaWrite, eapi::ID_STORAGE_STD, offset, data->data(), size);
        });
    }

    // ---- I2C ----------------------------------------------------------------------

    bool MainWindow::ReadI2CForm(eapi::Id& bus, std::uint32_t& addr, std::uint32_t& cmd)
    {
        bus = static_cast<eapi::Id>(std::max(0, I2CBusBox().SelectedIndex()));

        auto const checked = I2CTenBitCheck().IsChecked();
        bool const tenBit = checked && checked.Value();
        auto const a = eapi::ParseHex32(I2CAddressBox().Text());
        if (!a || *a > (tenBit ? 0x3FFu : 0x7Fu))
        {
            ShowStatus(InfoBarSeverity::Error, L"I2C", tenBit ? L"The 10-bit address must be 0 - 3FF (hex)."
                                                              : L"The 7-bit address must be 0 - 7F (hex).");
            return false;
        }
        addr = tenBit ? eapi::I2CEncode10BitAddr(*a) : eapi::I2CEncode7BitAddr(*a);

        int const type = I2CCmdTypeBox().SelectedIndex();
        if (type <= 0)
        {
            cmd = eapi::I2C_NO_CMD;
            return true;
        }
        auto const c = eapi::ParseHex32(I2CCmdBox().Text());
        if (!c || *c > (type == 1 ? 0xFFu : 0xFFFFu))
        {
            ShowStatus(InfoBarSeverity::Error, L"I2C", L"The command must be 0 - FF (standard) or 0 - FFFF (extended), hex.");
            return false;
        }
        cmd = type == 1 ? eapi::I2CEncodeStdCmd(*c) : eapi::I2CEncodeExtCmd(*c);
        return true;
    }

    winrt::fire_and_forget MainWindow::I2CProbe_Click(IInspectable, RoutedEventArgs)
    {
        auto strongThis = get_strong();
        eapi::Id bus{};
        std::uint32_t addr{}, cmd{};
        if (!ReadI2CForm(bus, addr, cmd))
            co_return;
        if (co_await RunAsync(L"Probe I2C device", [=](eapi::Library& lib) { return eapi::Call(lib.I2CProbeDevice, bus, addr); }, false)
            == EAPI_STATUS_SUCCESS)
            ShowStatus(InfoBarSeverity::Success, L"Probe I2C device", L"Device responded.");
    }

    winrt::fire_and_forget MainWindow::I2CRead_Click(IInspectable, RoutedEventArgs)
    {
        auto strongThis = get_strong();
        eapi::Id bus{};
        std::uint32_t addr{}, cmd{};
        if (!ReadI2CForm(bus, addr, cmd))
            co_return;
        std::uint32_t const length = ToU32(I2CReadLengthBox().Value(), 1, MaxTransfer);
        std::vector<std::uint8_t> data(length);
        if (co_await RunAsync(L"I2C read", [&](eapi::Library& lib)
            {
                return eapi::Call(lib.I2CReadTransfer, bus, addr, cmd, data.data(), length, length);
            }) == EAPI_STATUS_SUCCESS)
            I2COutputBox().Text(eapi::HexDump(data, 0));
    }

    winrt::fire_and_forget MainWindow::I2CWrite_Click(IInspectable, RoutedEventArgs)
    {
        auto strongThis = get_strong();
        eapi::Id bus{};
        std::uint32_t addr{}, cmd{};
        if (!ReadI2CForm(bus, addr, cmd))
            co_return;
        auto data = eapi::ParseHexBytes(I2CWriteBox().Text());
        if (!data || data->empty())
        {
            ShowStatus(InfoBarSeverity::Error, L"I2C write", L"Enter the data as hex bytes, e.g. 01 02 FF.");
            co_return;
        }
        auto const size = static_cast<std::uint32_t>(data->size());
        if (!co_await ConfirmAsync(L"Write to I2C device?",
                hstring(L"Writes " + std::to_wstring(size) + L" bytes to address " + std::wstring(I2CAddressBox().Text()) +
                        L". Writing to the wrong device (e.g. a memory SPD EEPROM) can damage the system.")))
            co_return;
        co_await RunAsync(L"I2C write", [&](eapi::Library& lib)
        {
            return eapi::Call(lib.I2CWriteTransfer, bus, addr, cmd, data->data(), size);
        });
    }

    winrt::fire_and_forget MainWindow::I2CWriteRead_Click(IInspectable, RoutedEventArgs)
    {
        auto strongThis = get_strong();
        eapi::Id bus{};
        std::uint32_t addr{}, cmd{};
        if (!ReadI2CForm(bus, addr, cmd))
            co_return;
        auto writeData = eapi::ParseHexBytes(I2CWriteBox().Text());
        if (!writeData)
        {
            ShowStatus(InfoBarSeverity::Error, L"I2C write + read", L"Enter the data as hex bytes, e.g. 01 02 FF.");
            co_return;
        }
        std::uint32_t const readLength = ToU32(I2CReadLengthBox().Value(), 0, MaxTransfer);
        std::vector<std::uint8_t> readData(readLength);
        auto const writeLength = static_cast<std::uint32_t>(writeData->size());
        if (co_await RunAsync(L"I2C write + read", [&](eapi::Library& lib)
            {
                // Byte counts are "number of bytes + 1" in EApiI2CWriteReadRaw.
                return eapi::Call(lib.I2CWriteReadRaw, bus, addr, writeData->data(), writeLength + 1,
                                  readData.data(), readLength, readLength + 1);
            }) == EAPI_STATUS_SUCCESS)
            I2COutputBox().Text(readData.empty() ? std::wstring(L"(no bytes read)") : eapi::HexDump(readData, 0));
    }

    // ---- Helpers ------------------------------------------------------------------

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
