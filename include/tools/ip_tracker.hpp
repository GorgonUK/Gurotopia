#pragma once

/* tracks failed logins per IP; 5 fails within 10 minutes = 15 minute block.
   all in-memory (reset on server restart). */

/* true while this address is blocked from logging in */
extern bool ip_login_blocked(const ENetAddress &address);

/* record one failed login attempt (may start a block) */
extern void ip_login_failed(const ENetAddress &address);

/* successful login clears earlier fails for this address */
extern void ip_login_succeeded(const ENetAddress &address);
