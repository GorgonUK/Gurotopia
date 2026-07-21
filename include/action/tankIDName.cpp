#include "pch.hpp"
#include "automate/holiday.hpp"
#include "database/database.hpp"

#include "tankIDName.hpp"

static std::string growid_for_uid(int uid)
{
    if (uid <= 0) return {};

    ::hStmt hStmt{ "SELECT growid FROM peer WHERE uid = ? LIMIT 1" };
    MYSQL_BIND param = make_bind_in(uid);
    if (mysql_stmt_bind_param(hStmt.pStmt, &param))
        return {};

    std::string growid;
    unsigned long length = 0;
    MYSQL_BIND result = make_bind_out(growid);
    result.length = &length;
    if (mysql_stmt_bind_result(hStmt.pStmt, &result))
        return {};
    if (mysql_stmt_execute(hStmt.pStmt) || mysql_stmt_store_result(hStmt.pStmt))
        return {};
    if (mysql_stmt_num_rows(hStmt.pStmt) == 0 || mysql_stmt_fetch(hStmt.pStmt) != 0)
        return {};

    growid.resize(length);
    return growid;
}

void action::tankIDName(ENetEvent& event, const std::string& header)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);

    std::vector<std::string> pipes = readch(header, '|');
    if (pipes.empty() || pipes.size() < 41ull) enet_peer_disconnect_later(event.peer, 0);

    // @note capture before mysql_load_progress — that overwrites peer fields from DB
    // (including an empty country), which used to wipe the flag the client just sent.
    std::string login_country;

    for (std::size_t i = 0; i + 1 < pipes.size(); ++i) 
    {
        if      (pipes[i] == "tankIDName")   pPeer->growid = pipes[i+1];
        else if (pipes[i] == "country")      login_country = pipes[i+1];
        else if (pipes[i] == "user")         pPeer->user_id = std::stoi(pipes[i+1]); // @todo validate user_id
    }

    // @note after OnSendToServer the client may send an empty tankIDName (ltoken /
    // Google logins). Recover the GrowID from uid so OnSpawn name / wrench work.
    if (pPeer->growid.empty() && pPeer->user_id > 0)
        pPeer->growid = growid_for_uid(pPeer->user_id);

    // @note game-session peer is a fresh reconnect after OnSendToServer; the
    // account metadata and progression loaded on the login peer were discarded,
    // so load both again here (before enter_game grants the starter kit).
    // This includes created_at, which the wrench menu uses for account age.
    if (!pPeer->growid.empty())
        pPeer->mysql_select_all();
    if (!pPeer->mysql_load_progress())
        fprintf(stderr, "[peer] failed to load progress for %s\n", pPeer->growid.c_str());
    pPeer->start_playtime_session();

    // Prefer the country the client just reported (ISO code, e.g. "gb" / "us").
    // Persist it so later sessions still show the flag if the packet omits it.
    if (!login_country.empty())
    {
        for (char &c : login_country)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (login_country.size() > 8)
            login_country.resize(8);
        if (pPeer->country != login_country)
        {
            pPeer->country = login_country;
            pPeer->mark_dirty();
        }
    }

    send_varlist(event.peer, { "OnOverrideGDPRFromServer", 18, 1, 0, 1 });

    /* v5.51 */
    send_varlist(event.peer, {
        "OnSuperMainStartAcceptLogonHrdxs47254722215a",
        2966867045u, // @note items.dat
        "ubistatic-a.akamaihd.net",
        "0098/150726456789/cache/",
        "cc.cz.madkite.freedom org.aqua.gg idv.aqua.bulldog com.cih.gamecih2 com.cih.gamecih com.cih.game_cih cn.maocai.gamekiller com.gmd.speedtime org.dax.attack com.x0.strai.frep com.x0.strai.free org.cheatengine.cegui org.sbtools.gamehack com.skgames.traffikrider org.sbtoods.gamehaca com.skype.ralder org.cheatengine.cegui.xx.multi1458919170111 com.prohiro.macro me.autotouch.autotouch com.cygery.repetitouch.free com.cygery.repetitouch.pro com.proziro.zacro com.slash.gamebuster",
        std::format(
            "proto=225|choosemusic=audio/mp3/about_theme.mp3|active_holiday={}|wing_week_day=0|ubi_week_day=0|server_tick=31402182|game_theme={}|clash_active=0|drop_lavacheck_faster=1|isPayingUser=1|usingStoreNavigation=1|enableInventoryTab=1|bigBackpack=1|seed_diary_hash=4266294761", 
            holiday, game_theme_string()
        ) + "|m_clientBits=|eventButtons={\"EventButtonData\":[{\"active\":false,\"buttonAction\":\"eventmenu\",\"buttonTemplate\":\"BaseEventButton\",\"counter\":0,\"counterMax\":0,\"itemIdIcon\":6244,\"name\":\"ClashEventButton\",\"order\":9,\"rcssClass\":\"clash-event\",\"text\":\"\"},{\"active\":true,\"buttonAction\":\"dailychallengemenu\",\"buttonTemplate\":\"BaseEventButton\",\"counter\":0,\"counterMax\":0,\"itemIdIcon\":23,\"name\":\"DailyChallenge\",\"order\":10,\"rcssClass\":\"daily_challenge\",\"text\":\"\"},{\"active\":true,\"buttonAction\":\"openPiggyBank\",\"buttonTemplate\":\"BaseEventButton\",\"counter\":0,\"counterMax\":0,\"name\":\"PiggyBankButton\",\"order\":20,\"rcssClass\":\"piggybank\",\"text\":\"\"},{\"active\":false,\"buttonAction\":\"showdungeonsui\",\"buttonTemplate\":\"DungeonEventButton\",\"counter\":0,\"counterMax\":20,\"name\":\"ScrollsPurchaseButton\",\"order\":30,\"rcssClass\":\"scrollbank\",\"text\":\"\"},{\"active\":false,\"buttonAction\":\"show_mailbox_ui\",\"buttonTemplate\":\"BaseEventButton\",\"counter\":0,\"counterMax\":0,\"name\":\"MailboxButton\",\"order\":30,\"rcssClass\":\"mailbox\",\"text\":\"\"},{\"active\":false,\"buttonAction\":\"show_auction_ui\",\"buttonTemplate\":\"BaseEventButton\",\"counter\":0,\"counterMax\":0,\"name\":\"AuctionButton\",\"order\":30,\"rcssClass\":\"auction\",\"text\":\"\"},{\"active\":false,\"buttonTemplate\":\"ActiveAuctionEventButton\",\"counter\":0,\"counterMax\":20,\"name\":\"ActiveAuctionButton\",\"order\":30,\"rcssClass\":\"activeauction\",\"text\":\"\"},{\"active\":false,\"buttonAction\":\"eventmenu\",\"buttonTemplate\":\"BaseEventButton\",\"counter\":0,\"counterMax\":0,\"itemIdIcon\":6244,\"name\":\"ClashEventButton\",\"order\":21,\"rcssClass\":\"clash-event\",\"text\":\"\"},{\"active\":false,\"buttonAction\":\"show_bingo_ui\",\"buttonTemplate\":\"BaseEventButton\",\"counter\":0,\"counterMax\":0,\"name\":\"WinterBingoButton\",\"order\":49,\"rcssClass\":\"wf-bingo\",\"text\":\"\"},{\"active\":false,\"buttonAction\":\"show_bingo_ui\",\"buttonTemplate\":\"BaseEventButton\",\"name\":\"UbiBingoButton\",\"order\":50,\"rcssClass\":\"ubi-bingo\",\"text\":\"\"},{\"active\":false,\"buttonAction\":\"winterrallymenu\",\"buttonTemplate\":\"BaseEventButton\",\"counter\":0,\"counterMax\":0,\"name\":\"WinterRallyButton\",\"order\":50,\"rcssClass\":\"winter-rally\",\"text\":\"\"},{\"active\":false,\"buttonAction\":\"leaderboardBtnClicked\",\"buttonTemplate\":\"BaseEventButton\",\"counter\":0,\"counterMax\":0,\"name\":\"AnniversaryLeaderboardButton\",\"order\":50,\"rcssClass\":\"anniversary-leaderboard\",\"text\":\"\"},{\"active\":false,\"buttonAction\":\"euphoriaBtnClicked\",\"buttonTemplate\":\"BaseEventButton\",\"counter\":0,\"counterMax\":0,\"name\":\"AnniversaryEuphoriaButton\",\"order\":50,\"rcssClass\":\"anniversary-euphoria\",\"text\":\"\"},{\"active\":false,\"buttonAction\":\"openLnySparksPopup\",\"buttonTemplate\":\"BaseEventButton\",\"counter\":0,\"counterMax\":5,\"name\":\"LnyButton\",\"order\":50,\"rcssClass\":\"cny\",\"text\":\"\"},{\"active\":false,\"buttonAction\":\"ShowValentinesQuestDialog\",\"buttonTemplate\":\"EventButtonWithCounter\",\"counter\":0,\"counterMax\":100,\"name\":\"ValentinesButton\",\"order\":50,\"rcssClass\":\"valentines_day\",\"text\":\"\"},{\"active\":false,\"buttonAction\":\"showegseeventui\",\"buttonTemplate\":\"EventButtonWithCounter\",\"counter\":0,\"counterMax\":20,\"name\":\"EasterButton\",\"order\":50,\"rcssClass\":\"easter_event\",\"text\":\"\"},{\"active\":false,\"buttonAction\":\"openStPatrickPiggyBank\",\"buttonTemplate\":\"BaseEventButton\",\"name\":\"StPatrickPBButton\",\"order\":50,\"rcssClass\":\"st_patrick_event\",\"text\":\"\"},{\"active\":false,\"buttonAction\":\"dailyrewardmenu\",\"buttonTemplate\":\"BaseEventButton\",\"counter\":0,\"counterMax\":1,\"name\":\"CincoPinataButton\",\"order\":50,\"rcssClass\":\"cinco_pinata_event\",\"text\":\"\"},{\"active\":false,\"buttonAction\":\"show_fruit_mixer_dialog\",\"buttonTemplate\":\"BaseEventButton\",\"counter\":0,\"counterMax\":0,\"name\":\"SPP_TropicalFruitsButton\",\"order\":50,\"rcssClass\":\"spp_tropical_fruits\",\"text\":\"\"},{\"active\":false,\"buttonAction\":\"claimprogressbar\",\"buttonTemplate\":\"EventButtonWithCounter\",\"counter\":0,\"counterMax\":0,\"name\":\"SummerfestButton\",\"order\":50,\"rcssClass\":\"summerfest\",\"text\":\"\"},{\"active\":false,\"buttonAction\":\"claimprogressbar\",\"buttonTemplate\":\"EventButtonWithCounter\",\"counter\":0,\"counterMax\":15,\"name\":\"HalloweenButton\",\"order\":50,\"rcssClass\":\"halloween\",\"text\":\"\"}]}"
    });
}
