#pragma once

namespace action
{
    extern void worldlock_storage_getpage(ENetEvent &event, const std::string &header);
    extern void worldlock_storage_modify_amount(ENetEvent &event, const std::string &header);
}
