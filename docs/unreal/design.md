# CheeseStrike: the design, for the Unreal port

Everything the Godot version of CheeseStrike does, written down so that the Unreal Engine version can be built from this document rather than from the Godot code. It is the specification; the Godot source at the `godot-final` tag is the reference where this is not enough, and `game/test/unit/` there is the rules written as 23,000 lines of tests.

Numbers are the Godot game's, which play well, unless a line says otherwise. The one thing that changes everywhere is scale: see the first section.

## The pitch, and what changes in the move

A multiplayer first-person shooter, up to sixteen players, with the handling and pace of an arena shooter and one idea of its own: the cover is cheese, and a minute into every match giant mice come over the edge of the map and start eating it, so that a map that began full of cover has less and less of it, and nowhere stays safe. Dealing damage charges an Overload, a short power of the player's choosing. Matches are quick, modes are many, bots fill empty seats, and there is a practice range, a tutorial, a level to earn and things to unlock.

The move to Unreal is a move in look and ambition, not in design. The game is to stand next to other shooters on Steam, which means a realistic, grounded picture: real materials, real light, real weapon and soldier models, a sober interface. The cheese stays as what it is, cover that can be eaten, but drawn as real cheese in a real place rather than a yellow cartoon block: a dairy, a warehouse, a market, a town built around cheese. Names can stay playful where the picture is serious. Decisions still open are listed at the end.

## Scale

The Godot game is built around a player 5.4 m tall, because its world was three times life size. Unreal works in centimetres with a 180 cm player, and all its assets, templates and defaults assume life size. So:

- **Lengths divide by three.** A Godot metre is 33.3 Unreal centimetres. A 48 m arena becomes 16 m; Curd City's 640 by 400 becomes 213 by 133 m. Weapon ranges and falloff distances divide by three too.
- **Times stay.** Fire intervals, reload times, respawn delays, fuse lengths, mode timers.
- **Speeds divide by three** to keep the feel, and then get checked against the arena: 6 m/s walking becomes 2 m/s, which is the same number of body-heights a second and crosses the same arena in the same time, but is slow beside other shooters. The right answer is to keep the ratios (time to cross an arena, time to kill, time to turn a corner) and tune the raw speed up with the arena sizes until the first map plays like a shooter at life size. Start the spike at Unreal's defaults (walk 4.5 m/s, sprint 6.5 m/s) with the arena scaled by three, and adjust the arena, not the character, if it feels wrong.

Where this document gives a length in metres, it is a Godot metre unless marked "life size".

## The player

**Body.** A capsule 1.8 m tall and 0.8 m across (life size: the usual Unreal capsule). Eyes at 0.9 of the height. What shots hit is the head, torso and legs, as boxes or the skeleton's bones; the head takes the weapon's head multiplier. Crouching drops the eyes by 0.27 of the height; leaning tilts the body by 0.3 rad and moves the eyes to the side, never past the body's own width. Looking up or down does not move what can be hit.

**Movement.** The table is the Godot game's; see the scale rule.

| Thing | Value |
|---|---|
| Walk | 6.0 m/s |
| Sprint | 1.5 × walk; cannot fire while sprinting without lowering the weapon |
| Aiming down sights | 0.6 × walk |
| Crouched | 0.5 × walk, crouch at 8 heights/s |
| Jump | 7.8 m/s upward; one jump; air steering at 14 m/s² |
| Step | up to 0.6 m is walked over; a ledge up to 2.5 m is climbed (mantled) in 0.4 s |
| Slide | from a sprint, crouching: 0.9 s, 1.15 × speed at first, usable from 1.2 × walk, a 0.4 s rest after |
| Fall damage | none up to 14 m/s of fall; 8 damage per m/s over that; a hard landing (over 0.7 of the safe speed) slows for 0.5 s |
| Flinch | being hit slows to 0.4 of speed for 0.7 s |
| Lean | left and right, held or toggled |
| Ladders | climbed at 5 m/s; jump pads throw along a set arc |

**Health and armour.** 100 health, up to 50 armour from a rind pickup; armour takes half of each hit until it is gone. No regeneration: health comes from cheese wedges (50 each). Killed, a player waits the respawn delay (3 s; the "frantic" rules make it 1, and a host can set 0.5 to 15) and comes back at the starting spot furthest from enemies on their own side, invulnerable and translucent for 3 s or until they fire, with only the pistol. They keep half their Overload meter.

**Loadout.** A sidearm (Curd Pistol, Twin Curds or Dual Burratas), a knife, two grenades, and up to four found weapons, with the launcher in a "power" slot of its own. A weapon dropped by the killed lies there 20 s, at most twelve about at once. Weapons take attachments, chosen in the loadout: a sight (0.8 × zoom, 0.8 × aimed spread, 1.15 × hip spread), a grip (0.75 × recoil, 0.95 × speed), or a long magazine (1.3 × rounds, 1.2 × reload). Alternate fire on some weapons (below).

**Controls.** Keyboard and mouse or gamepad, rebindable: move, look, jump, sprint, crouch, lean left and right, fire, alt fire, aim, reload, melee, grenade, Overload, weapon next and previous and a weapon wheel, interact (doors), ping, chat and team chat and quick chat, push to talk, emote, scoreboard, photo mode, pause. Toggle or hold for sprint, crouch, aim, lean, talk and scoreboard. Mouse sensitivity, separate aim and scope sensitivity, vertical ratio, invert, FOV 75 by default, stick dead zone and curve, aim assist and rumble on gamepad, and a "match the feel of" setting that converts sensitivity from Source, and other common games. Gyro and touch controls are not carried over.

## Weapons

All hitscan unless a projectile is given. Damage falls off linearly from `falloff start` to `falloff end` down to `floor` × damage. Spread is a cone; `bloom` is the number of shots in a row over which spread grows to its full value; recoil kicks the view up by `up` degrees per shot (to a cap) and sideways by up to `side`, settling in `reset` seconds. Aiming down sights uses the aimed spread, 0.6 × the recoil, and `zoom` as the FOV multiplier. `TTK` is the time to kill a full-health player with body shots, the tuning target.

| Weapon | Slot | Damage | Head × | Interval | Mag | Reload | Range | Spread aim / hip | Falloff start–end (floor) | Bloom | Recoil up / cap / side / reset | Zoom | TTK |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Curd Pistol | side | 12 | 1.5 | 0.25 | 10 | 1.2 | 90 | 0.6 / 7 | 25–60 (0.5) | 4 | 0.9 / 5 / 0.2 / 0.3 | 0.75 | 2.0 s |
| Twin Curds (two pistols, automatic) | side | 10 | 1.4 | 0.13 | 20 | 1.9 | 60 | 2.5 / 9 | 12–35 (0.4) | 8 | 0.5 / 8 / 0.5 / 0.3 | 0.9 | 1.2 s |
| Dual Burratas (two-round bursts) | side | 14 | 1.5 | 0.4, burst gap 0.06 | 24 | 2.2 | 80 | 1.4 / 7.5 | 18–45 (0.5) | 6 | 0.8 / 7 / 0.35 / 0.32 | 0.85 | 1.3 s |
| Cheddar Blaster (automatic rifle) | main | 20 | 1.75 | 0.12 | 24 | 1.6 | 150 | 0.3 / 9 | 40–90 (0.6) | 8 | 0.45 / 9 / 0.35 / 0.3 | 0.6 | 0.5 s |
| Blaster, charged alt fire | | 55 | 1.75 | 0.8 | 6 charged shots per magazine | | | 0.1 / 6 | | 1 | 2.5 / 2 / 0.3 / 0.8 | | |
| Colby Carbine (three-round bursts) | main | 18 | 1.6 | 0.48, burst gap 0.07 | 27 | 1.7 | 140 | 0.25 / 7 | 35–80 (0.6) | 6 | 0.45 / 6 / 0.2 / 0.3 | 0.6 | 0.6 s |
| Parmesan Grater (SMG) | main | 12 | 1.5 | 0.065 | 45 | 1.5 | 80 | 2.5 / 12 | 15–45 (0.4) | 12 | 0.28 / 12 / 0.5 / 0.25 | 0.75 | 0.5 s |
| Munster Machine Gun (LMG) | main | 16 | 1.5 | 0.085 | 75 | 3.6 | 120 | 1.2 / 13 | 25–70 (0.5) | 14 | 0.4 / 14 / 0.6 / 0.3 | 0.75 | 0.5 s; 0.9 s to draw, 0.85 × speed carrying |
| Swiss Shotgun (9 pellets) | main | 10 each | 1.5 | 0.85 | 6 | 2.2 | 40 | 14 / 32 | 8–25 (0.3) | 1 | 3 / 2 / 0.6 / 1.0 | 0.8 | two shots close |
| Shotgun, both barrels alt fire | | 10 × 18 | | 1.4 | | | | 18 / 36 | | | 5 / 2 / 1 / 1.4 | | |
| Roquefort Rifle (sniper, scoped) | main | 75 | 2.0 | 1.3 | 4 | 2.6 | 250 | 0 / 10 | none | 1 | 2.5 / 2 / 0.3 / 0.5 | 0.3 | one head shot; 0.5 s to draw; a visible lens glint when scoped |
| Breadstick Bow (projectile) | main | 80 on impact | 1.75 | 0.9 | 1 | 0.8 | | 0 / 6 | | | | 0.7 | arrow at 55 m/s, gravity 0.35 ×, 4 s life |
| Gouda Launcher (projectile) | power | 240 splash, 5 m radius, 9 knockback | | 0.9 | 2 | 2.4 | | 0 / 7 | | | 2 / 1 / 0 / 1 | 0.8 | wheel at 24 m/s, gravity 0.25 ×, bursts on touch or after 3 s |
| Launcher, bouncing alt fire | | same | | | | | | | | | | | 20 m/s, gravity 0.6 ×, bounces (0.55), bursts after 2.2 s |
| Wax Grenade (thrown) | thrown | 160 splash, 5.5 m, 8 knockback | | 0.8 | 2 carried | | | | | | | | 19 m/s, gravity 0.8 ×, bounces (0.42), fuse 2.6 s |
| Cheese Knife (melee) | | 55; 500 from behind | | 0.75 | | | 4.5 m reach, 16° sweep | | | | | | lunge 7 m/s, 0.25 s swing |

Reserves: each weapon carries two or three magazines' worth and holds at most about five; an ammunition crate refills a magazine's worth of every carried weapon. Shots penetrate up to 0.5 m of a wall for 0.6 × damage. Falloff and spread numbers are Godot metres and degrees.

**What a shot does on screen:** a muzzle flash, a tracer streak to where it ended, a casing, a little smoke; a mark on the surface and a puff in its colour (sparks off metal); a hit marker on the crosshair, different in shape and colour for a hit, a head hit and a kill; a damage number drifting up; a red arc at the edge of the view pointing at whoever hurt you; red edges as health falls; a blur when a burst goes off close. Bursts have a flash, a shockwave ring to exactly where damage stops, debris, a scorch and lingering smoke.

**Weapon finishes** (cosmetic): plain, red wax, blue vein, gold.

## Damage and fairness

The server decides every hit. A client fires at once, shows the shot, and sends the server the shot's origin and direction; the server checks the shot could have been fired (the weapon had a round, the interval had passed, the origin is within 2.5 m of where the server has the player, with 0.75 s of allowance around a reload), rewinds the targets to where they were at the shooter's time (lag compensation, kept to a short history), traces, and applies the damage, with head multiplier, falloff, armour, penetration, power-ups and Overload. Pellets are spread from a seed both sides share. Splash damage is checked for line of sight. Movement is client-predicted and server-checked; impossible movement is corrected. In Unreal: the Character Movement Component's prediction and the server-authoritative actor model do this; what has to be written is the shot validation, the rewind, and the damage rules, as Lyra's weapon and gameplay-ability code shows.

Friendly fire is off unless the rules say otherwise. An assist goes to anyone else who hurt the victim in the 10 s before a kill.

## Modes

Up to 16 players, 2 at least (bots fill in, with cheese names of their own: Gorgon Zola, Camem Bert and company). The two sides are The Gouda Guys and the Mozza Radicals; a newcomer joins the smaller, and the lobby shows both columns filled out with the bots that will stand in. The host picks a mode, a kill limit (5, 10, 20, 30, 50) or point limit, minutes (3, 5, 8, 10, 15), a map, bots and their skill, a sky, and a rules preset. A match has a 5 s countdown, plays to its limit or its time, shows the final standings and a highlight clip, and the lobby votes on the next map.

| Mode | Teams | Wins by | Notes |
|---|---|---|---|
| Free-for-all | no | kills | |
| Deathmatch (the Godot game's Cheddar vs Brie) | two | kills | the first team to the kill limit (50 unless the host says) |
| Capture the wheel | two | 3 captures | each base has a wheel of cheese; carry the enemy's to yours; a dropped wheel lies where it fell, returns after a while |
| Hold the hill | two | 100 points | one zone that moves between five spots; a second a point while one team alone is in it |
| Points | two | 150 points | three zones (A, B, C) held at once; each held zone scores over time |
| Elimination (the Godot game's Rounds) | two | 4 rounds | one life a round; a round ends when a side is gone or on its timer (2 minutes; at the time, the side with more standing takes it, then the one with more health, else a draw); the out watch a teammate; a 6 s pause, then everyone is back at their starts |
| Execution | two | rounds or kills | damaged enough, a player drops into a last stand with only the sidearm; only an enemy up close can finish them, and a teammate can revive them (the user's, October 2026; to come with the weapons' inventory) |
| Gun game | no | every weapon | each kill moves a player to the next weapon in the list; the last is the knife (as Counter-Strike's, simplified; to come with the weapons) |
| Practice | alone | | the practice range: targets at marked distances, every weapon on a rack, a stats board for each weapon |

Rules presets: standard; instant kill (1 health); low gravity (0.4 ×); launchers only; pistols only; fast and frantic (1.25 × speed, 1 s respawn); tough cheese (200 health); friendly fire. Skies: clear noon, dawn, dusk, night, rain, dust storm, or a surprise.

Also: a session can be private (joined by code only); a player can join to watch, or switch to watching, following any player or flying free, and the killed watch a teammate or their killer until they return; a vote can remove a player (three voters at least, 30 s); a dropped player can rejoin with their score; the host's departure ends the session (in Unreal, host migration or a dedicated server can change this).

## Arenas

Each arena has starting spots for 16 (the first two facing each other across a clear line), pickup pads with a kind each, places with names (shown on the HUD and in the kill feed, as "Nice shot · The Fountain"), objectives for the team modes, jump pads and ladders where it has them, doors that open as anyone comes up to them (no key, no button), and a menu camera. [maps/principles.md](maps/principles.md) is how the maps are designed and built, and lists the maps to build next. Sizes are Godot metres; divide by three.

| Arena | Size | Setting | Character |
|---|---|---|---|
| Cheddar Canyon | 48 × 48 | desert canyon | four mesas with lifts up to them, rock ledges, an arch, blocks of cheese |
| Brie Box | 32 × 32 | meadow | small and tight; pillars, a stack of blocks, a jump pad across it |
| Curd City | 640 × 400 | town | the large map: a yard at each end, three avenues and five cross streets, two- and three-storey houses of cheese to walk into with ramps between floors and ladders to roofs, warehouses, stone towers to find one's way by, traffic lights, steam from grates, a train beyond the fence |
| Double Cream Dairy | 44 × 36 | dairy | steel tanks, a gantry with a lift, a cold store with a door, a fondue vat and a grater as hazards (hurt every half second; staying in one is a kill) |
| Crumb Kitchen | 52 × 36 | giant kitchen | a worktop, a chopping board, a toaster, a kettle, a hob, a spice rack |
| Rind Market | 56 × 56 | city square | stalls with awnings, a fountain, a clock tower, a cart, cobbles, lamps at night |
| Gruyère Grotto | 48 × 44 | cave | always lamp-lit; pillars, a hollow, a ledge |
| Cracker Duel | 36 × 24 | meadow | small, for two to eight |
| Practice range | 48 × 156 | meadow | targets at 10, 25, 50 m and more, the weapon rack |
| The Creamery | 390 × 300 | cheese works | the first built in Unreal (docs/unreal/maps/creamery.md): docks at each end, a two-storey house of cheese on each plaza, a loading yard, an aging hall of cheese racks with catwalks across it, tunnels of cheese along it, a sunken canal; houses, tunnels, a wall of wheels, sealed doors, racks and a cheese bridge that the mice turn from a closed map into an open one |

Plus arenas players lay out themselves in an editor: a square floor of one of three sizes with blocks (stack two high), walls, crates, pickup pads and jump pads on a grid, encoded as a short code that can be shared in a link; starts and objectives are worked out from the layout. Worth keeping, as Unreal's runtime spawning makes it easy; the editor's interface is a later item.

Lighting and weather are per session: a sun or moon at a height and bearing, ambient, haze, rain streaks or dust motes round the player, lamps that come on under dark skies, cloud shadows drifting under sun. Birds, flags, a train: dressing that moves and matters to nothing.

## The mice

The host runs them; everyone sees the same. One minute into a match, after a red countdown, all the mice come in together over the edge. How many: two on a small arena, up to eight, one for every sixty blocks of cheese. A mouse is 12 m long and 6 m high (Godot metres: about twice a player's height, which at life size makes them giant rats the size of a car). Each goes for the nearest cheese low down (height costs 4 m of distance per metre up; cheese another mouse has costs 40 m), walks at 8 m/s, climbs over anything under 3 m and goes round the rest, eats a block bite by bite (40 of a block's 400, every half second: a block lasts five seconds of one mouse, and is drawn in three stages of being eaten) and moves on. Blocks are eaten from the bottom; a house of cheese loses its walls from the ground up. A mouse hurt by 300 (wearing off at 20 a second) runs at 18 m/s for 6 s and comes back; a mouse that makes no progress for 8 s gives up that cheese for 30 s. Mice hurt nobody and block nothing. They gnaw and squeak, and bob as they walk.

The tuning is a guess that play has not yet settled: cover should be plainly going by the middle of a match and mostly gone by its end, without the last minutes being a plain with nowhere to stand.

## Pickups, power-ups and the Overload

**Pads** hold a cheese wedge (50 health; only for the hurt; back after 15 s), an ammunition crate (12 s), a rind (50 armour; 25 s), a weapon (20 s), or the power-up (one pad per arena, first at 30 s, back after 60 s). Reach is 1.3 m across and 2 m up. A pickup lying ready glows softly, visible across the arena but not through walls.

**Power-ups** go round in turn, so nobody knows what the pad holds: double damage (12 s), speed (1.3 ×, 15 s), shield (hits do 0.4 ×, 12 s), sight (everyone seen through walls, 10 s). Taking one is announced to all; the holder is tinted in its colour.

**Kill streaks** (the user's, October 2026, in place of the Overload meter below): kills in a row without dying earn, at 3, the Unmanned Aerial Cheese (a cheese drone circles the map for 25 s, and every 2 s its sweep shows the earner's side every enemy); at 5, the Cheese Strike (the player marks a spot with a shot; after a falling whistle, seven gouda wheels land 3 m apart along the way they faced, each 150 splash damage falling to 40 at 5 m life size, checked for line of sight); at 10, the Gouda Overload (a siren and a warning to everyone for 4 s, then for 8 s a wheel falls every 0.7 s on every enemy, landing within 2 m life size of where they were, 70 falling to 10 over 4.5 m: cover overhead or a wall saves them, and so does keeping on the move). Earned streaks wait, oldest first, until the Streak key (4) calls the next in; they are kept through a death. A streak's kills are the caller's but don't count toward the next streak, and never hurt the caller or their side. Bots call theirs in. Everyone is told who called what.

**Overload** (the Godot game's; kill streaks above take its place). Damage dealt charges a meter at 0.5 per point: two kills fill it. Full, the player sets off the kind they chose in the loadout, after a 1 s build-up in which they glow and are heard, so that others can get clear:

- **Melt** (8 s): 1.35 × speed, and anyone they hit is slowed to 0.6 × for 2 s.
- **Rind** (8 s): hits on them do half.
- **Rain of wheels** (3 s): eight Gouda wheels fall one after another in a ring 5 to 9 m out, from 14 m up, each 70 splash damage over 3.5 m, never on the player.

None adds damage to the player's own guns. Killed, half the meter is kept.

## Bots

Fill the host's chosen seats (1, 3, 5 or 7), named for cheeses, at a skill. A bot takes a moment to notice an enemy, aims with an error that steadies over 1.5 s of tracking, fires in bursts, sidesteps while firing, goes for a wedge when under 45 health, otherwise heads for whatever the mode is about or wanders between route points; it backs off closer than 6 m, fights from about 0.45 of its weapon's range, and finds a way round when stuck for 1.2 s.

| Skill | Reaction | Aim error at first | Steadies to | Burst | Rest | Turn |
|---|---|---|---|---|---|---|
| Easy | 0.75 s | 7° | 0.55 × | 3 | 0.9 s | 3 rad/s |
| Normal | 0.45 s | 4° | 0.4 × | 5 | 0.55 s | 5 rad/s |
| Hard | 0.25 s | 2° | 0.3 × | 8 | 0.3 s | 8 rad/s |

Routes: hand-placed points and links (street corners, doorways) on the large map; Unreal's navmesh replaces them, with the same behaviour on top.

## Networking and sessions

**Godot:** a browser-hosted peer-to-peer session over WebRTC with a signaling server on AWS; the host is authoritative, sends the whole match state ten times a second, and everything else is RPCs. **Unreal:** the standard client-server model with a listen server (the host plays) now and dedicated servers later; replication of actors and properties, with the match state as a replicated game state; sessions, invites, the friends list and voice through Steam's Online Subsystem (or Epic Online Services, which also gives crossplay-free voice and stats). The session list, codes, private sessions, "play now" (join the fullest open session in the mode you would host, else host) and a rejoin with score are features to rebuild on top of Steam sessions. Keep the host's say over everything that scores, and the rule that the host simulates only what is cheap.

Voice: push to talk to teammates (everyone in a free-for-all), mutable per player, off by a setting. Text chat with team chat and eight quick phrases, four messages per six seconds, with a word filter that catches look-alike spellings and spares innocent words.

## Progression

Everything is kept in the player's own save (Steam Cloud later), unchecked.

**Experience** per match: 12 a minute (up to 30 minutes), 20 a kill, 8 an assist, 25 a medal, 120 for a win, 50 for a draw, 30 for finishing. Level 2 at 300, each level 120 more than the last, to 99. A title and level show under a player's name.

**Medals** during play: double and triple kill, a run of 5 and of 10, a long shot (40 m), a head shot, a comeback (after three deaths running), revenge. The announcer calls lead changes, the last minute, victory, defeat and streaks.

**Achievements** (Steam achievements later): First Crumb, Top of the Board, Mouser (100 kills), Well Aged (50 matches), Untouched Rind (win 5–0), Three in a Row, Sharpshooter (60 % of 20+ shots), From the Next County, Back from the Board, No Hard Feelings, Tour of the Dairy (every arena), Every Flavour (every mode), The Whole Board (every weapon), Long Maturing (10 hours).

**Challenges:** three a day (150 XP), two a week (500), one long (1000): kill N, kill N with a weapon, win N on an arena, play N of a mode, earn N medals, deal N damage, N assists.

**Unlocks** by level, achievement or challenge: camouflage patterns (cheese kinds), helmet and head gear, a thing held in the off hand, titles, weapon finishes, emotes (wave, dance, shrug, cheer). The hats, balloons and lollipops of the Godot game are replaced by serious equivalents: camo, helmet covers, patches, charms.

**Records:** past matches, best streaks, time played, per-weapon stats; a profile screen; a match report at the end of each match with what every weapon did, for the host, to tune by.

## Interface

A front end (play, practice, learn, customise, edit an arena, settings, credits), a host screen with the match settings, a session list with filters and a join-by-code, a join screen with tips, a loadout screen, a settings screen in sections, an in-game pause menu, and the combat HUD: crosshair (shape, size, gap, thickness, colour, outline, dot, shareable as a code; opens with spread), health and armour, ammunition and reserve, grenades, the weapon bar, the Overload meter, the power-up timer, a compass with pings and objectives, the place name, hit markers and damage numbers, a damage direction arc, a kill feed with places, medals, captions for sounds, a sound ring (optional) showing where sounds come from, the mice countdown, an objective tracker, the scoreboard with pings and talking indicators, a results screen, a highlight replay and a kill replay, and a photo mode. A tutorial walks a new player through the practice range one step at a time, in the words of the device in their hands.

Accessibility: text size, high contrast, colour-blind presets for team and hit colours (Okabe-Ito), custom colours, less motion, captions, hold-or-toggle for everything held, aim assist, a flashing-lights warning.

The look: in the move, the cheddar-and-rind rounded panels become a thin, square, white-and-grey interface with one accent colour and a condensed typeface; the information stays the same.

## Audio

Every sound in the Godot game is synthesized in code (gunshots in layers: crack, blast, thump, tail, action; footsteps as heel and sole, by surface; bursts; pickups; the interface; the announcer spoken by the device's own speech; music that follows the match, quiet under play, driving when it is nearly decided, a sting at the end). None of it carries to Unreal, where recorded, licensed sound replaces it, keeping the layering, several takes of each cue, and the mix: master, music, effects with weapons, steps, voices, world and interface under it, a dynamic-range choice, and acoustics (sounds further off are duller and reverberate indoors). The music director's logic carries as design.

## Open decisions

1. **How far the cheese theme goes in a realistic world.** The recommendation above: real cheese, real places, giant rats; playful names, serious picture. The alternative is a full rename and a generic military setting, which throws away the one mechanic that is the game's own.
2. **The comic weapons.** Dual Burratas, the Breadstick Bow and the Parmesan Grater are jokes as objects. Keep the stats and the slots, give them real models, and decide the names per weapon.
3. **Listen server or dedicated from the start.** Listen server for the spike; dedicated servers are an Unreal build target and a hosting bill, and matter once strangers play each other.
4. **Steam or Epic Online Services for sessions and voice.** Steam alone is simplest for a Steam-only game.
5. **Gameplay Ability System or plain C++** for Overloads, power-ups and the mice's effects. Decide after the spike.
6. **The arena editor and shared arena codes.** Keep, defer, or drop.
7. **The web build.** Gone. Unreal has no supported browser target.
