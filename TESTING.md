# Feature test checklist

Use after deploying to the VPS. **Keep a feature permanently (commit/push) only if it passes.**

## 0. Deploy health

- [ ] `docker compose up -d --build server` succeeds
- [ ] Logs show `listening on …:17091`, MariaDB connected, `items.dat parsed`
- [ ] `RestartCount` stays `0` after exercises below

## 1. Action logging

- [ ] Connect and log in
- [ ] Enter a world, leave to world select, chat a line, place a block, break a block, drop an item, trash an item (optional: buy from store)
- [ ] On host/VPS: `logs/gurotopia-YYYY-MM-DD.log` has lines for `login`, `world_enter`, `world_leave`, `chat`, `place`, `break`, `drop`, `trash` (and `buy` if tested)
- [ ] Lines include growid(uid) and world name where relevant
- [ ] No sustained CPU spike from logging alone

## 2. World Lock invite

Requires two accounts in the same **owned** world (World Lock placed).

- [ ] Owner wrenches invitee → sees **Add to World Lock**
- [ ] Non-owner wrenching someone else → button **absent**
- [ ] Owner clicks invite → invitee gets Accept/Decline dialog
- [ ] Decline → invitee message; no access; owner can invite again
- [ ] Accept → invitee message; owner notified; invitee name goes cyan/`c` if applicable
- [ ] Invitee can place/break in the private locked world
- [ ] After both rejoin the world, access still works (persisted)
- [ ] Inviting someone who already has access shows “already have access”

## 3. Mobile ad-background (new worlds only)

- [ ] Create a **brand-new** world name (old worlds keep old sky)
- [ ] On iPhone: empty sky should **not** show the ad banner
- [ ] If ads still appear → fail this feature (revert `generate_world` sky bg change)

## Pass / fail

| Feature | Pass? | Action |
|---------|-------|--------|
| Logging | | commit or revert |
| World Lock invite | | commit or revert |
| Ad background | | commit or revert |
| AGENTS.md | always | commit with docs |
