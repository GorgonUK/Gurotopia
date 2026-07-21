#pragma once

void battlepass(ENetEvent &event, const ::hPipe &hPipe);
void send_battlepass_tasks(ENetEvent &event);
void send_battlepass_rewards(ENetEvent &event);
void send_battlepass_leaderboard(ENetEvent &event);
