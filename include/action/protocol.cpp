#include "pch.hpp"
#include "https/server_data.hpp"
#include "on/ConsoleMessage.hpp"
#include "proton/Variant.hpp"
#include "tools/crypt.hpp"
#include "tools/ip_tracker.hpp"

#include "protocol.hpp"

void action::protocol(ENetEvent& event, const std::string& header)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);
    try 
    {
        if (ip_login_blocked(event.peer->address)) throw std::runtime_error("Too many failed logins. Try again later.");

        std::vector<std::string> pipes = readch(header, '|');
        if (pipes.size() < 4ull) throw std::runtime_error("Invalid login packet.");

        bool register_account = false;
        if (pipes[2ull] == "ltoken")
        {
            const std::string decoded = base64_decode(pipes[3ull]);
            if (decoded.empty()) throw std::runtime_error("Invalid login token.");

            if (std::size_t pos = decoded.find("growId="); pos != std::string::npos) 
            {
                pos += sizeof("growId=")-1ull;
                pPeer->growid = decoded.substr(pos, decoded.find('&', pos) - pos);
            }
            if (std::size_t pos = decoded.find("password="); pos != std::string::npos) 
            {
                pos += sizeof("password=")-1ull;
                pPeer->password = decoded.substr(pos, decoded.find('&', pos) - pos);
            }
            // @note reg=1 from login page = create account; reg=0 = existing login only
            if (std::size_t pos = decoded.find("reg="); pos != std::string::npos)
            {
                pos += sizeof("reg=")-1ull;
                register_account = (decoded.substr(pos, decoded.find('&', pos) - pos) == "1");
            }
        }
        if (pPeer->growid.empty() || pPeer->password.empty()) throw std::runtime_error("Missing GrowID or password.");

        // save plaintext before mysql_select_all may overwrite pPeer->password
        const std::string plaintext = pPeer->password;
        const bool account_exists = pPeer->exists(pPeer->growid);

        if (!account_exists)
        {
            if (!register_account)
                throw std::runtime_error("GrowID not found. Register first, or check the name.");

            std::string hashed = password_hash(plaintext);
            if (hashed.empty()) throw std::runtime_error("Could not create account.");

            // Prefer a single insert; if GTLogin already wrote this GrowID into the same DB
            // between exists() and here (or a prior handoff), fall through to verify.
            if (!pPeer->mysql_insert_account(pPeer->growid, hashed))
            {
                pPeer->mysql_select_all();
                if (!password_verify(plaintext, pPeer->password))
                    throw std::runtime_error("That GrowID is already taken.");
            }
        }
        else
        {
            // existing player: load hash from DB, verify against plaintext
            pPeer->mysql_select_all(); // pPeer->password = hash from DB now
            if (!password_verify(plaintext, pPeer->password))
                throw std::runtime_error("Incorrect password.");
        }
        pPeer->mysql_select_all();
        if (!pPeer->mysql_load_progress())
            fprintf(stderr, "[peer] failed to load progress for %s\n", pPeer->growid.c_str());

        ip_login_succeeded(event.peer->address);

        send_varlist(event.peer, {
            "OnSendToServer", 
            (int)gServer_data.port, 
            0, 
            pPeer->user_id, 
            std::format("{}|0|0", gServer_data.server), 
            1, 
            pPeer->growid.c_str() // @todo idk why this is 1028 if std::string
        });
    }
    catch (const std::exception& ex) { 
        ip_login_failed(event.peer->address);
        // @note tell the client why, then logon_fail + disconnect — otherwise some
        // clients sit forever on "Connecting..." after a rejected password.
        const char *msg = ex.what();
        if (msg && msg[0] != '\0')
            on::ConsoleMessage(event.peer, std::format("`4Login failed``: {}", msg));
        send_action(*event.peer, "logon_fail", "");
        enet_peer_disconnect_later(event.peer, 0);
    }
    catch (...) { 
        ip_login_failed(event.peer->address);
        send_action(*event.peer, "logon_fail", "");
        enet_peer_disconnect_later(event.peer, 0);
    }
}
