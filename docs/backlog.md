# Backlog

The plan for taking CheeseStrike from where it is now to a game people can play with each other. It is ordered: each milestone builds on the ones before it, and within a milestone the items are roughly in the order they should be done. Each item is sized to be one pull request, or a few.

**Keeping this current:** when a pull request finishes an item, delete the item in the same pull request; the commit history is the record of what was done. Add, reorder or rewrite items whenever the plan changes. Every item follows the testing rules in [CLAUDE.md](../CLAUDE.md); the *Tests* notes below say where an item's tests are likely to go, not that they are the only ones needed.

## Where the game is now

Players can host a session in the browser, find it in the session list or by code, and join it over WebRTC, or connect directly in a native build. Up to 16 players walk, look and jump around a grey-box arena with a few cheese blocks, as colored capsules, with keyboard and mouse, gamepad or touch. Each player starts at their own spot. When the host's tab is put away, everyone else freezes with a "Waiting for host" notice; when the host leaves, they are told the session is over. Players choose a name, and fight with one weapon, the Cheddar Blaster: the host decides every hit (`game/match/match.gd`), and keeps everyone's health and score in a `MatchState` it sends to all. Matches run to a kill limit or a time limit, free-for-all or in two teams (Cheddar and Brie), as the host chose on the menu, with a winner and then a fresh match; the session list shows each host's settings. Players are cheese characters who fight with four weapons (one a lobbed, bursting projectile), pick up weapons and cheese wedges, and fill a Gouda Overload meter, on two arenas, with synthesized sound and music. There is a crosshair, hit markers, a kill feed, a scoreboard with pings and an in-game menu. Others' movement is smoothed, shots are checked against where their targets were when fired (lag compensation), impossible movement is put right, a dropped player can rejoin with their score, and a host leaving ends the session with final standings.

Things about how it works that shape the plan:

- **Each player simulates their own movement** (`game/player/player.gd`) and a `MultiplayerSynchronizer` sends position and facing to everyone else. That is fine for movement, but anything that decides who wins, such as hits, damage and score, is decided by the host, or any player could award themselves kills. Rules live in plain classes (`game/combat/`, `game/match/`) so they can be tested without a network.
- **The host is a browser tab**, possibly on a phone. Whatever the host simulates for everyone has to stay cheap, and the session ends when the host leaves.
- **The signaling server only runs on the development PC**, and the game looks for it on the same machine as the page, at port 9080. Nobody outside the local network can play yet.
- **The web export is single-threaded** (`thread_support=false`), which lets it run on any static host without special headers. Keep it that way unless something needs threads badly enough to justify the hosting requirements.

## Milestone 1: Playable from anywhere

Before there is much more game, make it possible for people on different networks to reach it, so every later milestone can be playtested with real players.

1. **Host the signaling server publicly.** Run `signaling/server.py` behind TLS (`wss://`) on a small server or a container platform, with automatic restarts and a health check. Add a log line per session hosted and closed so problems can be traced.
   *Tests:* signaling tests for anything added to the server, such as a health endpoint. Manual: DEV-06 becomes runnable.
2. **Point the web build at the public server.** Replace the same-host-port-9080 guess in `Net._web_signaling_url()` with a URL set at export time, keeping the `?signal=` override and falling back to the current behavior for local development.
   *Tests:* pull the URL choice into a plain function and cover it with a GUT test.
3. **Add a TURN server.** STUN alone fails between players behind strict NATs, which is common on mobile data. Run a TURN server (coturn, or a hosted one) and hand out short-lived credentials from the signaling server rather than putting a fixed secret in the build.
   *Tests:* signaling tests for issuing credentials. Manual: a DEV row for a phone on mobile data joining a PC on home Wi-Fi.
4. **Publish the web build.** A workflow that exports the web build on every push to `main` and deploys it to a static host (GitHub Pages or itch.io), so there is always a current build to share.
5. **Version check on join.** Send the game's version with `host` and `join`, and have the signaling server refuse a join between different versions with a clear message ("This session is running an older version. Reload the page."). Once builds are deployed continuously, mismatched tabs will happen.
   *Tests:* signaling tests for the refusal; a browser test that the message reaches the menu.

## Milestone 6: Polish and release

1. **Invite links.** A Copy Link button that gives `…/?join=ABCD`; opening it goes straight to joining. Optionally, private sessions that are not in the list.
   *Tests:* browser test for joining by link; signaling tests for private sessions.
2. **First-time help.** A short controls overlay for the device being used, shown on the first visit.
3. **Accessibility.** Colorblind-safe team and hit colors, scalable HUD text, captions for important sounds and reduced screen shake.
4. **Signaling server protection.** Rate limiting per address, message size limits, and filtering of names, so the public server can't be easily flooded or abused.
   *Tests:* signaling tests for each limit.
5. **Release.** Version number on the menu, a changelog, a page on itch.io, and a round of playtests with people outside the project, running the full manual test list first.

## Later

Ideas worth keeping that aren't planned yet: more modes (capture the cheese wheel, last mouse standing), cosmetics, a map editor, bots for practicing alone, desktop builds on Steam, and a dedicated server option for large or competitive matches.
