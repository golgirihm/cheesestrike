# The Unreal spike

A two-week build in Unreal Engine 5 to settle, with evidence, whether CheeseStrike moves from Godot to Unreal. The decision was taken in principle in October 2026: the goal is a serious first-person shooter on Steam that stands next to other shooters, and the engine comparison is in the conversation that led here. The spike exists so that the full port starts from a working skeleton rather than a blank project, and so that the two weeks are lost rather than two months if Unreal turns out to fight the way we work.

What the spike is not: the game. No modes beyond free-for-all, no bots, no progression, no mice. Those come after, in the order [design.md](design.md) gives.

## What "done" looks like

At the end, all of the following, in one Unreal project on this repository's `main`, with the Godot game kept at the `godot-final` tag:

1. **Two players over Steam.** One hosts as a listen server, the other joins through the Steam friends list or a session search, on the Steam test app id (480). Both move, jump, crouch, sprint and shoot each other with client-side prediction, so the joiner feels no lag on their own movement.
2. **One arena, grey-boxed from the Godot layout.** Cheddar Canyon's floor, mesas, blocks, ramps, pads and starting spots, at real scale (see the scale rule in the design document), with Megascans or Fab textures on the floor, rock and walls, and the sky, sun and post-processing set up for a grounded daylight look.
3. **One weapon with the Godot numbers.** The Cheddar Blaster: hitscan, 20 damage, 1.75 head multiplier, 0.12 s between shots, 24 in the magazine, falloff from 40 to 90 player-heights, bloom over 8 shots, recoil up 0.45 and sideways 0.35 per shot, aim-down-sights at 0.6 zoom. Hits decided on the server with lag compensation. A licensed rifle model with first-person arms.
4. **Knock-outs and respawns.** Health, death, a respawn after a few seconds at the starting spot furthest from enemies, a kill feed and a scoreboard. A match ends at a kill limit.
5. **The workflow holds up.** The project builds from the command line, a test (Unreal Automation) runs headless and is run by a GitHub Actions workflow, and the gameplay code is C++ rather than Blueprint, so that it can be read, edited and reviewed as text.
6. **A screenshot next to the Godot one.** The same spot in Cheddar Canyon, at the same time of day, side by side with the native Forward+ build on the `godot-final` tag.

Then the go/no-go: did building this feel like the way to build the next two years of the game? If yes, the spike project becomes the real repository and the port continues in [design.md](design.md)'s order. If no, the Godot project continues from `godot-final` with the asset list from October.

## Before anything can start: manual steps

These need an account, a password, a credit card or a licence click, so they are yours.

1. **An Epic Games account**, and the Unreal Engine EULA accepted on it. The same account signs in to the Epic Games Launcher and to Fab.
2. **The Epic Games Launcher**, then from its Unreal Engine tab install the current 5.x release (5.8, from June 2026, or whichever is the latest non-preview). Tick "Editor symbols for debugging" off to save 30 GB. Expect about 60 GB for the engine itself.
3. **Visual Studio 2026 Community** with the "Game development with C++" workload, and under its optional parts: the Windows 10 or 11 SDK, "Unreal Engine installer", ".NET desktop development", and the **MSVC v14.50 toolset** picked explicitly in the individual components. Unreal 5.8 supports Visual Studio 2026 (18.0 or later) and recommends the 14.50 compiler; the newer 14.51 that a Visual Studio update brings in breaks the engine build (a removed `hash_map` header), so keep 14.50 installed and set as the toolset until Epic says otherwise. Visual Studio 2022 (17.14 or later, compiler 14.44) is the fallback that needs no care. Unreal's own setup page lists the exact boxes; the launcher checks them on first project creation. Community is free for an individual developer whatever the game earns; its revenue clause (over $1M a year, or over 250 PCs) applies to organizations of more than five developers, and even then a Professional seat is about $45 a month, beside Epic's 5 percent. There is no way round Microsoft's toolchain for a Windows build: Clang on Windows uses the same headers, libraries and SDK, and Unreal does not cross-compile Windows from Linux.
4. **Lyra Starter Game**, from the Launcher's Samples tab (it is Epic's and free). Also from Fab, claim while signed in: Epic's free Paragon characters and any current free-for-the-month packs that have weapons, military props or environment sets. Note that most Megascans moved to paid on Fab in 2025; there is still a free tier and Epic's own free samples (City Sample, the Lyra weapons and characters), and buying a texture bundle is cheap if the free ones run out.
5. **Keep the code public and the assets private.** *Decided in October 2026.* The Unreal project is mostly assets, and Fab and Marketplace licences forbid redistributing their files, so its `Content/` folder is the private `cheesestrike-assets` repository, as a git submodule, and this repository stays public with the C++ code and config. A private repository on GitHub's free plan can't have rulesets, so this also keeps `main` protected. The cost is that a change to both code and assets is two commits, one in each repository, with this one moving the submodule to the new assets commit. Both use Git LFS for binary files. The free LFS allowance (1 GB stored, 1 GB a month down, over the account) is gone within a week of an Unreal project, so buy one **LFS data pack** ($5 a month for 50 GB) when GitHub starts refusing pushes; more as it grows.
6. **Steam, later.** A Steamworks partner account ($100 one-time) is only needed for the real app id, store page and achievements. The spike uses Steam's public test app id 480 ("Spacewar"), which needs only the Steam client installed and running on both PCs, signed in to two different accounts (any two accounts that have ever spent money; a second account may need a cheap game bought on it).
7. **Disk and time.** Free 150 GB before starting: engine, Lyra, the project with its derived data, and Visual Studio. First-time shader compilation of Lyra takes an hour on this PC.

What I need from you once those are done: that the Launcher shows the engine installed. Everything after that, I can do from the terminal and the editor's command line, with you in the editor for the things that are only clickable (importing a model the first time, setting up the Steam plugin's project settings page).

## The plan, by day

Days are working days and are a guess; the order matters more than the dates.

**Day 1: the project.** *Done in October 2026.* The Godot tree removed from `main` in one commit, then a C++ project from the First Person template (not Lyra: see below), named for the game, `.gitattributes` for LFS, `.gitignore` for Unreal, the editor's source control off (git is driven from outside), and CLAUDE.md, the README and the workflows rewritten for Unreal. A `Source/` layout with one game module. Confirm it builds from `Build.bat` on the command line and opens. First commit.

**Day 2: the character.** *Done in October 2026; the slide too, and lean and mantling are left for the port.* The template's character replaced with ours: a Character Movement Component tuned to the design document's movement table (walk, sprint, crouch, jump, air steering, slide), the camera at eye height, input through Enhanced Input with the Godot key bindings. Prediction and replication come with the component. A second window joining over the local network (`?listen` and `open 127.0.0.1`) proves it.

**Day 3: the arena.** Cheddar Canyon grey-boxed from `game/world/maps/cheddar_canyon.tscn` at the `godot-final` tag, scaled by the scale rule, as static meshes or geometry brushes: floor, four mesas, the blocks, the ramps, the pads as markers, player starts. A directional light, sky atmosphere, sky light, exponential height fog, a post-process volume with Lumen on. Nothing textured yet.

**Day 4: the weapon.** A weapon actor and a weapon-data asset holding the Godot stats, the Cheddar Blaster filled in. Firing on the client, a server RPC with the shot's origin and direction, server-side line trace with Unreal's lag compensation (the `AGameMode`'s hit-validation hooks or a short rewind of capsule positions, as Lyra does), damage with the head multiplier against a head bone or a head box, falloff, recoil and bloom applied on the client. Hit markers and a crosshair that opens with spread.

**Day 5: knock-outs.** Health as a replicated attribute, death, ragdoll or a fall animation, respawn at the start furthest from enemies after the design's delay, a kill feed and scoreboard widget, a kill limit ending the match and a results screen. Free-for-all only.

**Day 6: Steam.** The Online Subsystem Steam plugin, app id 480, sessions created and found through the subsystem, a join through the Steam overlay's friends list. Two PCs (or one PC and a laptop) over the internet, not the LAN.

**Day 7: models and arms.** A licensed rifle and first-person arms from Fab, the animation blueprint for aiming, firing and reloading. The soldier model for other players, which may be Lyra's or Paragon's for the spike.

**Day 8: materials and sky.** Megascans or Fab textures on the canyon floor and rock, a desert sky, the post-process tuned for a grounded look. The side-by-side screenshot.

**Day 9: tests and CI.** One Automation test of the damage calculation and one of the shot validation, run headless with `UnrealEditor-Cmd.exe -ExecCmds="Automation RunTests ..."`, and a GitHub Actions workflow on a self-hosted runner (this PC) that builds and runs them. A hosted runner can't hold the engine.

**Day 10: the verdict.** Play it for an evening with a second person. Write up what was easy, what was hard, and the decision.

## Source control: Git with LFS now, Perforce if there are ever several of us

Unreal studios use Perforce (Helix Core), and the editor integrates with it: exclusive checkout of a binary asset so two people never edit the same one, which git cannot do well. Helix Core is free for up to five users and twenty workspaces, with no storage limit, but you run the server: on this PC, or a cloud machine at $10 to $40 a month for the disk, with backups your own problem. Past five users it costs hundreds of dollars a user a year. Git with LFS keeps GitHub, pull requests, Actions and the way the work has been reviewed so far, for $5 a month per 50 GB. For one developer, and for two who talk, git wins; Perforce is the move when a second person edits binary assets every day.

When GitHub's LFS bill matters (past about 100 GB), the objects can move to a bucket without touching the repository: git holds only pointer files, and `.lfsconfig` names the server they are fetched from. A small LFS server in front of S3 or GCS (`estranged-lfs` on Lambda and S3, so nothing stays running; or `rudolfs` or `giftless` on a VM) costs about $0.02 a GB-month plus egress, a fifth of GitHub's packs. The migration is: fetch every object, change the endpoint, push every object once. What is lost is GitHub's file locking and its zero setup: collaborators and CI need the bucket's credentials, which is why the CI runner is this PC. Azure DevOps is not an option: its repositories are capped at 250 GB with LFS counted in.

## Why the First Person template and not Lyra

Lyra is the better shooter and the worse starting point. It is Epic's complete multiplayer shooter: the Gameplay Ability System for everything a player does, a modular game-feature architecture, dedicated server builds, cosmetics, a settings screen, ranked team modes. It is also 80,000 lines of C++ and Blueprints across plugins, with a learning curve measured in weeks before a change lands where you expect. Starting the game inside it means inheriting all of it.

The plan is: build the spike on the First Person template, which is a character, a gun and a room, so that every system in the project is one we wrote and understand; and read Lyra as the reference for how Epic solves each problem (its weapon data, its lag-compensated traces, its session and team setup), taking its solutions one at a time as they are needed. If after the spike the Gameplay Ability System looks like the right fit for Overloads, power-ups and the mice, it is a plugin that can be added to our project; it does not have to come with Lyra around it.

## What carries over from Godot, and how

Nothing carries over as code. What carries over is the design, which [design.md](design.md) writes down in full, and three things in particular:

- **The numbers.** Every weapon's stats, the movement constants, the mode rules, the mice, Overload and power-up timings, and the progression tables are tuning that took play to find. They go into Unreal data assets as they are, scaled by the scale rule where they are lengths.
- **The arenas' layouts.** The `.tscn` files under `game/world/maps/` at the `godot-final` tag are plain lists of boxes with positions and sizes; `curd_city.gd` is a plan that builds a town. A script can read either and emit a level's actors. The dressing (holes in cheese, windows, fences) does not carry: it was a stand-in for real models.
- **The tests' intent.** `game/test/unit/` at the `godot-final` tag is 23,000 lines of rules written down as tests. They are the specification of what the game does. Each system ported gets its rules re-expressed as Automation tests, reading the Godot test for what to assert.

The signaling server, the browser tests, the WebRTC transport and the AWS stack do not come: Unreal's sessions over Steam replace all of them, and dedicated servers, if ever wanted, are an Unreal build target rather than a Lambda.
