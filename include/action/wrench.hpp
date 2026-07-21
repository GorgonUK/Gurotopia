#pragma once

namespace action
{ 
    extern void wrench(ENetEvent& event, const std::string& header);
    extern void send_self_wrench_menu(ENetEvent &event, int netid);
}
