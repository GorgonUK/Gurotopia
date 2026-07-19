#include "pch.hpp"
#include "on/ConsoleMessage.hpp"
#include "tools/ransuu.hpp"
#include "tools/create_dialog.hpp"
#include "database/achievements.hpp"

#include "surgery.hpp"

namespace
{
    constexpr u_char MAX_MISTAKES = 3;
    constexpr std::size_t MAX_STEPS = 6ull;

    /* every surgical tool a patient may need (ids from items.dat) */
    constexpr std::array<short, 11ull> surgery_tools
    {
        1258/*Sponge*/, 1260/*Scalpel*/, 1262/*Anesthetic*/, 1264/*Antiseptic*/, 
        1266/*Antibiotics*/, 1268/*Splint*/, 1270/*Stitches*/, 4308/*Pins*/, 
        4310/*Transfusion*/, 4314/*Clamp*/, 4316/*Ultrasound*/
    };

    struct ailment
    {
        std::string_view name{};
        std::array<short, MAX_STEPS> steps{}; // @note required tools in order, 0-padded
    };
    constexpr std::array<::ailment, 5ull> ailments
    {{
        { "Broken Arm",        { 1262, 1268, 1264, 1270 } },
        { "Appendicitis",      { 1262, 1260, 4314, 1258, 1270, 1266 } },
        { "Deep Laceration",   { 1264, 4314, 1270, 1266 } },
        { "Torn Ligament",     { 4316, 1262, 1260, 4308, 1270 } },
        { "Internal Bleeding", { 4316, 1262, 1260, 4310, 1270, 1266 } }
    }};

    struct session
    {
        int patient_uid{};
        int surgeon_uid{};
        std::string world{};
        u_char ailment{};
        u_char step{};
        u_char mistakes{};

        u_char total_steps() const
        {
            return std::ranges::count_if(ailments[ailment].steps, std::identity{});
        }
        short next_tool() const
        {
            return ailments[ailment].steps[step];
        }
    };
    std::vector<::session> sessions{};

    ::session *find_session(int patient_uid, const std::string &world)
    {
        auto it = std::ranges::find_if(sessions, [&](const ::session &s) 
        { 
            return s.patient_uid == patient_uid && s.world == world; 
        });
        return (it != sessions.end()) ? &*it : nullptr;
    }

    ENetPeer *find_patient(const std::string &world, int patient_uid)
    {
        ENetPeer *found{};
        peers(world, PEER_SAME_WORLD, [patient_uid, &found](ENetPeer &peer)
        {
            if (static_cast<::peer*>(peer.data)->user_id == patient_uid) found = &peer;
        });
        return found;
    }

    void show_dialog(ENetEvent &event, const ::session &session, const ::peer &patient)
    {
        ::create_dialog dialog;
        dialog
            .set_default_color("`o")
            .add_label_with_icon("big", "`wSurgery``", 4296)
            .embed_data("patient", session.patient_uid)
            .add_spacer("small")
            .add_textbox(std::format("Patient: `2{}``", patient.growid))
            .add_textbox(std::format("Diagnosis: `4{}``", ailments[session.ailment].name))
            .add_textbox(std::format("Progress: `2{}``/`2{}``  Mistakes: `4{}``/`4{}``", 
                session.step, session.total_steps(), session.mistakes, MAX_MISTAKES))
            .add_spacer("small")
            .add_textbox(std::format("The patient needs: `w{}``", id_to_item(session.next_tool()).raw_name))
            .add_spacer("small");
        for (short tool : surgery_tools)
            dialog.add_button(std::format("tool_{}", tool), std::format("`w{}``", id_to_item(tool).raw_name));
        dialog.add_spacer("small");

        send_varlist(event.peer, { "OnDialogRequest", dialog.end_dialog("surgery", "Step Away", "") });
    }
}

void surgery_start(ENetEvent &event, ENetPeer *target)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);
    ::peer *pTarget = static_cast<::peer*>(target->data);

    if (std::ranges::find(pPeer->slots, 4296/*Surg-E*/, &::slot::id) == pPeer->slots.end())
    {
        send_varlist(event.peer, { "OnTalkBubble", pPeer->netid, "`oYou need a `wSurg-E`` unit to perform surgery!``", 0u, 1u });
        return;
    }

    ::session *session = find_session(pTarget->user_id, pPeer->recent_worlds.back());
    if (session == nullptr)
    {
        ransuu ransuu;
        sessions.emplace_back(pTarget->user_id, pPeer->user_id, pPeer->recent_worlds.back(), 
            (u_char)ransuu[{0, (int)ailments.size() - 1}]);
        session = &sessions.back();

        const std::string message = std::format("`{}{}`` begins operating on `{}{}``!", 
            pPeer->prefix, pPeer->growid, pTarget->prefix, pTarget->growid);
        peers(pPeer->recent_worlds.back(), PEER_SAME_WORLD, [message](ENetPeer& peer) 
        {
            on::ConsoleMessage(&peer, message);
        });
    }
    else if (session->surgeon_uid != pPeer->user_id)
    {
        send_varlist(event.peer, { "OnTalkBubble", pPeer->netid, "`oSomeone else is already operating on this patient!``", 0u, 1u });
        return;
    }

    show_dialog(event, *session, *pTarget);
}

void surgery(ENetEvent &event, const ::hPipe &hPipe)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);

    const std::string &buttonClicked = hPipe["buttonClicked"];
    if (!buttonClicked.starts_with("tool_")) return;
    const short tool = atoi(buttonClicked.c_str() + sizeof("tool_") - 1ull);
    if (std::ranges::find(surgery_tools, tool) == surgery_tools.end()) return;

    const std::string world = pPeer->recent_worlds.back();
    ::session *session = find_session(atoi(hPipe["patient"].c_str()), world);
    if (session == nullptr || session->surgeon_uid != pPeer->user_id) return;

    ENetPeer *target = find_patient(world, session->patient_uid);
    if (target == nullptr) // @note patient left mid-surgery
    {
        std::erase_if(sessions, [session](const ::session &s) { return &s == session; });
        on::ConsoleMessage(event.peer, "`oYour patient has left the operating table!``");
        return;
    }
    ::peer *pTarget = static_cast<::peer*>(target->data);

    const auto slot = std::ranges::find(pPeer->slots, tool, &::slot::id);
    if (slot == pPeer->slots.end() || slot->count < 1)
    {
        send_varlist(event.peer, { "OnTalkBubble", pPeer->netid, 
            std::format("`oYou don't have any `w{}``!``", id_to_item(tool).raw_name), 0u, 1u });
        show_dialog(event, *session, *pTarget);
        return;
    }
    modify_item_inventory(event, ::slot(tool, -1)); // @note every attempt uses up the tool

    if (tool == session->next_tool())
    {
        if (++session->step >= session->total_steps()) // @note surgery complete
        {
            const std::string message = std::format("`{}{}`` successfully cured `{}{}``'s `4{}``!", 
                pPeer->prefix, pPeer->growid, pTarget->prefix, pTarget->growid, ailments[session->ailment].name);
            peers(world, PEER_SAME_WORLD, [pPeer, message](ENetPeer& peer) 
            {
                send_varlist(&peer, { "OnTalkBubble", pPeer->netid, message, 0u, 1u });
                on::ConsoleMessage(&peer, message);
            });

            pPeer->add_xp(event, 200);
            std::erase_if(sessions, [session](const ::session &s) { return &s == session; });
            achievement_progress(event, ACH_SURGERIES_DONE);
            return;
        }
        send_varlist(event.peer, { "OnTalkBubble", pPeer->netid, "`2That's exactly what the patient needed!``", 0u });
    }
    else if (++session->mistakes >= MAX_MISTAKES) // @note botched surgery
    {
        const std::string message = std::format("`{}{}`` botched the surgery on `{}{}``!", 
            pPeer->prefix, pPeer->growid, pTarget->prefix, pTarget->growid);
        peers(world, PEER_SAME_WORLD, [pPeer, message](ENetPeer& peer) 
        {
            send_varlist(&peer, { "OnTalkBubble", pPeer->netid, message, 0u, 1u });
            on::ConsoleMessage(&peer, message);
        });

        /* @note same visuals as action::respawn, aimed at the patient */
        send_varlist(target, { "OnSetFreezeState", 2 }, pTarget->netid);
        send_varlist(target, { "OnKilled" }, pTarget->netid);
        send_varlist(target, {
            "OnSetPos", 
            CL_Vec2f{pTarget->rest_pos.x, pTarget->rest_pos.y}
        }, pTarget->netid, 1900);
        send_varlist(target, { "OnSetFreezeState" }, pTarget->netid, 1900);

        std::erase_if(sessions, [session](const ::session &s) { return &s == session; });
        return;
    }
    else send_varlist(event.peer, { "OnTalkBubble", pPeer->netid, "`4That wasn't the right tool!``", 0u });

    show_dialog(event, *session, *pTarget);
}
