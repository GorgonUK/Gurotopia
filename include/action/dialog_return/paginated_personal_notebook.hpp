#pragma once

extern void send_notebook_view(ENetEvent &event, int page_num);
extern void send_notebook_edit(ENetEvent &event, int page_num, const std::string &prefill);
extern void paginated_personal_notebook_view(ENetEvent &event, const ::hPipe &hPipe);
extern void paginated_personal_notebook_edit(ENetEvent &event, const ::hPipe &hPipe);
