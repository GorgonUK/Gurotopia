#include "pch.hpp"
#include "tools/create_dialog.hpp"
#include "automate/holiday.hpp"

#include "RequestGazette.hpp"

namespace
{
constexpr const char* k_guro_discord = "https://discord.com/invite/zzWHgzaF7J";

[[nodiscard]] std::string ordinal_day(int day)
{
    if (day >= 11 && day <= 13) return "th";
    switch (day % 10)
    {
        case 1: return "st";
        case 2: return "nd";
        case 3: return "rd";
        default: return "th";
    }
}

// June 1 – July 23 (archived SummerFest / Phoenix gazette)
[[nodiscard]] bool in_summerfest_gazette_window(const std::tm& time)
{
    if (time.tm_mon == 5) return time.tm_mday >= 1;
    if (time.tm_mon == 6) return time.tm_mday <= 23;
    return false;
}

// Backed up SummerFest / Phoenix gazette — scheduled Jun 1 – Jul 23
[[nodiscard]] std::string build_summerfest_phoenix_gazette(const std::tm& time, const std::vector<std::string>& month)
{
    return ::create_dialog()
        .set_default_color("`o")
        .add_label_with_icon("big", "`wThe Gurotopia Gazette``", 5016)
        .add_spacer("small")
        .add_image_button("banner", holiday_banner(), "bannerlayout", "")
        .add_spacer("small")
        .add_textbox(std::format("`w{} {}{}: {}|", month[time.tm_mon], time.tm_mday, ordinal_day(time.tm_mday), holiday_greeting().first))
        .add_spacer("small")
        .add_textbox("The sun is shining, the beaches are calling, and `2SummerFest`` is making waves once again! Dive into a season packed with sizzling adventures, sunny surprises, and fire-y rewards! As we head into Day 2 and beyond, it's time to unlock some of the biggest highlights of the event!")
        .add_spacer("small")
        .add_textbox("The Phoenix has officially risen! For a limited time, a select group of classic `4Phoenix Items`` will make their grand comeback! Whether you missed them the first time or are looking to complete your fiery collection, now's your chance to grab them while they're still around! Check the store for more info on how to get your hands on these rare treasures!")
        .add_spacer("small")
        .add_textbox("We're also adding `8Summer Artifact Chests`` with selected `2Gem Packs``! That's right, keep an eye out for bonus rewards when purchasing selected `2Gem Packs`` throughout the rest of the `2Summerfest``! You never know what summer treat might come your way!")
        .add_spacer("small")
        .add_textbox("Visit our Social Media pages for more Content!")
        .add_spacer("small")
        .add_image_button("gazette_DiscordServer", "interface/large/gazette/gazette_5columns_social_btn01.rttex", "7imageslayout", k_guro_discord)
        .add_layout_spacer("7imageslayout")
        .add_layout_spacer("7imageslayout")
        .add_layout_spacer("7imageslayout")
        .add_layout_spacer("7imageslayout")
        .add_layout_spacer("7imageslayout")
        .add_layout_spacer("7imageslayout")
        .add_spacer("small")
        .add_image_button("gazette_PrivacyPolicy", "interface/large/gazette/gazette_3columns_policy_btn02.rttex", "3imageslayout", "https://www.ubisoft.com/en-us/privacy-policy")
        .add_image_button("gazette_GrowtopianCode", "interface/large/gazette/gazette_3columns_policy_btn01.rttex", "3imageslayout", "https://support.ubi.com/en-us/growtopia-faqs/the-growtopian-code/")
        .add_image_button("gazette_TermsofUse", "interface/large/gazette/gazette_3columns_policy_btn03.rttex", "3imageslayout", "https://legal.ubi.com/termsofuse/")
        .add_quick_exit().add_spacer("small")
        .end_dialog("gazette", "", "OK");
}

// Official Beach Party gazette (GTProxy @ 2026-07-24) — active Jul 24 – Sep 30
[[nodiscard]] std::string build_beach_party_gazette()
{
    return ::create_dialog()
        .set_default_color("`o")
        .add_label_with_icon("big", "`wThe Gurotopia Gazette``", 5016)
        .add_spacer("small")
        .add_image_button("banner", "interface/large/gui_event_beachparty.rttex", "bannerlayout", "")
        .add_spacer("small")
        .add_textbox("`wJuly 23rd: `5Beach Party``")
        .add_spacer("small")
        .add_image_button("iotm_layout", "interface/large/gazette/gazette_3columns_feature_btn09.rttex", "3imageslayout", "OPENSTORE", "main/beach_party_ticket", "")
        .add_image_button("iotm_layout", "interface/large/gazette/gazette_3columns_feature_btn03.rttex", "3imageslayout", "OPENSTORE", "main/gems_bundle06", "")
        .add_image_button("iotm_layout", "interface/large/gazette/gazette_3columns_feature_btn01.rttex", "3imageslayout", "OPENSTORE", "main/rt_grope_battlepass_bundle01", "")
        .add_spacer("small")
        .add_textbox("Summer is finally here, which means that `2Beach Party`` is rolling in with high waves of fun and more sparkling treasures from June till August!")
        .add_spacer("small")
        .add_textbox("Grab your `8Beach Party Ticket`` or `8Beach Party Training Ticket`` and head over to `2BEACHPARTYGAME``, where every dive into the sand could uncover magical pearls and seaside surprises waiting for you beneath the waves!")
        .add_spacer("small")
        .add_textbox("Collect pearls and trade them in for dazzling rewards or hunt for this weekend's shimmering catch, the magnificent `pMagic Pearls``! Bring a touch of ocean magic wherever your adventure takes you with this weekend's shimmering catch is only here for a limited time!")
        .add_spacer("small")
        .add_textbox("Looking for even more treasure? Selected `2Gem Pack`` purchases will include `9Golden Pearl Chests`` packed with rewards to help you make the most of your beachside adventure!")
        .add_spacer("small")
        .add_textbox("Make sure to check out MystikCorpse's Dev Diary below for all the sunny details!")
        .add_spacer("small")
        .add_image_button(
            "gazette_GrowtopiaForumUpdates",
            "interface/large/gazette/gazette_3columns_forum_btn01.rttex",
            "3imageslayout",
            "https://www.growtopiagame.com/forums/forum/general/announcements/7285618-beach-party-july-2026",
            "Do you want to open the Growtopia Forum and check out the latest update?",
            "")
        .add_layout_spacer("3imageslayout")
        .add_layout_spacer("3imageslayout")
        .add_spacer("small")
        .add_textbox("Freebie alert! Complete offers and earn for free!")
        .add_url_button(
            "comment",
            "`wEarn Free Gems with Xsolla``",
            "noflags",
            "https://xsolla.growtopiagame.com",
            "Complete offers and earn Free Gems.")
        .add_spacer("small")
        .add_textbox("Throw caution to the wind and embrace the mayhem with the July Grow Pass items!")
        .add_spacer("small")
        .add_label_with_icon("small", "Item of the Season: `2Cloak of Chaos``", 24)
        .add_label_with_icon("small", "Royal Item of the Season: `2Royal Cloak of Chaos``", 24)
        .add_label_with_icon("small", "Subscriber Item: `8Astronomer's Hat`` chosen by Xenoso!", 24)
        .add_spacer("small")
        .add_textbox("Please make sure to check our announcement to find out more!")
        .add_spacer("small")
        .add_image_button(
            "gazette_GrowtopiaForumUpdates",
            "interface/large/gazette/gazette_3columns_forum_btn01.rttex",
            "3imageslayout",
            "https://www.growtopiagame.com/forums/forum/general/announcements/7285615-july-update-2026",
            "Do you want to open the Growtopia Forum and check out the latest update?",
            "")
        .add_layout_spacer("3imageslayout")
        .add_layout_spacer("3imageslayout")
        .add_spacer("small")
        .add_textbox("The Growtopia survey for the June Update is still open! By completing the survey, you will receive `21 Growtoken`` if you have Advanced Account Protection enabled. Complete the survey and claim your reward on your mobile device to let us know what you think!")
        .add_spacer("small")
        .add_url_button("survey", "`wGive us your opinion!``", "noflags", "OPENSURVEY")
        .add_spacer("small")
        .add_textbox("Don't forget to join our `2Official Gurotopia Discord Server`` — click the link below!")
        .add_spacer("small")
        .add_image_button("iotm_layout", "interface/large/gazette/gazette_3columns_community_btn01.rttex", "3imageslayout", "OPENCOMMUNITY", "community_growtorials/TUTORIAL_DOORS", "")
        .add_image_button("gazette_DiscordServer", "interface/large/gazette/gazette_3columns_community_btn04.rttex", "3imageslayout", k_guro_discord, "Would you like to join our Discord Server?", "")
        .add_image_button("gazette_Youtube", "interface/large/gazette/gazette_3columns_community_btn03.rttex", "3imageslayout", "https://tiktok.com/@growtopia", "Would you like to open this in TikTok?", "")
        .add_spacer("small")
        .add_textbox("Visit our Social Media pages for more Content!")
        .add_spacer("small")
        .add_image_button("gazette_DiscordServer", "interface/large/gazette/gazette_5columns_social_btn01.rttex", "7imageslayout20", k_guro_discord, "Would you like to join our Discord Server?", "")
        .add_image_button("gazette_Instagram", "interface/large/gazette/gazette_5columns_social_btn02.rttex", "7imageslayout20", "https://www.instagram.com/growtopia", "Would you like to open this in Instagram?", "")
        .add_image_button("gazette_TikTok", "interface/large/gazette/gazette_5columns_social_btn03.rttex", "7imageslayout20", "https://tiktok.com/@growtopia", "Would you like to open this in TikTok?", "")
        .add_image_button("gazette_Twitch", "interface/large/gazette/gazette_5columns_social_btn04.rttex", "7imageslayout20", "https://www.twitch.tv/growtopiagameofficial", "Would you like to open this in Twitch?", "")
        .add_image_button("gazette_Twitter", "interface/large/gazette/gazette_5columns_social_btn06.rttex", "7imageslayout20", "https://twitter.com/growtopiagame", "Would you like to open this in X?", "")
        .add_image_button("gazette_Youtube", "interface/large/gazette/gazette_5columns_btn04.rttex", "7imageslayout20", "https://www.youtube.com/growtopia_official", "Would you like to open this in Youtube?", "")
        .add_image_button("gazette_Facebook", "interface/large/gazette/gazette_5columns_btn05.rttex", "7imageslayout20", "https://www.facebook.com/growtopia", "Would you like to open this in Facebook?", "")
        .add_spacer("small")
        .add_image_button("gazette_PrivacyPolicy", "interface/large/gazette/gazette_3columns_policy_btn02.rttex", "3imageslayout", "https://legal.ubi.com/privacypolicy/en-INTL", "Do you want to read the Privacy Policy?", "")
        .add_image_button("gazette_GrowtopianCode", "interface/large/gazette/gazette_3columns_policy_btn01.rttex", "3imageslayout", "https://ubisoft-mobile.helpshift.com/hc/en/26-growtopia/section/337-the-growtopian-code/", "Do you want to read the Growtopian Code?", "")
        .add_image_button("gazette_TermsofUse", "interface/large/gazette/gazette_3columns_policy_btn03.rttex", "3imageslayout", "https://legal.ubi.com/termsofuse/en-INTL", "Do you want to read the Terms of Use?", "")
        .add_spacer("small")
        .add_spacer("small")
        .add_quick_exit()
        .add_spacer("small")
        .add_url_button(
            "gazette_visitforum",
            "`wVisit Growtopia Forums``",
            "noflags",
            "https://www.growtopiagame.com/forums",
            "Visit the Growtopia forums?")
        .add_spacer("small")
        .add_url_button("", "`wWOTD: `1CHILDGH`` by `#Laecy````", "NOFLAGS", "OPENWORLD", "CHILDGH")
        .add_spacer("small")
        .add_url_button(
            "",
            "`wVOTW: `1A Book of 12 Years - Growtopia Animation!``",
            "NOFLAGS",
            "https://www.youtube.com/watch?v=woOXBDRohOA",
            "Watch 'A Book of 12 Years - Growtopia Animation!' by slayer7 on YouTube?")
        .set_survey_enabled(true)
        .end_dialog("gazette", "", "OK");
}
} // namespace

void on::RequestGazette(ENetEvent& event)
{
    const std::tm time = localtime();
    const std::vector<std::string> month = {
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    };

    // Outside Jun 1–Jul 23, serve Beach Party gazette (covers Jul 24–Sep and off-season).
    const std::string dialog = in_summerfest_gazette_window(time)
        ? build_summerfest_phoenix_gazette(time, month)
        : build_beach_party_gazette();

    send_varlist(event.peer, { "OnDialogRequest", dialog });
}
