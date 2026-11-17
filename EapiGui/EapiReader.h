#pragma once
// Reads all EAPI information into plain data (no UI types), so it can run on a
// background thread and be rendered or copied as text afterwards.

#include "EapiDefs.h"
#include <string>
#include <vector>

namespace eapi
{
    class Library;

    enum class EntryState { Ok, NotSupported, Error };

    struct Entry
    {
        std::wstring name;
        std::wstring value;
        EntryState state{ EntryState::Ok };
        std::wstring detail; // tooltip: raw value / status code
    };

    struct Section
    {
        std::wstring title;
        std::vector<Entry> entries;
    };

    inline constexpr wchar_t NotSupportedText[] = L"Not supported";

    std::wstring StatusToString(Status status);

    std::vector<Section> ReadAll(Library const& lib);

    std::wstring ToText(std::vector<Section> const& sections);
}
