#pragma once

/* begin (or resume) operating on target; caller must hold a Surg-E (4296) */
extern void surgery_start(ENetEvent &event, ENetPeer *target);

/* dialog_return handler for the surgery tool dialog */
extern void surgery(ENetEvent &event, const ::hPipe &hPipe);
