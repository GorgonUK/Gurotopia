#pragma once

namespace on
{
    struct emoticon_entry
    {
        std::string_view name;
        std::string_view glyph;
        std::string_view unlock_hint;
        bool unlocked;
    };

    extern const std::vector<emoticon_entry>& emoticon_catalog();
    extern void EmoticonDataChanged(ENetEvent& event);
}
