# Ideas before the Steam launch

A list of what could still make the game feel finished, written on 4 November 2026, before the launch a couple of weeks later. **Priority**: P0 should be in before the launch; P1 the launch window or the first patch; P2 after. **Effort** is agent time: S up to half a day, M about a day, L several days, XL a week or more. What wave 15 is doing (the loadout, the smoke grenade, the UI themes, the grips, rounded cheese on the maps) is left out.

## Steam and launch readiness
| Item | Why | Priority | Effort |
|---|---|---|---|
| A real App ID: wire `UCheeseStrikeRecordsSubsystem::SetOnSteam` achievements and stats, and Rich Presence display strings (the localisation file: "In lobby 3/16", "Playing Execution on Rind Market") | The stub ships otherwise; the presence text is what friends see | P0 | M |
| A Shipping build once the App ID exists (packaging is Development today because Shipping relaunches as app 480) | Development builds are slower and leave the console and cheats in | P0 | S |
| Steam Cloud for `GameUserSettings.ini` and the Records save slot | The loadout, the settings and the achievements follow the player | P1 | S |
| Steam Networking Sockets (SDR relay) for listen-server NAT traversal; check two strangers behind NATs can join | Most "can't join my friend" complaints come from this | P0 | M |
| The store page: capsule art, six or more screenshots at 1080p, a 30–60 s trailer, GIFs, tags, "Full controller support" | Conversion; the user's work | P0 | M |
| A version string in the front end's corner and a patch-notes panel | Bug reports need it; every big menu has it | P1 | S |
| Crash reporting: the engine's CrashReportClient with symbols uploaded (or Sentry); keep the GPU-hang dumps | Players will hit the 4 Nov GPU hang | P1 | M |
| Trade dress: the BabyBomb (red wax and a white strip strongly evoke Babybel, Bel Group) — rename or restyle, or accept the risk; "Craft" is already handled | A takedown at launch is the worst timing | P1 | S |
| A cook audit: drop `Variant_Shooter` once the Proving Ground is rebuilt, prune `DirectoriesToAlwaysCook`, report the package size | A smaller download, faster loads | P1 | S |
| A closed Steam Playtest with 8–16 friends over two sessions, two weeks out | Nothing finds matchmaking bugs like real NATs | P0 | M (user) |

## The first run and onboarding
| Item | Why | Priority | Effort |
|---|---|---|---|
| A splash movie and a first-run hardware benchmark (`RunHardwareBenchmark`) that picks a scalability preset | Stops low-end PCs opening at Epic and stuttering | P0 | M |
| A PSO/shader cache bundled with the build, and a "preparing shaders" step on the first launch | First-match hitches are the first "feels unfinished" tell on UE5 | P0 | M |
| A "How to play" page: cheese is cover, the vermin come at 3:00, streaks, Execution's last stand, the stands | The game's twist needs one screen | P1 | M |
| Loading-screen tips (twenty short lines) on the cover | Free teaching | P1 | S |
| Key and button rebinding (Enhanced Input user settings) | The most-asked-for option in any PC shooter | P1 | L |
| A match intro: a five-second camera sweep of the map with the mode's name and "VERMIN AT 3:00" during the count-in | A cinematic touch every match | P1 | M |

## Sessions and matchmaking
| Item | Why | Priority | Effort |
|---|---|---|---|
| Quick Play (join the fullest open lobby of any mode, else host one) | One-click play is expected | P1 | M |
| The browser: ping, sort and filter by mode, map and players, refresh, password-protected lobbies | Today's list is plain text | P1 | M |
| Lobby text chat, and team chat in a match | A silent lobby feels dead; Steam voice later | P1 | M |
| The host can kick; sides balanced as players join; bots fill in when someone leaves mid-match | Fairness and full matches | P1 | S–M |
| Joining a running match from the friends panel or the Steam overlay | Friends join late all the time | P1 | M |
| After a match: a map vote (three cards) and "play again" without the whole lobby trip | Keeps people between matches | P1 | M |
| A time limit for Deathmatch, free-for-all and King of the Hill (campers can stall a kill-limit match for ever) | A match that never ends is a bug to players | P1 | S |
| AFK detection: a bot stands in, a kick after two minutes | Standard | P2 | S |
| Host migration, or at least "the host left → everyone re-lobbies under a new host" | The listen server's weakest point | P2 | XL / L |
| Reconnecting to a match after a drop | | P2 | L |

## Combat feel
| Item | Why | Priority | Effort |
|---|---|---|---|
| Camera feedback: a sprint FOV kick, a landing dip, a hit shake scaled by damage, explosion shake (the slide's roll exists) | The biggest single multiplier of feel | P1 | M |
| Recoil patterns and visual kick per weapon; a muzzle-flash light; shell ejection; crumbs on cheese and chips on stone where shots land | The guns feel attached to the world | P1 | M |
| A headshot's own marker and sound (the kill already ka-chings) | Instant feedback | P1 | S |
| A flinch on the body others see when hit | Enemies look alive | P1 | M |
| A killer cam: three seconds of who killed you (not a full replay) | Cuts the "where did that come from" | P1 | M |
| Spawns chosen away from enemy sight lines, not only away from the hill | Spawn deaths make people quit | P1 | M |
| A grenade danger indicator in the HUD, and a smoke/frag icon over the ammunition | Standard feedback | P1 | S |
| Multi-kill lines (Double, Triple), "Final kill", "Overtime", "Last of the vermin" | Cheap excitement | P1 | S |
| A vermin meter in the HUD: how much cheese cover is left | Unique to this game; makes the mice a visible clock | P2 | S |
| Mantling and vaulting over low cheese | Movement fluidity | P2 | L |
| Weapon inspect and in-match emotes (the podium's exist) | | P2 | M |

## Audio
| Item | Why | Priority | Effort |
|---|---|---|---|
| In-match music: a low ambient bed, a sting when the vermin come, tension in the last 30 s, victory and defeat stingers | Matches are silent but for the effects | P1 | M |
| Ducking: music and ambience under the announcer and explosions (a sound mix) | Clarity | P1 | S |
| Audio occlusion and reverb volumes per map (the Grotto's cave, the Kitchen's tile) | An enemy behind a wall shouldn't sound beside you | P1 | M |
| Footsteps by surface (cheese, stone, wood), and enemies' footsteps heard at 10–15 m | Reading enemies by sound is core FPS skill | P1 | M |
| An ambience loop per map (drips, the market's murmur, the kitchen's hum) | Places feel inhabited | P1 | S |
| Separate voice and UI volume sliders | | P1 | S |
| Captions for the callouts (an option) | Accessibility, cheap | P1 | S |

## Visual polish
| Item | Why | Priority | Effort |
|---|---|---|---|
| A colour-grading LUT per map and one warm "cheese" look; a bloom and exposure pass on every map | Screenshots sell the game | P1 | S |
| Scalability presets tested on a GTX 1060 / RX 580 class GPU; a "Lumen off" fallback for Low | 4 GB GPUs are common on Steam | P0 | M |
| Crumbs when a block is eaten or falls; the mice's gnawing dust | The core spectacle wants juice | P2 | S |
| Tracers and trails per weapon (the BabyBomb's smoke trail, the Grater's shard glint) | Projectiles read better | P1 | S |
| The front end: a slow camera drift, a background per map, the figure reacting to hover | | P2 | M |
| Hero colour and wax skins unlocked by achievements | Gives the achievements a purpose | P2 | L |

## Progression and retention
| Item | Why | Priority | Effort |
|---|---|---|---|
| An end-of-match stats card: accuracy, best streak, badges, damage, time on the hill | Players screenshot these | P1 | M |
| A STATS page in the menu from the Records (lifetime K/D, badges, achievements with progress bars) | The data is already there | P1 | S |
| A level and XP with a title shown in the lobby and on the scoreboard (local, and a Steam stat) | Even a light meta keeps people playing | P1 | M |
| Steam leaderboards (kills, wins, best streak) | | P2 | M |
| Daily and weekly challenges ("Win with the Cheese Stick") | | P2 | L |

## UI and HUD
| Item | Why | Priority | Effort |
|---|---|---|---|
| Scoreboard: a ping column, assists, badge counts, level | | P1 | S |
| Options: the anti-aliasing method, TSR/FSR upscaling and sharpness, a motion-blur toggle, gamma, HUD scale, crosshair style and colour, hold/toggle for aim, crouch and sprint, camera-shake strength, colourblind team colours | The settings-menu checklist | P1 | M |
| Mouse sensitivity in standard units (cm/360), with separate aim and scope multipliers | Competitive players look first | P2 | S |
| Weapon icons in the kill feed (the HUD's icons exist) and icons for the modes' events | | P1 | S |
| Text ready for localisation (FText everywhere) even if English-only at launch | Retrofitting later is XL | P2 | L |
| A front-end hierarchy pass: a PLAY hub (Quick Play, Host, Browse, Practice), Loadout, Stats, Settings, Credits | Everything found at a glance | P1 | S |

## Maps and modes
| Item | Why | Priority | Effort |
|---|---|---|---|
| A spawn and sight-line tuning pass per map with bots (a heat map of deaths from a soak run) | Data beats guessing | P1 | M |
| Playlists and a map rotation with a mode–map matrix (some maps suit Execution, others the hill) | | P2 | M |
| "Capture the Cheese" (capture the flag with a wheel) | A flagship team mode for the trailer | P2 | L |
| Collision smoothing on cheese edges (the rounded blocks help); an out-of-bounds sweep of every map | Getting stuck means a refund | P1 | S |

## Performance, stability and QA
| Item | Why | Priority | Effort |
|---|---|---|---|
| A 16-player bot soak of 30 minutes on every map: memory, frame time, bandwidth per client; packet-loss simulation (`Net PktLoss`) | Finds the leaks and the replication hogs | P1 | M |
| A startup budget (under 10 s to the menu) and the cover's time per map | The first impression | P1 | S |
| Alt-tab, several monitors, DPI 125–200%, 21:9 menus (the 1280x720 minimum exists) | Window bugs are loud on the forums | P1 | S |
| A bug-report button in the game (zips the logs and a screenshot, opens a prefilled issue or email) | Turns forum rants into tickets | P2 | S |
| Opt-in telemetry: matches, modes, crashes, average match length | Wanted in the first week | P2 | M |

## Accessibility
| Item | Why | Priority | Effort |
|---|---|---|---|
| A colourblind-safe team palette option and a crosshair colour | Orange against white is fine; add a blue | P1 | S |
| Reduced motion (shake, bob and blur off), large HUD text, hold/toggle options, captions | Steam's accessibility tags; cheap goodwill | P1 | S |
| Gamepad: dead-zone and curve sliders, vibration on a hit and a kill | Vibration is missing entirely | P1 | S |

## The five that move the needle most for the least work
A bundled shader cache with a first-run benchmark; camera feedback (the FOV kick, the landing dip, the shakes); in-match music with stingers and ducking; the end-of-match stats card; Quick Play.
