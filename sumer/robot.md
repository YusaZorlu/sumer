# robot.md: notes for the next AI (or person) working on Sumer

Read this first. `README.md` describes the game for players; this file is about *working on* it: how to
build and test, how the code is organised, what to update with every change, and the traps already hit.

## The project in one paragraph

An open-world prototype of Ur, about 2100 BC: C99 and raylib 5.5, everything drawn from primitive shapes,
about 17,000 lines in `src/`. It runs as a desktop program (Linux, macOS) and in the browser (Emscripten),
where up to six people share one world: one player's browser is the **host** and runs the whole world,
`server.py` only relays messages. English and Turkish. The player who writes the requests plays in the
browser with friends and tests by playing, so the browser build has to work, not just the desktop one.

## Working with the person who owns the project

- Requests come as plain-language wishlists (often several at once, sometimes with details left open). Do all
  of it, choose sensible numbers, and say in the reply which numbers you chose so they can be changed.
- They want **commit messages in plain language**: a short title, then sections ("Fishers' village", "Archers",
  ...) saying what a player will notice, not what the code did. See `git log` for the style. End each message
  with the attribution trailers the session's instructions give. Never put a model name in code, comments
  or commit text.
- Develop on the branch the session names. **Never force-push** (it gets denied). **Push to `main` only when
  asked**; otherwise push the feature branch and offer to bring `main` up to date. Do not open pull requests
  unless asked.
- Verify in the real browser before saying "works": the project has been bitten by bugs that only showed
  there (see "Traps"). Report what you could not test (Firefox, real speakers, real gamepads) plainly.
- You cannot listen to the music or see the game on a real GPU. Say so; use the numeric checks below and
  ask the player to judge sound.

## Commands

All from the repository root. `tools/out/` is for build products and is git-ignored.

| What | Command |
|---|---|
| Run the desktop game (installs packages, builds raylib into `raylib-linux/`) | `sh run.sh` |
| Build the browser version into `web/` (needs Emscripten on PATH: `source emsdk_env.sh`) | `sh build-web.sh` |
| Play the browser version, alone or together | `python3 server.py` then open the address it prints |
| Logic tests (native, about 130 checks, needs `xvfb` or a display) | `sh tools/harness/run.sh` |
| Browser test build (with `tools/web/testhooks.c`) | `bash tools/web/build.sh` |
| Serve the test build on :8123 | `sh tools/web/serve.sh &` |
| Browser suites (host + guests in headless Chromium; needs Playwright, `PLAYWRIGHT=<module path>`) | `sh tools/web/run-suites.sh` |
| Real-click tests of the production build (serve `web/` on :8124 first) | `node tools/web/click/click3.js`, `flow.js`, `tap.js`, `swing2.js` |
| Strings without a Turkish entry | `python3 tools/trcheck.py src` (only non-text literals such as file names and "WASD" may remain) |
| Render and check the music | `sh tools/music/render.sh` |
| Rebuild `sumer-prebuilt` (zig, glibc 2.29) | `sh tools/prebuilt.sh` (`pip install ziglang`) |
| Screenshot a panel without playing | `tools/out/harness ui out.png <panel> <lang 0/1>` (panel numbers = `Panel` enum: 4 pause, 5 help) |
| Screenshot another player mid-fight, and the red hit lights | `tools/out/harness pvpshot out.png <0-4>` (raising, striking, flinching + lights, mace, parrying) |
| Free patches in the city (for placing a house) | `tools/out/harness spots x0 z0 x1 z1 step half [skip]`; also test a spot against what is built before the city houses (see the sixth house's history in `git log`) |

The harness has more modes (`shot`, `citymap`, `textmap`, `wetways`, `canals`, `pvp`, `archers`, `aim`,
`stats`): read `main()` at the bottom of `tools/harness/harness.c`. `test` runs everything in
`tools/harness/tests.c`. Add new checks as a `TestX()` function called from `RunTests()`, and use
`Check(ok, fmt, ...)`. **Do not put a function call that changes state inside `Check`'s arguments next to a
second argument that reads that state**: C does not say which argument is evaluated first (this printed
stale values once). Compute first, then check.

The harness opens a window (some modes take screenshots): `Xvfb :77 -screen 0 1280x720x24 &` and `DISPLAY=:77`
if there is no display. The test build and servers die when the sandbox restarts: check `curl localhost:8123`.

## Every change: the checklist

1. **Text.** Every English string a player can see goes through `L()` (or a function that does) and needs a
   Turkish entry in `src/turkish.c`, keyed by the *exact* English text. Messages built with `Say("%s", L(..))`
   or `Say("fmt", args)` translate the format. Run `tools/trcheck.py`.
2. **Docs.** Update `README.md` (controls table, the section for the feature, source layout, balance knobs) and the
   in-game help (`DrawHelpPanel` in `src/ui.c`: key list and tips; check both languages with the `ui` screenshot mode,
   the Controls column must not run into Tips).
3. **Network.** If anything sent between host and guests changes (snapshot fields, requests, events, welcome
   message), **bump `NET_VERSION` in `src/mp.c`** (host and guests must match; a mismatch says so). Anything a guest
   must see has to be in the snapshot; guests only *show* the world, the host decides (see below).
4. **Saves.** If the save layout changes, bump `SAVE_VERSION` in `src/save.c` and keep loading old versions
   (migrate, do not refuse). The hosted world has its own version in `mp.c`/`save.c`.
5. **Tests.** Add a check in `tools/harness/tests.c` for the new rule; a browser check in `tools/web/*.js` if it
   involves two players or a real click; run everything.
6. **Rebuild the products.** `web/index.*` (needs emsdk) and `sumer-prebuilt` (zig) are committed; rebuild
   both when `src/` changed, so people who just pull get the new version. Smoke-test the prebuilt (it should
   run until stopped: `timeout 10 ./sumer-prebuilt`).
7. **Commit** on the feature branch with a plain-language message; push the branch.

Code style: C99, 4 spaces, braces on the same line for control flow and on a new line for functions, `static`
functions inside the file that uses them, comments that say *why* (the existing comments are the model), no
warnings with `-Wall -Wextra`. Keep new code the same density as its neighbours. One shared global `Game G` (declared
in `src/game.h`) holds the whole world.

## How the code is organised

`README.md` has the file-by-file table. The ideas that cross files:

- **Frame order** (`GameUpdate` in `src/game.c`): music, menus, player, goods tracking, `SyncSelf`, camera, world
  (`UpdateWorld`: animals, people, combat, villages, jobs, caravans, raiders, arrows), clock, sleep, dawn, weather,
  fields, sky, lights, network, targets, keys. Drawing is `GameDraw`.
- **Host authority.** The host runs everything. A guest's actions go as requests (`Act()` in `src/actions.c`: the host
  does it directly, a guest sends it; `ServeRequest()` answers; `ApplyReply()` applies the answer). The host sends
  snapshots at 10 Hz (`src/mp.c`, `WVitals/RVitals` for everyone's fight state, animals, villagers, raiders,
  caravans, peers). `G.actor` is who is acting; `Say()` goes to the actor (a guest's message is sent to them), `SayTo`
  and `Announce` address others. Peers are in `G.peers[]` (slot 0 is the host); each machine's own slot is `G.self`.
  Guests ease positions toward `netPos`/`netAt`.
- **People fight through `Fighter` structs** rebuilt every frame in `src/combat.c` from villagers, workers, traders,
  caravans and raiders (`FK_*` kinds). Leashes (`InReach`): city guards stay in the city lands and within 40 m of their
  post, caravan guards within 14 m, villagers 45 m. Village anger lasts 120 s, being wanted 120 s (city lands only).
  `Vitals` hold hp, hostility, target, windup, jump state, drawing. Damage to players goes through
  `HurtPlayer(dmg, by)` / `HurtSlot` / `NetHurt` with a cause (`BY_GUARD`, ..., `BY_PLAYER + slot`); the cause is
  what the death message names.
- **Archers** (`Archery()`, `Loose()` in `combat.c`; `LooseFor`/`FlyNpcArrow` in `wildlife.c`): stand still for
  `BOW_DRAW_TIME`, lead the target, arrows with `owner = -1` hit only players.
- **NPC walking** (`StepToward`, `AroundWater`, `JumpWater` in `combat.c`/`city.c`): people jump canals like the player,
  or go round wide water by the bridges; townsfolk follow the street graph in `village.c`.
- **Death** (`Collapse` in `game.c`): the player loses what they gathered that day (`G.safe[]` is the baseline kept
  by `TrackGoods`; storage deposits count as kept).
- **PvP rules** come from the host (`NetRules`, sent in the welcome message): `G.pvp`, `G.combat`. Combat mode gives
  everyone the best gear through the `Use*()` accessors in `items.c`, never by changing their own records.
  In PvP the world also changes: `RemoveZigguratGuards()` (village.c) drops the guards on the ziggurat (host and
  guests both call it right after the rules are known, so their villager lists stay identical), and `CitySpawn()`
  picks where players start and come to after being brought down (the welcome message also carries the spawn).
- **Blows between players** (`combat.c`: `HitPlayer`, `StartBlow`, `UpdateBlows`, `LandBlow`): a melee swing at a player is
  queued and lands `PVP_WINDUP` (0.3 s) later; meanwhile the swinger's `Peer.mark` is `MARK_WARN` (yellow), then
  `MARK_PARRIED` (green: the victim's raised weapon was in time; the swinger is stunned by `NetStunned`/`swingCool` and
  left open to a 1.25x answer, `RIPOSTE`) or `MARK_HIT` (red). The host decides and replicates `mark` in the peers'
  snapshot data; `ui.c` draws the diamond over the head. Arrows still land at once. The swing animation
  (`WeaponAngle` in player.c, `PeerWeapon` in players.c) is timed so the weapon comes down when the blow lands.
- **Where a blow came from**: every damage source passes its position (`HurtSlotFrom(slot, dmg, by, &pos)`, `NetHurt`
  carries it to a guest); `HurtPlayerFrom` turns it into the red edge lights (`G.hitLight[4]`: front, back, left, right
  relative to the camera) and the body's flinch (`G.flinchT`, camera jolt in `FollowCamera`). Other players' flinch is
  guessed from a drop in `Peer.health` (`UpdatePeers`). New damage sources should use `HurtSlotFrom`.
- **Music** (`src/music.c`): see its own section below.
- **Web specifics:** `src/web.c` (storage in IndexedDB: call `SyncStorage()` after writing a file), `src/shell.html`
  (the page, click and key tap fixes), `src/lobby.c` (title screen), `src/net.c` (websocket to `server.py`).
  The canvas size has to be kept in step with the window (a past bug: clicks landed in the wrong place in any
  window that was not 1280x720).
- **Limits that have been hit:** `MAX_VILLAGERS` (64; it ran out at 48), `MAX_FIGHTERS` 128, `MAX_BOXES` 20000.
  `MAX_PLAYERS` is 6 (relay `MAX_GUESTS` in `server.py` is 5; houses in `HOUSE_SPOTS`, colors and their names in
  `village.c`). A new house goes where nothing built *before* the city's own houses overlaps it (those are laid out
  last and go around it), clear of the townsfolk's streets.

## Traps already hit

- Emscripten 6 no longer exposes `Module.HEAPF32`, which raylib's Web Audio code reads on every audio callback:
  without `-sEXPORTED_RUNTIME_METHODS=HEAPF32` (now in `build-web.sh` and `tools/web/build.sh`) the browser game
  throws on each audio block and is silent. Check after any Emscripten or raylib upgrade (`tools/web/audio.js`).
- Browsers start the audio context only after a click or key; the game does not need to handle that (miniaudio does).
- Do not read several values inside one brace initializer when they come from a stream (`Vector3 v = { R(), R(), R() }`):
  evaluation order is unspecified. Read into variables in order.
- Test hooks for the browser (`tools/web/testhooks.c`): `TestInfo()` returns the state as JSON, `TestCommand(n, a, b)`
  drives the game (1 teleport, 2 E, 3 F, 4 look, 5 shoot, 6 swing, 7 add item, 9 set hour, 13 hurt, 14 equip ...). Add
  fields there when a test needs to see something new.
- The prebuilt binary must stay linkable on old systems: build it with `tools/prebuilt.sh`, not with the system compiler.
- Headless Chromium has no GPU: it runs about 5 frames a second, and the game clamps each frame to 0.05 s, so game time
  runs at a quarter of real time there (a 0.3 s windup takes over a second). Time-based checks need generous waits; use
  `waitFor` in `tools/web/lib.js` instead of fixed sleeps, and poll for what you expect.
- Asking a page for its state (`info()`) many times a second slows that page, and a seventh heavy page makes the whole
  machine crawl: `tools/web/mp6.js` tests the full game with six pages and asks the relay itself (a raw WebSocket)
  whether a seventh would be turned away.
- `tools/web/audio.js` counts empty audio blocks; in the 5 fps headless browser a few percent are normal (Web Audio shares the
  main thread with a very slow render loop), so it allows 5%. It has not been measured in a real browser on a real GPU.
- Without a sound device the game runs silently (raylib prints a warning). That is expected on servers and in CI.
- `N` is music, `M` is the map. Check `KEY_*` use in `game.c`, `player.c`, `ui.c` before giving a key a job.

## Music: how to work on it

`src/music.c` holds everything: a synthesizer, three tracks, mood choice, volume and the audio stream. There are
no sound files. Read the comment at the top of the file and the notation comment above the tunes.

- **Loop**: edit the tables (`DAY_PARTS`, `FIGHT_PARTS`, `NIGHT_PARTS`, built from the `LYRE_*`, `PIPE_*`, ... macros) ->
  `sh tools/music/render.sh` -> read the numbers and look at `tools/out/music/spec*.png` -> `sh tools/harness/run.sh`.
  The renderer prints each part's length in beats (they should come out as whole bars) and the loudness; `tools/music/notes`
  lists the notes a part plays with their pitch in cents (`tools/out/music/notes <mood> <part> <first> <count>`);
  `tools/out/music/parts <mood> <seconds>` plays one part at a time and shows which one dominates the mix.
- **Targets**: no clipping, peak under -3 dBFS, RMS about -19 (morning), -17 (fight), -21 dBFS (night); strings and
  pipes in tune to within half a cent (`pitchtest`). The three tunes are balanced against each other; keep them so.
- **Scales** are Pythagorean (`TONE` 9/8, `LIMMA` 256/243) on purpose: that is how the Old Babylonian tuning tablets
  tune a lyre. Do not "fix" them to equal temperament.
- **Notation**: scale degree `1`-`9` (`'` octave up, `,` down), `:beats` (sticky), `-` rest, `1+5+1'` chord, `!` loud, `.` soft,
  `^` grace note from above, `_` from below, `~` slide into the note; drums `D T k B S C`. A part loops on its own length, so
  different part lengths make a long tune; keep every part a whole number of bars.
- **Mood** (`ChooseMood`, `Threatened` in `music.c`) uses only state every client has (hostile flag and target of fighters
  and animals, health drops, other players' flags), so a guest's own game decides its own music. The fight tune holds
  12 s after the last trigger; dusk uses hysteresis (`G.night` above 0.55 enters, below 0.35 leaves). `tests.c` has
  `TestMusic` for all of this.
- **Stream** (`MusicUpdate`): 16384-frame buffers, made a slice at a time (2048 frames per game frame) and all at once only
  when the stream runs dry, so slow frames do not make gaps; volume is raylib's stream volume (instant), not baked in.
  In the browser it runs on the main thread (ScriptProcessor), which is why the buffers are large.
- **Settings file** `sumer.music` (0 off .. 3 high) is written next to the saves and ignored by git.
- New sound effects do not exist yet. If you add some, reuse the voices in `music.c` (plucks, drums) and mix them through
  the same stream, or add a second stream; keep the "no files, nothing to license" property unless told otherwise.

## Ideas that were mentioned and not done

- Firefox and Safari are untested (only Chromium here). Touch/gamepad input does not exist.
- Sound effects (steps, blows, bow, ambience); the music is the only audio.
- The prebuilt binary and `web/` are rebuilt by hand; there is no CI.
