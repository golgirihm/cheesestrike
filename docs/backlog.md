# Backlog

The plan for taking CheeseStrike from where it is now to a game that people can play with each other and that feels as finished as a big-budget shooter: the features players expect of one, and the polish. It is ordered: each milestone builds on the ones before it, and within a milestone the items are roughly in the order they should be done. Each item is sized to be one pull request, or a few.

**Keeping this current:** when a pull request finishes an item, delete the item in the same pull request; the commit history is the record of what was done. Add, reorder or rewrite items whenever the plan changes. Every item follows the testing rules in [CLAUDE.md](../CLAUDE.md); the *Tests* notes below say where an item's tests are likely to go, not that they are the only ones needed.

## Where the game is now

Players can host a session in the browser, find it in the session list or by code or an invite link, and join it over WebRTC, or connect directly in a native build. Up to 16 play free-for-all or in two teams, to a kill limit or a time limit, on three arenas (Cheddar Canyon, Brie Box and Curd City), each fenced at its edge with scenery beyond and a sky with clouds.

Players are wedges of cheese with drawn-on faces and stick limbs, who walk, run and jump with keyboard and mouse, gamepad or touch. They start with a sidearm and pick up four more weapons, one of them a launcher whose wheels burst, and cheese wedges for health. Weapons can be fired from the hip or aimed down their sights. Dealing damage fills a Gouda Overload meter. The host decides every hit (`game/match/match.gd`) and keeps everyone's health and score in a `MatchState` it sends to all. Others' movement is smoothed, shots are checked against where their targets were when fired, impossible movement is put right, and a dropped player can rejoin with their score.

On screen there is a crosshair that shows spread, hit markers, a kill feed, a scoreboard with pings, an in-game menu, a list of controls on a first session, and a settings screen (look sensitivity, field of view, volume, graphics quality, keys, touch layout, text size, captions and screen shake). Sound and music are synthesized in code. On phones the game goes full screen and sideways and can be installed to the home screen.

What it doesn't have yet is most of what makes a shooter feel finished: recoil, crouching, reload animations, more than a handful of weapons and modes, textures, particles, recorded sound, a front end, anything to earn, or a way to find a match with one button. Milestones 2 to 14 are that work, roughly in the order a player would feel it.

Things about how it works that shape the plan:

- **Each player simulates their own movement** (`game/player/player.gd`) and a `MultiplayerSynchronizer` sends position and facing to everyone else. That is fine for movement, but anything that decides who wins, such as hits, damage and score, is decided by the host, or any player could award themselves kills. Rules live in plain classes (`game/combat/`, `game/match/`) so they can be tested without a network.
- **The host is a browser tab**, possibly on a phone. Whatever the host simulates for everyone has to stay cheap, and the session ends when the host leaves.
- **The signaling server only runs on the development PC**, and the game looks for it on the same machine as the page, at port 9080. Nobody outside the local network can play yet.
- **The web export is single-threaded** (`thread_support=false`), which lets it run on any static host without special headers. Keep it that way unless something needs threads badly enough to justify the hosting requirements.
- **The arena editor needs a keyboard and mouse**, and makes flat arenas: a floor of one of three sizes with blocks, walls, crates and pads on a grid (`game/world/arena_code.gd`). An arena is a short code, kept in the player's settings and shared in a link; the host sends it to whoever joins.
- **The light is not baked.** Each arena has a sun, a cool fill light, soft dark edges drawn on the ground round what stands on it, and in some a few lamps: all cheap, and all worked out as the game runs. Real baked lighting needs the editor to bake it and the baked maps to be downloaded.
- **The renderer is Compatibility**, the one that runs in browsers and on phones. It has no ambient occlusion, volumetric light or screen-space reflections, so the look has to come from shapes, colour, baked light and particles.
- **The session list knows two modes and three arenas.** The server that keeps the list accepts only free-for-all and teams, and only the first three arenas, so a session in any newer mode is listed as whichever of those two is nearer, and one on a newer arena, or on one a player laid out, as Cheddar Canyon (`MatchSettings.advertised`). Nor is it told of bots, the host's rules or the sky. Players get all of it once they join. Listing it properly means teaching `signaling/` the rest.
- **There is no server that keeps anything.** Names and settings live in each browser. Anything a player earns and keeps (Milestone 11) needs either that, which can't be trusted, or a service and a database, which have to be run and paid for.
- **Almost everything is made in code**: meshes, sounds, arenas' dressing. That has kept the build small and the look consistent, and it has a ceiling. Recorded sound, textures and animation (Milestones 4, 7 and 9) mean taking on assets, their licences and their size.

## Milestone 1: Playable from anywhere

Before there is much more game, make it possible for people on different networks to reach it, so every later milestone can be playtested with real players.

1. **Host the signaling server publicly.** Run `signaling/server.py` behind TLS (`wss://`) on a small server or a container platform, with automatic restarts and a health check. Add a log line per session hosted and closed so problems can be traced. If a proxy in front of it handles TLS, start the server with `--behind-proxy`, or its per-address limits will count every player as the proxy.
   *Tests:* signaling tests for anything added to the server, such as a health endpoint. Manual: DEV-06 becomes runnable.
2. **Point the web build at the public server.** Replace the same-host-port-9080 guess in `Net._web_signaling_url()` with a URL set at export time, keeping the `?signal=` override and falling back to the current behavior for local development.
   *Tests:* pull the URL choice into a plain function and cover it with a GUT test.
3. **Add a TURN server.** STUN alone fails between players behind strict NATs, which is common on mobile data. Run a TURN server (coturn, or a hosted one) and hand out short-lived credentials from the signaling server rather than putting a fixed secret in the build.
   *Tests:* signaling tests for issuing credentials. Manual: a DEV row for a phone on mobile data joining a PC on home Wi-Fi.
4. **Publish the web build.** A workflow that exports the web build on every push to `main` and deploys it to a static host (GitHub Pages or itch.io), so there is always a current build to share.
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

The game uses the Compatibility renderer so it runs in a browser and on phones. Some effects common in big games (ambient occlusion, volumetric light, screen-space reflections) aren't available there. Items here stay within it; a second, richer renderer for desktop builds is in Milestone 13.

1. **Less detail far away.** Simpler shapes for far things, and nothing drawn that can't be seen, so big arenas hold their frame rate on a phone.
   *Tests:* a benchmark scene with a frame-time budget run in CI.
2. **No hitches.** Compile shaders and build meshes behind a loading screen, so the first shot, burst and pickup don't stutter.
   *Tests:* a browser test that measures the longest frame in a first fight.

## Milestone 9: Sound that sells it

1. **Sounds from recordings.** Replace the synthesized guns, steps and bursts with recorded or designed ones, licensed for use, layered (a close shot, a tail, a mechanical click) and with several takes of each.
   *Tests:* GUT tests that every cue has a sound, a length and a level. Manual: SND-03.
2. **A mix.** Buses for weapons, steps, voices, world and interface, with the music and the world dipping under gunfire and important cues.
   *Tests:* GUT tests of the buses and the ducking.
3. **Space.** Shots and steps sound different indoors, in an alley and in the open; sounds behind a wall are muffled; far shots are a dull crack.
   *Tests:* GUT tests of which treatment a position gets.
4. **Footsteps by surface.** Different steps on asphalt, wood, rock, grass and cheese, and for running, crouching and landing. Opponents' steps are clearer than teammates'.
   *Tests:* GUT test that every surface in every arena has steps.
5. **An announcer.** A voice for the start and end of a match, the lead changing, a minute left, multiple knock-outs and power-ups.
   *Tests:* GUT tests of what triggers each line.
6. **Characters with voices.** Short squeaks and grunts for effort, pain, a taunt and a reload called to teammates.
   *Tests:* GUT tests of the triggers.
7. **Music that follows the match.** A menu theme, and in a match music that builds in the last minute and on match point, and a sting for winning and losing.
   *Tests:* GUT tests of which piece plays when.
8. **The interface makes sound.** Hover, press, back, an error, a countdown.
   *Tests:* GUT test that every button has one.
9. **Sound settings.** A volume for each bus, a dynamic-range choice (headphones, speakers, night), mono, and a hit-sound choice.
   *Tests:* extend `test_settings.gd`.
10. **Talking.** Voice chat between teammates, push-to-talk, with a mute for each player, over the connections the game already has.
    *Tests:* browser test with a fake microphone.

## Milestone 10: An interface that looks the part

1. **A look for the interface.** A theme of its own: a typeface, colours, panel and button styles, icons for weapons and actions, and motion when screens change.
   *Tests:* the small-screen test runs on every screen. Manual rows.
2. **A front end.** A title screen with the logo and a living backdrop, then Play, Customise, Settings and Credits, in place of one panel of buttons.
   *Tests:* browser tests of the paths through it.
3. **Loading.** A progress bar while the game downloads and starts, and a screen with the arena's name, a picture and a tip while joining.
   *Tests:* browser test that the bar moves and ends.
4. **A map.** A small overhead map or a compass with teammates, objectives, pings and the direction of gunfire heard.
   *Tests:* GUT tests of what it shows and hides.
5. **Pings.** Point and press to mark a place, an enemy or a pickup for teammates, with a sound and a marker.
   *Tests:* browser test that a teammate sees one and an opponent doesn't.
6. **Chat.** Text chat to all or to the team, a wheel of quick messages for gamepad and touch, a filter for bad language, and a mute for each player.
   *Tests:* GUT tests of the filter; browser test.
7. **Medals.** Call out a double knock-out, a streak, a long shot, a comeback, revenge, with an icon and a sound.
   *Tests:* GUT tests of each rule.
8. **The end of a match.** A screen with the podium, each player's numbers (accuracy, damage, best streak, favourite weapon), medals earned and what was unlocked.
   *Tests:* GUT tests of the numbers; browser test.
9. **A fuller scoreboard.** Assists, damage, objective score, ping as bars, who is talking or muted, and a way to mute or report from it.
   *Tests:* extend `test_scoreboard.gd`.
10. **Your crosshair.** Shape, size, gap, thickness, colour, outline and a centre dot, with a code to share.
    *Tests:* GUT tests of the settings and the code.
11. **The HUD your way.** Move and scale each part, set its opacity, turn parts off, and a minimal preset.
    *Tests:* GUT tests that no layout puts anything off the screen.
12. **Choosing weapons quickly.** A wheel for gamepad and touch, and icons with numbers for the keyboard.
    *Tests:* GUT tests of the wheel's choice from a stick direction.
13. **Numbers for the curious.** A frame-rate counter, ping, packet loss and a small graph, each a setting.
    *Tests:* GUT tests of the readings.
14. **A photo mode.** Free the camera, hide the HUD, and save a picture.
    *Tests:* browser test that a picture is saved.

## Milestone 11: A reason to come back

Everything here needs somewhere to keep what a player has earned. Start with the browser's own storage, which costs nothing and can't be trusted, and move to accounts on a server only when there is something worth protecting.

1. **Who you are.** A profile: a name, a chosen cheese, a title, kept between visits and shown to others.
   *Tests:* GUT tests of saving and loading; browser test.
2. **Your record.** Matches played, knock-outs, accuracy, time with each weapon, best streak, kept and shown on a page of their own.
   *Tests:* GUT tests of the tallies.
3. **Levels.** Experience for playing, winning and medals, with a level shown by a player's name.
   *Tests:* GUT tests of the rule.
4. **Things to earn.** Cheeses, hats, weapon finishes, emotes and titles unlocked by level and by challenges. Never anything that changes a fight.
   *Tests:* GUT test that nothing unlockable changes a weapon's or a body's numbers.
5. **Challenges.** A few each day and each week (knock-outs with the shotgun, a win on Curd City), and long ones for each weapon.
   *Tests:* GUT tests of progress.
6. **Achievements.** One-off feats with a picture and a line each.
   *Tests:* GUT tests of each trigger.
7. **Accounts.** Sign in to keep a profile across devices, with the server holding it. Needs a small service and a database, a privacy policy, and a way to delete an account.
   *Tests:* tests for the service; browser test of signing in.
8. **Leaderboards.** Best this week and of all time, among friends and everyone, from results the server checked.
   *Tests:* service tests.
9. **Ranked play.** A hidden skill rating from match results, used to match players, with visible ranks and seasons.
   *Tests:* tests of the rating's arithmetic.
10. **Past matches.** A list of recent matches with their results and numbers.
    *Tests:* GUT tests of what is kept.

## Milestone 12: Online, properly

1. **Play now.** One button that finds a session with room, a good connection and players of similar skill, or hosts one.
   *Tests:* signaling tests of the choice; browser test.
2. **A session browser worth using.** Sort and filter by mode, arena, players, ping and region; favourites; a password for private sessions.
   *Tests:* signaling tests; browser tests.
3. **Parties and friends.** Invite friends, see who is playing, and join a session together and stay together between matches.
   *Tests:* signaling tests; browser test with three windows.
4. **A session that outlives its host.** When the host leaves, hand the match to another player with its state, rather than ending it.
   *Tests:* GUT tests of handing over `MatchState`; a browser test that closes the host's window.
5. **Dedicated servers.** Run a headless build as the host, so no player has the advantage or the burden, started on demand in the region players are in.
   *Tests:* a CI job that plays a match against a headless host.
6. **Movement you can trust, and feel.** Predict the player's own movement and correct it gently when the host disagrees, in place of snapping back, and have the host own movement on dedicated servers.
   *Tests:* extend `test_movement_check.gd`; browser tests under a throttled connection.
7. **Harder to cheat.** The host checks aim that turns impossibly fast, shots through walls it can't see through, and rates of everything; sessions can remove a player; on servers, results are signed. The check on speed needs tightening first: its slack for bunched updates lets a player go several times their top speed as long as their updates keep coming close together.
   *Tests:* GUT tests of each check, with the cheat written as a test.
8. **Reporting and moderation.** Report a player with a reason, mute and block, and a filter on names and chat.
   *Tests:* service tests; browser test.
9. **A bad connection, handled.** Icons for high ping and packet loss, a clear message and a short wait to reconnect on a drop, and a warning before joining a far-off host.
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
4. **Phones, properly.** Android and iOS builds, or the installed web app made good enough: a touch layout that can be rearranged, haptics, a battery-saving mode and a frame-rate cap.
   *Tests:* extend `test_touch_controls.gd`. Manual rows on real phones.
5. **Gamepads, properly.** Aim assist that slows the view over a target and nothing more, response curves, dead zones, rumble, button pictures that match the controller, and every menu usable without a mouse.
   *Tests:* GUT tests of the assist's rule and of menu focus on every screen.
6. **Gyro aim.** Aim by tilting a phone or a controller that can.
   *Tests:* GUT tests of the mapping.
7. **A smaller, faster download.** Shrink and compress the web build, load arenas and sounds when needed, and keep a copy for the next visit.
   *Tests:* a size budget in CI.
8. **Threads.** Decide whether the web build should use threads, for a smoother frame rate on a busy host, against the hosting it then needs.
   *Tests:* the benchmark and soak tests either way.

## Milestone 14: For everyone

1. **Every control, any key.** Rebind every action, including the mouse buttons and wheel and every gamepad button, with a second binding for each and a warning on clashes.
   *Tests:* extend `test_preferences.gd`.
2. **Sensitivity in full.** Separate sensitivity when aiming and for each zoom, separate across and up, acceleration off, and a way to match another game's.
   *Tests:* GUT tests of each.
3. **Colour.** Presets for the common kinds of colour blindness and free choice of the enemy, team and hit colours.
   *Tests:* extend `test_accessibility.gd`.
4. **Less motion.** One switch that calms bob, sway, shake, flashes and the field-of-view changes, and a warning before anything that flashes.
   *Tests:* GUT tests that each is at rest with it on.
5. **Hearing.** Captions with speaker, direction and distance, a ring on the HUD that shows where sounds come from, and mono sound.
   *Tests:* extend the caption tests.
6. **Seeing.** A high-contrast mode, larger text everywhere including menus, and menus read aloud by a screen reader.
   *Tests:* GUT tests that every control has a name to read.
7. **Hands.** Hold or toggle for everything, one-handed layouts, and nothing that needs keys mashed.
   *Tests:* GUT tests of the presets.
8. **Other languages.** Move every word on screen into translation files, lay the interface out for longer words and right-to-left scripts, and translate into the first few languages.
   *Tests:* a GUT test that no text on screen is missing from the files, and that every screen fits in every language.

## Milestone 15: Release

1. **Tests that look.** Compare screenshots of each arena, character and screen against kept ones in CI, so a change to the look is seen before it lands.
   *Tests:* this is one.
2. **Tests that last.** A long match of bots on every arena in CI, watching for errors, leaks and frame times; a load test of the signaling server.
   *Tests:* this is one.
3. **Playtests.** A regular session with people outside the project, a form for what they found, and a build channel for them.
4. **The words.** Credits, licences for everything used, a privacy policy, terms, and an age rating.
5. **Telling people.** A logo and key art, a trailer, screenshots, a store page, a press kit and a place for the community.
6. **Release.** Version number on the menu, a changelog shown in the game, a page on itch.io, and the full manual test list run first.
7. **After release.** A cadence for updates, a public list of known problems, and a way to turn a feature off without a new build if it breaks.

## Later

Ideas worth keeping that aren't planned yet: a co-operative mode against waves of mice, a story for one player, vehicles (the cars in Curd City, driven), a battle royale on a large arena, clans and tournaments with brackets, cross-play accounts between the browser and the store builds, mod support beyond the arena editor, and a replay file for whole matches.
