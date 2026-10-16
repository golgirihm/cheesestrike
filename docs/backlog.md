# Backlog

The plan for taking CheeseStrike from where it is now to a game that people can play with each other and that feels as finished as a big-budget shooter: the features players expect of one, and the polish. It is ordered: each milestone builds on the ones before it, and within a milestone the items are roughly in the order they should be done. Each item is sized to be one pull request, or a few.

**Keeping this current:** when a pull request finishes an item, delete the item in the same pull request; the commit history is the record of what was done. Add, reorder or rewrite items whenever the plan changes. Every item follows the testing rules in [CLAUDE.md](../CLAUDE.md); the *Tests* notes below say where an item's tests are likely to go, not that they are the only ones needed.

## Where the game is now

Players can host a session in the browser, find it in the session list or by code, and join it over WebRTC, or connect directly in a native build. Up to 16 play free-for-all or in two teams, to a kill limit or a time limit, on three arenas (Cheddar Canyon, Brie Box and Curd City), each fenced at its edge with scenery beyond and a sky with clouds.

Players are wedges of cheese with drawn-on faces and stick limbs, who walk, run and jump with keyboard and mouse or gamepad (touch works too, but phones are deprecated: see [CLAUDE.md](../CLAUDE.md#phones-are-deprecated)). They start with a sidearm and pick up four more weapons, one of them a launcher whose wheels burst, and cheese wedges for health. Weapons can be fired from the hip or aimed down their sights. Dealing damage fills a Gouda Overload meter. The host decides every hit (`game/match/match.gd`) and keeps everyone's health and score in a `MatchState` it sends to all. Others' movement is smoothed, shots are checked against where their targets were when fired, impossible movement is put right, and a dropped player can rejoin with their score.

On screen there is a crosshair that shows spread, hit markers, a kill feed, a scoreboard with pings, an in-game menu, a list of controls on a first session, and a settings screen (look sensitivity, field of view, volume, graphics quality, keys, touch layout, text size, captions and screen shake). Sound and music are synthesized in code. On phones the game goes full screen and sideways and can be installed to the home screen.

What it doesn't have yet is most of what makes a shooter feel finished: recoil, crouching, reload animations, more than a handful of weapons and modes, textures, particles, recorded sound, a front end, anything to earn, or a way to find a match with one button. Milestones 2 to 14 are that work, roughly in the order a player would feel it.

Things about how it works that shape the plan:

- **Each player simulates their own movement** (`game/player/player.gd`) and a `MultiplayerSynchronizer` sends position and facing to everyone else. That is fine for movement, but anything that decides who wins, such as hits, damage and score, is decided by the host, or any player could award themselves kills. Rules live in plain classes (`game/combat/`, `game/match/`) so they can be tested without a network.
- **The host is a browser tab** on a desktop: phones are deprecated and no longer a limit. Whatever the host simulates for everyone has to stay cheap, and the session ends when the host leaves.
- **The signaling server is serverless on AWS** ([backend.md](backend.md)): one Lambda function, a WebSocket API and a DynamoDB table, with the game's files in an S3 bucket. Only hosts stay connected to it. `python infra/deploy.py` deploys it; in development the same rules run as one process on port 9080.
- **The web export is single-threaded** (`thread_support=false`), which lets it run on any static host without special headers. Keep it that way unless something needs threads badly enough to justify the hosting requirements.
- **The arena editor needs a keyboard and mouse**, and makes flat arenas: a floor of one of three sizes with blocks, walls, crates and pads on a grid (`game/world/arena_code.gd`). An arena is a short code, kept in the player's settings and shared in a link; the host sends it to whoever joins.
- **The light is not baked.** Each arena has a sun, a cool fill light, soft dark edges drawn on the ground round what stands on it, and in some a few lamps: all cheap, and all worked out as the game runs. Real baked lighting needs the editor to bake it and the baked maps to be downloaded.
- **The renderer is Compatibility**, the one that runs in browsers. It has no ambient occlusion, volumetric light or screen-space reflections, so the look has to come from shapes, colour, baked light and particles.
- **The session list knows two modes and three arenas.** The server that keeps the list accepts only free-for-all and teams, and only the first three arenas, so a session in any newer mode is listed as whichever of those two is nearer, and one on a newer arena, or on one a player laid out, as Cheddar Canyon (`MatchSettings.advertised`). Nor is it told of bots, the host's rules or the sky. Players get all of it once they join. Listing it properly means teaching `signaling/` the rest.
- **There is no server that keeps anything.** Names and settings live in each browser. Anything a player earns and keeps (Milestone 11) needs either that, which can't be trusted, or a service and a database, which have to be run and paid for.
- **Almost everything is made in code**: meshes, sounds, arenas' dressing. That has kept the build small and the look consistent, and it has a ceiling. Recorded sound, textures and animation (Milestones 4, 7 and 9) mean taking on assets, their licences and their size.

## First: large maps, and the mice that eat them

What makes the game its own: maps worth crossing, built mostly of cheese, and giant mice that come out partway through a match and eat it away, so that there is less and less to hide behind. This comes before everything below.

Curd City is the first large map (`game/world/maps/curd_city.gd`, built by `game/world/big_map.gd`): a town about 640 by 400 metres, a minute's run from one team's yard to the other's, with houses of cheese that can be walked into, ramps between their floors, ladders to their roofs, and routes bots find their way by (`game/bots/routes.gd`). Cracker Duel, the practice range and arenas laid out in the editor stay small.

1. **Giant mice.** About a minute into a match, with a red countdown on the HUD beforehand ("Mice invade in 0:42"), giant mice come out and eat the cheese: each makes for a building or a block, eats it bite by bite until it is gone, and moves on, with more of them as the match goes on. A mouse that is shot runs off for a while, which gives away whoever shot it; they don't hurt players. They replace the small mice that now gnaw a block every twenty seconds (`BlockRules`), and the host decides where each goes, as it decides everything else.
   *Tests:* the mice's rules in a plain class like `BlockRules`, with GUT tests of when they come, where they go, what they eat and running off when shot; a browser test that both windows see the same mouse and the same cheese go. Manual: how they look and sound, and how a match feels as cover goes.
2. **A minimap.** A small map in a corner of the HUD: teammates always shown as dots, and an enemy only for a few seconds after they fire a gun (not the knife), at the spot they fired from, not followed after.
   *Tests:* who is shown and for how long in a plain class; GUT tests of the HUD.
3. **The other arenas, large.** Rebuild Cheddar Canyon, Brie Box, Double Cream Dairy, Crumb Kitchen, Rind Market and Gruyère Grotto as large maps with `BigMap`, one or two a pull request, each keeping its own look: the canyon's mesas, the dairy's tanks, the kitchen's giant counters. `BigMap` will want more kinds of building as it goes.
   *Tests:* `test_big_maps.gd` and `test_interiors.gd` run over every large map. Some browser tests rely on Cheddar Canyon being small and should move to Cracker Duel when it is rebuilt.
4. **Bots that climb.** Bots go in and out of houses by the doors, but never up a ramp or a ladder. Add the floors and roofs to the routes.
   *Tests:* `test_big_maps.gd`.

## Milestone 1: Playable from anywhere

Before there is much more game, make it possible for people on different networks to reach it, so every later milestone can be playtested with real players.

1. **Deploy for the first time, and prove it.** The backend has only ever run against imitations of AWS. Follow the steps in [backend.md](backend.md), run `python infra/deploy.py`, and work through the *Deployed game* manual tests (AWS-01 to AWS-09), fixing whatever the real services do differently.
   *Tests:* a rule that turns out wrong goes in `signaling/test_stores.py`, so both stores are held to the fix.
2. **Log sessions on AWS.** A line per session hosted, resumed and closed, so problems can be traced in CloudWatch, with a short retention so the logs cost nothing.
3. **Add a TURN server.** STUN alone fails between players behind strict NATs, such as those on many office, campus and mobile networks. Run a TURN server (coturn, or a hosted one) and hand out short-lived credentials from the signaling server rather than putting a fixed secret in the build.
   *Tests:* signaling tests for issuing credentials. Manual: a DEV row for a PC behind a strict NAT, such as a phone's hotspot, joining a PC on home Wi-Fi.
4. **Deploy on every push to `main`.** A workflow that runs `infra/deploy.py`, so there is always a current build to share. It should sign in to AWS through GitHub's OIDC provider and a role limited to this stack, not a stored access key.
5. **Version check on join.** Send the game's version with `host` and `join`, and have the signaling server refuse a join between different versions with a clear message ("This session is running an older version. Reload the page."). Once builds are deployed continuously, mismatched tabs will happen.
   *Tests:* signaling tests for the refusal; a browser test that the message reaches the menu.

## Milestone 2: Gunplay that feels right

The shooting works and is fair. This milestone is about how it feels in the hand: what a big-budget shooter spends most of its polish on.

1. **More kinds of grenade.** Beside the wax grenade that bursts: one that blinds with a puff of parmesan, one that sticks to what it hits, and a way to choose between them.
   *Tests:* GUT tests of each in `Projectile` and `Match`; browser test of the choice.
2. **Tune the weapons from play.** The host is shown, at the end of each match, what every weapon did: shots, hits, damage and knock-outs. Play matches with people, read those numbers, and adjust each weapon until none is the obvious choice or the one nobody picks up. Repeat after each milestone that adds weapons.
   *Tests:* the weapon tests already hold each weapon to its role; add to them whatever the tuning decides.

## Milestone 3: Movement with depth


## Milestone 4: Characters that are alive


## Milestone 5: An arsenal


## Milestone 6: Ways to play


## Milestone 7: Arenas with character


## Milestone 8: A picture that holds up

The game uses the Compatibility renderer so it runs in a browser. Some effects common in big games (ambient occlusion, volumetric light, screen-space reflections) aren't available there. Items here stay within it; a second, richer renderer for desktop builds is in Milestone 13.

1. **Simpler shapes far away, and nothing drawn behind walls.** Small dressing is already left out far away (`game/world/detail.gd`). Still to do: a simpler shape for far characters and scenery in place of the full one, and leaving out what a wall hides, if occlusion culling can be had in the browser.
   *Tests:* extend `test_detail.gd` and its budgets.

## Milestone 9: Sound that sells it

1. **Sounds from recordings.** The guns, steps and bursts are synthesized in code (`game/audio/sound_bank.gd`), in layers (a crack, a blast, a thump, a tail, the click of the weapon's works) and with several takes of each. Replace them with recorded ones, licensed for use, keeping the layers and takes.
   *Tests:* the tests in `test_sounds.gd` that every take of every cue has a sound, a length and a level. Manual: SND-03.

## Milestone 10: An interface that looks the part

1. **Muting and reporting from the scoreboard.** The scoreboard shows who is talking and who is muted; muting is done from the in-game menu. Let a row be chosen on the board itself to mute them, and to report them, which needs somewhere for a report to go.
   *Tests:* extend `test_scoreboard.gd`.

## Milestone 11: A reason to come back

Everything here needs somewhere to keep what a player has earned. Start with the browser's own storage, which costs nothing and can't be trusted, and move to accounts on a server only when there is something worth protecting.

1. **Accounts.** Sign in to keep a profile across devices, with the server holding it. Needs a small service and a database, a privacy policy, and a way to delete an account.
   *Tests:* tests for the service; browser test of signing in.
2. **Leaderboards.** Best this week and of all time, among friends and everyone, from results the server checked.
   *Tests:* service tests.
3. **Ranked play.** A hidden skill rating from match results, used to match players, with visible ranks and seasons.
   *Tests:* tests of the rating's arithmetic.

## Milestone 12: Online, properly

1. **Play now by connection and skill.** "Play now" joins the fullest session with room in the mode the player would host, or hosts one. Have it also weigh the connection to each host and the players' skill, which the session server would need to know.
   *Tests:* signaling tests of the choice; browser test.
2. **A session browser worth using.** The list sorts by players and arena and filters by mode, arena and room. Left, all needing the session server: ping and region, favourites, and a password for private sessions.
   *Tests:* signaling tests; browser tests.
3. **Parties and friends.** Invite friends, see who is playing, and join a session together and stay together between matches.
   *Tests:* signaling tests; browser test with three windows.
4. **A session that outlives its host.** When the host leaves, hand the match to another player with its state, rather than ending it.
   *Tests:* GUT tests of handing over `MatchState`; a browser test that closes the host's window.
5. **Dedicated servers.** Run a headless build as the host, so no player has the advantage or the burden, started on demand in the region players are in.
   *Tests:* a CI job that plays a match against a headless host.
6. **Movement the host owns.** On dedicated servers, have the host own movement, with the player's own game predicting it. (Being put back by the host already carries the view there gently.)
   *Tests:* extend `test_movement_check.gd`; browser tests under a throttled connection.
7. **Harder to cheat.** The host refuses shots that go where their shooter isn't facing, and speed that many small updates add up to. Left: a check on aim that turns impossibly fast, and on servers, results that are signed.
   *Tests:* GUT tests of each check, with the cheat written as a test.
8. **Reporting.** Report a player with a reason, to a service that keeps reports for someone to act on. (Muting, blocking in every session, and a filter on names and chat are done.)
   *Tests:* service tests; browser test.
9. **A far-off host, warned of.** Warn before joining a host far away, which needs the session server to measure it. (A slow or lossy connection is now warned of in play, and a dropped one is tried again after a short wait.)
   *Tests:* browser tests with a throttled and a cut connection.
10. **Knowing what breaks.** Collect crashes and errors from builds in the wild, with the version and device, and anonymous counts of what players do, with consent.
    *Tests:* tests of what is sent and that nothing is without consent.
11. **More players.** Measure what a browser host can carry and raise the limit beyond 16 where it holds, with interest management so each player is sent only what is near.
    *Tests:* a soak test with bots at the limit.

## Milestone 13: Every device

1. **Desktop builds.** Windows, macOS and Linux builds with full screen, resolutions and windowed modes, published from CI.
   *Tests:* CI builds each and runs the game tests on it.
2. **A richer picture on desktop.** Offer the Forward+ or Mobile renderer on desktop builds, for the lighting and effects the browser can't have, with the Compatibility look as the floor.
   *Tests:* the benchmark scene on both.
3. **A storefront.** Steam: a page, achievements, friends and invites through it, cloud saves, and the Steam Deck checked.
   *Tests:* manual rows.
4. **Gamepads in the menus.** Button pictures that match the controller, and every menu usable without a mouse. (Aim assist that only slows the view, response curves, a dead zone and rumble are done.)
   *Tests:* GUT tests of menu focus on every screen.
5. **A smaller, faster download.** Shrink and compress the web build, load arenas and sounds when needed, and keep a copy for the next visit.
   *Tests:* `browser_tests/test_build_size.py` holds the build to a budget; lower it as the build shrinks.
6. **Threads.** Decide whether the web build should use threads, for a smoother frame rate on a busy host, against the hosting it then needs.
   *Tests:* the benchmark and soak tests either way.

## Milestone 14: For everyone

1. **Every control, any key.** Rebind every action, including the mouse buttons and wheel and every gamepad button, with a second binding for each and a warning on clashes.
   *Tests:* extend `test_preferences.gd`.
2. **An enemy colour.** A colour of one's own for opponents outside teams, as in free-for-all. (Sets for the common kinds of colour blindness, and free choice of each team's colour and of the hit mark's, are done.)
   *Tests:* extend `test_accessibility.gd`.
3. **A screen reader.** Menus read aloud by a screen reader, once the engine can do it in the browser. (High contrast, larger text in menus, and a test that every button shown has a name to read are done.)
   *Tests:* GUT tests that every control has a name to read, extended to its role.
4. **One-handed layouts.** Sets of keys for playing with one hand, chosen in one go. (Pressing once in place of holding, for everything that is held, and holding to keep firing a weapon that fires once a press, are done.)
   *Tests:* GUT tests of the presets.
5. **Other languages.** Move every word on screen into translation files, lay the interface out for longer words and right-to-left scripts, and translate into the first few languages.
   *Tests:* a GUT test that no text on screen is missing from the files, and that every screen fits in every language.

## Milestone 15: Release

1. **Tests that look.** Compare screenshots of each arena, character and screen against kept ones in CI, so a change to the look is seen before it lands.
   *Tests:* this is one.
2. **Tests that last.** A long match of bots on every arena in CI, watching for errors, leaks and frame times; a load test of the signaling server.
   *Tests:* this is one.
3. **Playtests.** A regular session with people outside the project, a form for what they found, and a build channel for them.
4. **The words.** Credits, licences for everything used, a privacy policy, terms, and an age rating.
5. **Telling people.** A logo and key art, a trailer, screenshots, a store page, a press kit and a place for the community.
6. **Release.** A page on itch.io, and the full manual test list run first. (The version on the title screen, and what is new in it in the credits, are done; `game/changes.gd` says what is new, and a test holds it to the version in `project.godot`.)
7. **After release.** A cadence for updates, a public list of known problems, and a way to turn a feature off without a new build if it breaks.

## Later

Ideas worth keeping that aren't planned yet: a co-operative mode against waves of mice, a story for one player, vehicles (the cars in Curd City, driven), a battle royale on a large arena, clans and tournaments with brackets, cross-play accounts between the browser and the store builds, mod support beyond the arena editor, and a replay file for whole matches.
