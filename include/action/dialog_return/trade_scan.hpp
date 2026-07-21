#pragma once

void trade_scan(ENetEvent &event, const ::hPipe &hPipe);
void send_trade_scan_main(ENetEvent &event);
void send_trade_scan_search(ENetEvent &event);
void send_trade_scan_price_check(ENetEvent &event);
void send_trade_scan_buy_credits(ENetEvent &event);
