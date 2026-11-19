#pragma once
// Reads all EAPI information into plain data (no UI types), so it can run on a
// background thread and be rendered or copied as text afterwards.

#include "EapiDefs.h"
#include <optional>
#include <string>
#include <string_view>
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

    struct GpioPin
    {
        Id id;              // bank ID or single-pin ID
        std::uint32_t bit;  // bitmask selecting this pin within `id`
        std::uint32_t number;
        bool capsKnown;     // false: library does not report capabilities, both directions assumed
        bool canInput, canOutput;
        Status directionStatus, levelStatus;
        bool isInput, isHigh;
    };

    // Enumerates GPIO banks; falls back to single-pin IDs 0..15 when no bank is supported.
    // Returns the capability status of the first ID when no pin was found.
    Status ReadGpio(Library const& lib, std::vector<GpioPin>& pins);

    // "50", "0x50" -> 0x50. nullopt for empty, non-hex or more than 8 digits.
    std::optional<std::uint32_t> ParseHex32(std::wstring_view text);
    // "01 0x02,ff 0a0b" -> {01 02 ff 0a 0b}. Tokens are 1 digit or an even number of digits.
    std::optional<std::vector<std::uint8_t>> ParseHexBytes(std::wstring_view text);
    // 16 bytes per line: "00000010  01 02 ...  |..|"
    std::wstring HexDump(std::vector<std::uint8_t> const& data, std::uint32_t baseOffset);
}
