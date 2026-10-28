# robot.md: notes for the next AI (or person) working on Sumer

Read this first. `README.md` describes the game for players; this file is about *working on* it: how to
build and test, how the code is organised, what to update with every change, and the traps already hit.

## The project in one paragraph

An open-world prototype of Ur, about 2100 BC: C99 and raylib 5.5, everything built from primitive shapes and
lit by the game's own shaders (`src/gfx.c`, `src/shaders.c`), about 20,000 lines in `src/`. It runs in the browser (Emscripten),
alone or on the game's servers: `server.py` keeps the accounts and runs each shared world in **the game's server
program** (`host/sumer-host`, the same sources built natively), which is the **host**: it runs the whole world and has
no player of its own; the players' browsers are its guests. Live at sumergame.app (a small Debian VPS behind Caddy
and Cloudflare; the owner pulls `main` there). English and Turkish. The player who writes the requests plays in the
browser with friends and tests by playing. **This branch is browser-only** (the owner's choice, to speed up
testing): the desktop program, `run.sh` and `sumer-prebuilt` are frozen on the `desktop` branch; do not
rebuild or update them unless asked. The native test harness still builds against `raylib-linux/`.

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
  there (see "Traps"). Report what you could not test (Safari, real speakers, real gamepads) plainly.
- You cannot listen to the music or see the game on a real GPU. Say so; use the numeric checks below and
  ask the player to judge sound.

## Commands

All from the repository root. `tools/out/` is for build products and is git-ignored.

| What | Command |
|---|---|
| Build the browser version into `web/` (needs Emscripten on PATH: `source emsdk_env.sh`) | `sh build-web.sh` |
| Build the game's server program into `host/sumer-host` (native; builds `raylib-linux/` the first time) | `sh build-host.sh` |
| Play the browser version, alone or together | `python3 server.py` then open the address it prints |
| Logic tests (native, about 235 checks, needs `xvfb` or a display) | `sh tools/harness/run.sh` |
| `server.py` with the server program: accounts, rooms, things kept, idle stop, `--max-active` | `python3 tools/server-test.py` |
| The server program builds the very same world as the browser (needs emsdk and Node.js) | `sh tools/worldcheck.sh` |
| Browser test build (with `tools/web/testhooks.c`) and the test server program (with `tools/web/hosthooks.c`) | `bash tools/web/build.sh` |
| Serve the test build on :8123 (`--test`: no work for new accounts, rooms made on demand, `/test/...`) | `sh tools/web/serve.sh &` |
| Browser suites (players in headless Chromium; needs Playwright, `PLAYWRIGHT=<module path>`) | `sh tools/web/run-suites.sh` |
| Real-click tests of the production build (serve `web/` on :8124 first) | `node tools/web/click/click3.js`, `flow.js`, `tap.js`, `swing2.js` |
| Strings without a Turkish entry | `python3 tools/trcheck.py src` (only non-text literals such as file names and "WASD" may remain) |
| Render and check the music | `sh tools/music/render.sh` |
| Screenshot a panel without playing | `tools/out/harness ui out.png <panel> <lang 0/1>` (panel numbers = `Panel` enum: 4 pause, 5 help) |
| Screenshot another player mid-fight, and the red hit lights | `tools/out/harness pvpshot out.png <0-4>` (raising, striking, flinching + lights, mace, parrying) |
| Screenshot a view (shaders on by default; `SUMER_GFX=Low` or `Classic` for the others) | `tools/out/harness shot out.png x z yaw pitch dist hour [frames] [rain]` |
| Mesh sizes and build times of the shader renderer | `GFX_STATS=1 tools/out/harness shot ...` (prints to stderr) |
| A phone held sideways: lobby, tap, HUD, panels (screenshots in `tools/web/shots/phone-*.png`) | `node tools/web/phone.js` (test build on :8123) |
| A swing frozen at t seconds, seen from the side (weapon and trail) | `tools/out/harness swingshot out.png <t> <mace 0/1>` |
| The sound effects as .wav files | `tools/out/harness sfx <dir>` |
| A line-up of people: players' choices with armor (0), townsfolk and other roles (1), poses (2), heads up close (3) | `tools/out/harness lineup out.png <0-3> [hour]` |
| The "Your look" window on the player | `tools/out/harness ui out.png 7 <lang 0/1>` |
| Every guard's post, alone and after PvP takes the ziggurat's away | `tools/out/harness guards` |
| Things standing on the ground that hang above it as drawn (worst first, and how many) | `tools/out/harness floaters [min gap]` (0 or 1 expected) |
| A screenshot standing on an upper floor (the ziggurat's middle terrace: 8) | `SHOT_FLOOR=8 tools/out/harness shotin out.png x z yaw pitch dist hour` |
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
   must see has to be in the snapshot; guests only *show* the world, the host decides (see below). If a player's
   record (`WriteRecord`) changes, the accounts' stored copies need `RECORD_FORMAT` (mp.c) and a way to read the old one.
   **World building must give the same world on every compiler** (the server program is gcc, the browser clang): never
   two random draws in one expression or argument list (C leaves the order open; write them into variables first),
   build with `tools/same-world.flags`, and run `tools/worldcheck.sh` after changing what the world is built from.
4. **Saves.** If the save layout changes, bump `SAVE_VERSION` in `src/save.c` and keep loading old versions
   (migrate, do not refuse). The hosted world has its own version in `mp.c`/`save.c`.
5. **Tests.** Add a check in `tools/harness/tests.c` for the new rule; a browser check in `tools/web/*.js` if it
   involves two players or a real click; run everything.
6. **Rebuild the product.** `web/index.*` (needs emsdk) and `host/sumer-host` (`sh build-host.sh`, 64-bit Linux,
   built in a container with an older glibc than the server's Debian 13) are committed; rebuild both when `src/`
   changed, so the server, which just pulls, gets the new version.
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
  and `Announce` address others. Peers are in `G.peers[]`; each machine's own slot is `G.self`.
  Guests ease positions toward `netPos`/`netAt`.
- **The server program** (`host/main.c`, `ServeRoom` and `ServerTick`): `server.py` starts it as
  `sumer-host survival|pvp|royale ROOM-ID` in `data/rooms/<id>/` and talks to it over its standard input and output
  (`net.c`: every message has its length in front, 4 bytes little-endian; stdout is taken for that and printing goes
  to stderr). It ticks 60 times a second in real time (`ServerTick`: the world without a local player), loads and saves
  `sumer-mp.sav` there (survival and PvP), and stops (saving) when its input closes. **Slot 0 is the server's own and never
  a player** (`Dedicated()`); players have slots 1-20, and houses and colors go by `Seat(slot)` = slot - 1, so the first
  player in gets the blue house as one playing alone does. Rules come from the room's kind (`G.pvp` for PvP and the
  royale, `G.combat`, best gear for everyone, only in the royale); there is no combat mode any more.
- **What the server program and server.py tell each other** (`mp.c`): `S_JOIN id` carries the account's name and its
  record for that kind of room (`[RECORD_FORMAT][WriteRecord]`, empty the first time and in the royale); the player's own
  `M_HELLO` only checks the version. Back to `server.py` (`NetSend(NET_TO_SERVER, ...)`): `H_STORE` (a player's record
  whenever they send one: every 15 s, on waking, on leaving), `H_PROGRESS` (experience, kills and deaths gained, by name;
  the player's game hears it as `EV_PROGRESS`), `H_STATUS` (every 2 s: players, the royale's phase, for the list of
  servers), and in test builds `S_TEST`/`H_TEST` (`tools/web/hosthooks.c`).
- **Accounts** (`server.py`: `Store` on SQLite, scrypt hashes, sessions as an HttpOnly cookie holding a token whose
  SHA-256 is stored; `Limits` per address in memory; `CHALLENGES` for the proof of work). The game's side is
  `src/account.c`: requests go through `ApiStart`/`ApiDone` (`web.c`, fetch); answers are lines of tab-separated words
  (`name xp kills deaths`; the rooms as `id mode players max state title`). Logging in and creating an account read the
  page's own fields (`shell.html`: `#acctName`, `#acctPass`, `#acctPass2`, placed over the title screen by `AccountForm`),
  so the password never enters the game; their key events are kept from GLFW's handlers (a capture listener on window
  registered before GLFW's, `stopImmediatePropagation`), or Backspace would not work in them. A new account first gets
  a challenge and finds a number whose SHA-256 (with the challenge and the name) starts with `--work` zero bits
  (`WorkDone` in `sha256.c`, a slice per frame in `AccountFrame`). The title's `PAGE_ACCOUNT` changes the password
  (`/api/password`, ends the account's other sessions) or deletes the account (`/api/delete`, refused while it plays;
  `AccountForm(FORM_PASSWORD, ..)` lays `#acctPass`, `#acctPass2`, `#acctPass3` over it). `web/privacy.html` is the
  privacy page (owner Yuşa Zorlu; keep it true when what is kept changes). Singleplayer saves go to the account after every
  `SaveGame` (`AccountSaved`) and come back from it at login (`TakeSaves`, replacing the browser's). An account with
  no saves on the server takes up the browser's own instead (made before accounts), unless `sumer.owner` (whose
  saves the browser holds, written at each login) names another account.
- **Experience** (`src/progress.c`): `AwardXp(slot, xp, kills, deaths)` from the kill sites (`DamagePerson` in
  combat.c, `HitAnimal` in wildlife.c, `RQ_COLLAPSE` in actions.c, the royale's winner); on a server it becomes
  `HostProgress`, alone `AccountGain(.., report)`, which `account.c` sends every 10 s. `LevelStart(L)` = 50 L (L - 1).
- **Touch controls** (`src/touch.c`): the page's touch events (not pointer events: synthetic touches in headless Chrome
  give pointer events at 0,0) are read once a frame in `TouchFrame` (called from `PageFrame`); each finger gets a role
  where it came down: a button, the stick (left 40%, lower 65%) or looking. The game asks `TouchPressed/TouchDown/
  TouchStick/TouchRun/TouchLook` next to its keys. raylib counts any finger as the left mouse button, so the mouse bow
  draw uses `MouseHeld` and mouse strikes need the pointer captured. A tap on a touch button is not also a menu click
  (`ForgetClick`). Shown where `OnPhone()`, else after the first touch. Tested by phone.js with real touch events.
  On a phone (`OnPhone()`) the game never asks for pointer lock (`LockMouse`, `WantPointer`) and never pauses on
  losing focus or the pointer: Firefox for Android granted the lock on a tap and its loss paused the game. The
  account fields are the page's own, so a tap on one brings the phone's keyboard.
  Full screen on a phone is asked for by the page (`shell.html`, on touchend). `LeftHanded()` mirrors the thumb's
  buttons and the stick's half; the Attack finger also aims while the bow is drawn.
- **Frame limit** (`FpsLimit`, game settings, kept in `sumer.prefs` with the arrow camera, view and hand; loaded at
  start-up too): `Frame` in main.c lets the page's frame calls pass until 0.9/limit seconds have gone.
- **Chat** (T, online only; T fast-forwards only offline): `PANEL_CHAT` keeps the game keys off while the line is typed in
  `DrawChat` (`ui.c`, which also keeps the last 8 lines). A guest sends `M_CHAT` to the host; `HostChat` (`mp.c`) tidies it
  (`ChatTidy`: control characters out, 100 bytes, cut on a whole UTF-8 letter), allows two a second per slot, and sends
  `EV_CHAT` (slot + text) to everyone. The browser test types real keys (mp.js); `TestInfo` has the last line as `chat`.
- **People fight through `Fighter` structs** rebuilt every frame in `src/combat.c` from villagers, workers, traders,
  caravans and raiders (`FK_*` kinds). Leashes (`InReach`): city guards stay in the city lands and within 40 m of their
  post, caravan guards within 14 m, villagers 45 m. Village anger lasts 120 s, being wanted 120 s (city lands only).
  `Vitals` hold hp, hostility, target, windup, jump state, drawing. Damage to players goes through
  `HurtPlayer(dmg, by)` / `HurtSlot` / `NetHurt` with a cause (`BY_GUARD`, ..., `BY_PLAYER + slot`); the cause is
  what the death message names.
- **Archers** (`Archery()`, `Loose()` in `combat.c`; `LooseFor`/`FlyNpcArrow` in `wildlife.c`): stand still for
  `BOW_DRAW_TIME`, lead the target, arrows with `owner = -1` hit only players. Drawn with a bow always visible:
  `HoldBow` (held low between shots), `SlungBow` (city guards with a spear in hand), the bow pose while drawing.
- **Water** (`WadeSpeed` in `world.c`): the player wades through water boxes (`MoveHorizontal` skips them): 40% speed in
  canals, ponds and the harbor, 15% in the river (by x between `RIVER_X0`/`RIVER_X1`), floating head-out over the deep
  river bed (`MoveVertical`). Only below `WATER_LEVEL + 0.2` does water slow you, so bridges, piers and the (solid) harbor quays
  are walked at full speed. Rain skips drops under any box whose bottom is 1.8 m over the ground (roofs, ceilings: `DrawRain`).
  The camera stops at thin boxes overhead too (house roofs), so inside a roofed room it stays under the roof. The ferry was removed; `STN_FERRY_E/W` remain only as the landing stages' positions.
- **Photo detail** (`assets/detail.png`, `LoadDetail()` in `gfx.c`, `det()` and `detailMap` in `shaders.c`): four grey
  CC0 maps in one texture, R ground, G mud brick, B wood, A rock, mid grey = no change. The world shader samples them in
  world space (triplanar, no UVs) where it used to draw noise, and multiplies the vertex color by them. Rebuild with
  `python3 -I tools/assets/make_detail.py` (Pillow, network); the browser builds embed it (`--embed-file`). A missing file
  falls back to flat grey. Only CC0 sources (Poly Haven, ambientCG, Kenney, Quaternius); list each in the README's credits.
- **People's grain, roads, flames**: `assets/detail2.png` (R cloth, G fleece/fur, B leather, A road) is read
  by the world shader for MAT_FIGURE where a color's alpha says what it is (`Tag()` in `figures.c`: 254 cloth,
  253 fleece, 252 leather, 251 metal, which only shines; 255 the rest). People's vertices carry their position on
  their own body in the 2nd-4th material bytes (`VVertex` in `gfx.c`), so the grain moves with them. Roads are
  ground with alpha 254. Flames: `Flame()`/`DrawTorches()` call `GfxFlame()`, and `DrawFlames()` in `gfx.c` draws
  them after the world as additive billboards from `assets/flame.png`; without shaders the old glow boxes stay.
- **Trees and fields**: `assets/detail3.png` (R bark, G leaves, B tilled earth, A straw) on texture unit 9 (WebGL 2
  promises only units 0-15). Any non-figure color with alpha 254 bark (`COL_BARK`), 253 leaves (`COL_TREE`,
  `COL_PALM_LEAF`, the tree literals in `world.c`), 252 tilled earth (`COL_SOIL`), 251 straw (`TAG_CROP()`) gets it,
  triplanar in world space (`det3()`), and is drawn opaque. Keep plain `COL_PALM_TRUNK` (255) for beams and
  posts, and do not `Mix()` a tagged color with an untagged one (the alpha lands on a random tag).
- **Finding a way** (`src/path.c`): fighters (guards, wardens, raiders, barbarians, bandits, caravan guards, villagers
  in a fight) go straight while `PathClear` says the line is walkable (looked along every 0.5 s); when it is not,
  `PathWay` follows corners from `FindPath`, A* on a 1 m grid that follows the ground's height (stairs, terraces,
  bridges; a step up of `STEP_HEIGHT`, down of 1.3 m), crosses canals narrow enough to jump (wet cells cost a
  little more; `JumpWater` does the jump) and not the river, harbor or ponds. At most 2 searches a frame
  (`PathNewFrame` in `UpdateCombat`), 4000 cells each (about 2 ms); a goal beyond that gets the way to the
  nearest place found, looked for again in 3 s. `TestPaths` has a U of walls the old steering never left.
- **Speed** (measured with `harness stats` and a `-pg` build of the harness): `BoxesNear()` in `world.c` is an
  8 m grid of the boxes; `GroundHeight`, `SolidAt`, the player's and NPCs' collisions, the camera and the aim ray
  ask it instead of scanning every box (update went from 0.87 to 0.06 ms a frame). New boxes join it before
  the next question and `BuildWorld` starts it over; boxes must never move sideways. `InView(p, size)` in
  `render.c` skips people and animals behind the camera (with a margin for their shadows); call it in any new
  loop that draws figures. Guests carry other players on at their last speed (`PeerMoved`, `vel`) and
  animals along their heading (`Ease` in `mp.c`), so they glide between the host's ten pictures a second.
  `TestFaster` checks all three.
- **Music of places** (`UpdatePlaces` in `music.c`, files in `assets/music/` from `tools/assets/make_music.sh`): one
  raylib music stream per place (city, village, open land, ziggurat), each with a 1 s buffer so slow frames do
  not starve it; while one plays, the synthesizer's tracks are silent and not computed at all (`idle`). The
  fight mood brings the synthesizer back. Only CC0 music; list it in the README's credits. `audio.js` checks
  each place (`MusicPlace()` in the test info).
- **Alehouse** (`BuildAlehouse`, `AlehouseFolk`, `InAlehouse` in `village.c`; the rule in `AlehouseRule` and
  `SetEquip` in `player.c`): built before the city's houses so they keep clear of it. Its folk are appended
  last (`Villager.talks`: they turn and gesture while drawn). No drink is sold there yet, by the owner's wish.
- **Leaf crowns**: a blob in leaf color (alpha 253) is lumpy (`Lump()` in `render.c`, the same waves at every level of
  detail) and gets no normal, so the shader lights it facet by facet. Trees add two hashed clumps (no `RandF`).
- **Partridges** (`AN_PARTRIDGE`, item `IT_PARTRIDGE`): herds appended at the end of `HERDS` because saves keep the
  killed-animals mask by index; they are fliers (`flies`), rising 3 m while they flee.
- **The open land** (`BuildWilds()` in `world.c`): tamarisk, camelthorn, fallen wood and stones, none solid, kept off
  roads, fields and walls by `InKeepOut`. It draws from the world's random numbers before the herbs, so moving it
  moves everything placed after it (tests that pick "the first wolf east of the river" must not lean on luck).
- **Player torch** (`UpdateTorch()` in `player.c`): `G.torchOn` is the wish (B), `G.torchHeld` what the hand holds,
  1 s (`TORCH_TIME`) to change, reset by `SetEquip()`; dropped at once when the bow is drawn (the draw never waits). `TorchBurning()` sets `PF_TORCH`
  in the peer flags, which gives the light (`UpdateLights`) and the wider notice for wolves and prey (`wildlife.c`).
  Every torch burns while `TorchesLit()` (`G.torchGlow`, from 17:00, brightening to full by 20:00 or with the dark);
  the world shader's `night` uniform is that glow, as it only scales torchlight there.
  Bodies: `STYLE_WOMAN` (256) in the 16-bit style. First person: `ViewModel()` in `player.c`.
- **Ground as drawn** (`SettleOnGround` in `world.c`, after `BuildTerrain`): things are placed on `TerrainHeight`, but the
  ground is drawn as 6 m triangles (`GroundSurface`/`MeshHeight`), which sag below it over hills. The pass extends
  whatever stands on the land down to the drawn ground (boxes and cylinders grow down, blobs and cones sink). Anything
  added after `BuildTerrain` must use `GroundSurface` itself.
- **Bandits** (`raiders.c`, `BANDS`, `BAND_FIRST`): two roaming bands of three after the camps in `G.raiders` (camp index
  `CAMPS + b`, `RaiderBandit(i)`), role `ROLE_BANDIT`, cause `BY_BANDIT`. `Roam()` leads the first living member to
  random stops (`BandSpot`: far from city, villages, camps and edge, off roads), sits the band down at some of them, and
  keeps followers on the leader's trail. Only the host moves them; guests see the snapshots.
- **Wolves** (`wildlife.c`): `Step()` tries side steps round anything solid and eases ground animals apart; a wolf flees
  a fight only when `Beaten()` (badly hurt and `Lone()`: no pack mate on its feet within 40 m).
- **NPC walking** (`StepToward`, `AroundWater`, `JumpWater` in `combat.c`/`city.c`): people jump canals (they do not wade),
  or go round wide water by the bridges; townsfolk follow the street graph in `village.c`.
- **Gate towers** (`BuildWalls`/`BuildGateTower` in `city.c`): hollow, axis-aligned, t along the wall away from the gate and
  n into the city. 25 steps (`StairStep`: a square landing in each corner, six steps along each wall) from the ground to
  `GT_MID` (= `WALL_TOP`, the wall walk) and on to `GT_TOP`; floors are tiles left open over steps a head below
  (`TowerFloor`); walls with openings (`TowerSide`). Wall points within reach of a tower are left out (`skip`) and a box
  joins the wall walk to the middle floor's door. `GateTowerPost` places the roof archer (`Villager.perch`: never moves,
  only shoots, in `UpdateCombat`; kept in PvP) and the guard sitting on the middle floor. `harness towers` lists them.
- **Rooftops** (`HouseClimbs` in `city.c`): after every house stands, stairs or crates on a free side of some houses
  (`ClearStrip`). A jump rises about 1.17 m and a step up is at most 0.55 m (`STEP_HEIGHT`).
- **Esc and the mouse** (`HandleMenus` in `game.c`): a browser gives the pointer back only on a click, and a grab
  made on Esc is dropped by that same key, which reopened the pause menu; so a menu closed with Esc leaves the mouse
  free (`WantPointer(true)`, `G.cursorLocked = false`) until the next click takes it.
- **Death** (`Collapse` in `game.c`): the player loses what they gathered that day (`G.safe[]` is the baseline kept
  by `TrackGoods`; storage deposits count as kept).
- **PvP rules** come from the room's kind (`ServeRoom`, sent in the welcome message): `G.pvp`, `G.combat`. The royale's
  best gear goes through the `Use*()` accessors in `items.c`, never by changing anyone's own records.
  PvP servers set `G.combat` too (the best gear, as the old combat mode); `UnlimitedArrows()` keeps the quiver full; in `Hurt` (combat.c) guards never take sides between
  players (the user's wish: no wanted status from PvP; attacking NPCs still makes you wanted). Livestock (`G.cows`,
  `G.sheep`, `G.flock`) cannot be hurt and are solid for the local player only (`PushOutHerd` in people.c), since
  each machine's livestock wander on their own.
  In PvP the world also changes: `RemoveZigguratGuards()` (village.c) drops the three guards on the ziggurat's terrace (posts 5 m up or more; every gate keeps its pair) (host and
  guests both call it right after the rules are known, so their villager lists stay identical), and `CitySpawn()`
  picks where players start and come to after being brought down (the welcome message also carries the spawn).
- **Blows between players** (`combat.c`: `HitPlayer`, `StartBlow`, `UpdateBlows`, `LandBlow`): a melee swing at a player is
  queued and lands `PVP_WINDUP` (0.3 s) later; meanwhile the swinger's `Peer.mark` is `MARK_WARN` (yellow), then
  `MARK_PARRIED` (green: the victim's raised weapon was in time; the swinger is stunned by `NetStunned`/`swingCool` and
  left open to a 1.25x answer, `RIPOSTE`) or `MARK_HIT` (red). The host decides and replicates `mark` in the peers'
  snapshot data; `ui.c` draws the diamond over the head. Arrows still land at once. The swing animation
  (`WeaponAngle` and `SwordFrame` in player.c, drawn for everyone by `HoldWeapon`) is timed so the weapon comes down when the blow lands.
- **When a swing lands** (`MeleeStrike(phase)` in wildlife.c, timed in `UpdatePlayer`): a swing has two halves.
  `STRIKE_START` (the button) only sets a blow at another player going (it is queued for `PVP_WINDUP` and gives the
  yellow mark); `STRIKE_CONTACT`, `CONTACT_TIME` (0.3 s) later as the weapon comes down, hits the person or animal in
  front (`StrikeFor`, the host's half; a guest sends both halves as `RQ_STRIKE` with the phase). `WeaponAngle(t)` and
  `MaceYaw(t)` (player.c) are drawn by elapsed time so the weapon visibly meets the target at contact; people use
  `BlowAngle`/`SpearThrust`, which come down in the last moment of their `NPC_WINDUP`. Keep all of these in step when
  changing a timing.
- **How a blow feels** (effects.c): `BlowLanded` (hit-stop `G.hitStop` that holds the swing, view kick `G.shakeT`,
  local blood/dust, `AddFlash` that makes whoever stands there pale for a moment, a thud), `BlowTurnedAside` (clang,
  sparks), `BlowTaken` (thump, from `HurtPlayerFrom`). The swinger's own game predicts contact with `StrikeWouldMeet`,
  so it feels at once even as a guest; a blow on a player is felt when the host says it landed (`EV_LANDED`), a parry
  through `EV_PARRIED`/`EV_STUNNED`. `G.meleeing` tells `HitMark` a blow from an arrow (arrows crack, `EV_HIT` kind).
- **Sound effects** (`src/sfx.c`): made at the first `SfxPlay` from formulas (no files), three voices each (raylib sound
  aliases), pitch varied a little. `SfxPlayAt` fades with distance from the camera. `tools/out/harness sfx <dir>` writes
  them as .wav files; keep most of the energy above 150 Hz or phone and laptop speakers lose it.
- **Where a blow came from**: every damage source passes its position (`HurtSlotFrom(slot, dmg, by, &pos)`, `NetHurt`
  carries it to a guest); `HurtPlayerFrom` turns it into the red edge lights (`G.hitLight[4]`: front, back, left, right
  relative to the camera) and the body's flinch (`G.flinchT`, camera jolt in `FollowCamera`). Other players' flinch is
  guessed from a drop in `Peer.health` (`UpdatePeers`). New damage sources should use `HurtSlotFrom`.
- **People** (`src/figures.c`; `src/people.c` is the unrelated body-collision code): `DrawHuman(pos, facing, stride,
  look, crouch, bow, draw)` draws a body from a `Look` (colors, then `hairStyle`, `beard`, `dress`, `jewel`, `head`,
  `armor`, `woman`). Every role gets its look from `PersonLook(LOOK_*, seed, robe)` (the seed is the person's index,
  so host and guests agree without sending anything); players from `PlayerLook(slot, style, armor)`, where `style` is
  the byte from `PackStyle` that the "Your look" window (`DrawLookPanel` in ui.c, `PANEL_LOOK`) saves in `sumer.look`
  and that goes to the others in `Peer.style` (state and snapshot messages). Shapes: `Loft` (rings stacked up, a
  garment painted by `TorsoPaint`), `Tube` (limbs), blobs; three levels of detail by distance (26 m, 80 m), about
  the same vertex count as the old block people. Legs bend (a step, a crouch on bent knees), and the lower foot stays
  on the ground (`lift`). **Hands**: draw what a person holds first and call `GripHere(hand, x, y, z)` inside its
  transform (or `GripAt` with a point on the body); the next `DrawHuman` reaches that hand there (two-bone `Reach`).
  The player's and other players' weapons go through `HoldWeapon`/`DrawWeaponPart` (player.c), whose frames
  (`SwordFrame`, `ArmFrame`, `BlowFrame` for people's tools and axes) the swing trail also uses: change them together.
- **Battle royale** (`src/royale.c`, Sumer Royale on the title screen): `Royale()` is set by the room joined
  (`SetRoyale` from the room's kind; `?mode=br` opens the title on its page). `MAX_PLAYERS` is 21 (slot 0 the server's),
  `PlayerLimit()` 20 players in it and 6 outside it; there are `MAX_HOUSES` 6 houses (`HouseOf(slot)`). A round starts
  `ROUND_WAIT` (10 s) after everyone in (two or more) is ready (`PF_READY` in each player's flags, `ReadyCount`, shared with
  the party in party.c; `rnd.countdown`, sent with the round, is -1 while anyone is not ready); `UpdateIdle` sends away
  (`KickSlot`) whoever is not ready and has not moved for 3 minutes. The
  winner gets `XP_WIN` for each player it began with (`rnd.started`, also sent: guests once read 0). The
  world is built with `BuildRoyale()` (statues, more trees; same on every machine) and `ShapeRoyale()` after `GameInit`
  builds it (no guards or wardens, caravans away; raiders and the bandit bands are skipped in their own functions). The host runs the round
  (`rnd`: phase `RP_WAIT/RUN/OVER`, its clock, the claimed statues as a bit mask, the winner), sent in every snapshot
  (`WriteRoyale`/`ReadRoyale`); `EV_ROUND` sets a guest down for a new round; `RQ_STATUE` claims a statue. A fallen player
  sets `PF_OUT` in their own flags (`RoyaleFall` from `Collapse`); out players are not bodies, targets or drawn, and their
  camera follows another (`RoyaleCamera`). The storm hurts through `HurtSlotFrom(.., BY_STORM, ..)` on the host. Tests:
  `TestRoyale` (harness), `tools/web/royale.js` (a `royale-*` room on serve.sh's port 8123), `harness royale out.png x z
  yaw t [statue]` for pictures.
- **Sumer Party** (`src/party.c`, room kind `party`, `ROOM_PARTY`; `?mode=party`): `Party()`; `Match()` is royale or
  party (nothing kept, no house use, `HostRecord` skipped), `RaidersAway()` hides raiders except in city defence,
  `OutOfRound()` (royale out, or `PartyOut()`: watching a game joined late, or the 5 s down after a fall) sets `PF_OUT`.
  `ShapeParty()` after `GameInit` (both sides: no guards, caravans away, `PartyCrowd()` adds 24 walkers, so the villager
  lists match). The host runs `pty` (phase `PP_WAIT/COUNT/GAME/BREAK/END`, clock, the four games' `order`, `joined` bit
  mask, `round`/`total` scores, the hide-and-seek `seeker`, `npcKills`, `found`), sent in every snapshot
  (`WriteParty`/`ReadParty`); `EV_PARTY` (`NetParty`) sets a guest down for a game (kind 1) or a break (0) through
  `PartyBegin`. The veto vote rides in the player flags (`PF_VOTE_SHIFT`, game + 1). `ApplyRules()` sets `G.pvp` per
  game (`G.combat` always: the best gear). `PartyFall` (from `Collapse`) handles a fall on every machine; the host
  scores it in `PartyDeath` (from `RQ_COLLAPSE`). Per game: lava (`StartSpot` on `RoofSpot`s from city.c, `BurnLava`
  through `BY_LAVA` after a 3 s grace, `DrawLava` from `DrawEffects`); archers (`G.equip` forced to the bow, arrows
  1000 damage in wildlife.c); defence (`StormCity`/`Defence` in raiders.c, `SpotPlayer` sees everyone, +2 in the kill
  credit in combat.c); dates (`Peer.dates` in `M_STATE`, scored live); hide and seek (`PartyHider`, `PartyMayHurt` in
  `Hittable`, `PartyDisguise` for looks, no tags or map dots, `PartyNpcKill`, `HideUpdate`, `BY_STRAY`, `FarVillage`).
  Tests: `TestParty` and `TestHide` (harness; `PartySeekerForTest`), `tools/web/party.js` and `tools/web/hide.js`
  (`party-*` rooms; host cmd 30 the party's clock, 31 the seeker finds a player).
- **Music** (`src/music.c`): see its own section below.
- **Drawing**: see "Graphics: how drawing works" below.
- **Web specifics:** `src/web.c` (storage in IndexedDB: call `SyncStorage()` after writing a file; requests to `/api`),
  `src/shell.html` (the page, click and key tap fixes, the account fields), `src/lobby.c` (title screen), `src/net.c`
  (websocket to `server.py`).
  The canvas size has to be kept in step with the window (a past bug: clicks landed in the wrong place in any
  window that was not 1280x720). `PageFrame` now gives the canvas the screen's own pixels (`PixelRatio()`, up to
  about 4 million) and the page's CSS stretches it over the window; mouse and finger positions for menus come from
  the page (`PointerPosition()`), because raylib follows only the mouse and misses quick taps.
- **The UI's units** (`UiScale` in `ui.c`): the UI is laid out for at least 960 x 540 and drawn through one scale
  (`UiBegin`/`UiEnd` around `DrawUI` and the lobby). Inside, use `UiWidth()`/`UiHeight()`, `UiMouse()` and
  `OnScreen()` (world to UI point), never `GetScreenWidth()`, `GetMousePosition()` or `GetWorldToScreen()` directly;
  a scissor rectangle has to be multiplied by `UiScale()`. Panels must fit 960 x 540.
- **Limits that have been hit:** `MAX_VILLAGERS` (64; it ran out at 48), `MAX_FIGHTERS` 128, `MAX_BOXES` 20000.
  `MAX_PLAYERS` is 21 with slot 0 the server's (`MAX_PLAYERS` per kind in `server.py`: 6, 6, 20; houses in
  `HOUSE_SPOTS`, colors and their names by seat in `village.c`). A new house goes where nothing built *before* the city's own houses overlaps it (those are laid out
  last and go around it), clear of the townsfolk's streets.

## Traps already hit

- Emscripten 6 no longer exposes `Module.HEAPF32`, which raylib's Web Audio code reads on every audio callback:
  without `-sEXPORTED_RUNTIME_METHODS=HEAPF32` (now in `build-web.sh` and `tools/web/build.sh`) the browser game
  throws on each audio block and is silent. Check after any Emscripten or raylib upgrade (`tools/web/audio.js`).
- Browsers start the audio context only after a click or key; the game does not need to handle that (miniaudio does).
- Do not read several values inside one brace initializer or argument list when they come from a stream
  (`Vector3 v = { R(), R(), R() }`, `AddBox(x + RandF(..), .., RandF(..))`): evaluation order is unspecified, and gcc
  and clang differ. Read into variables in order. For random draws while building the world this once made the
  server program's world differ from the browser's (reeds 3 m apart): `tools/worldcheck.sh` catches it.
- clang (the browser) fuses `a * b + c` when it folds constants at build time and gcc does not: every build passes
  `tools/same-world.flags` (`-ffp-contract=off`, and no build-time folding of `sinf` and the like).
- The server program's world runs in real time, while a headless browser page's game time runs slow (see below): in
  the browser suites a 0.3 s blow warning or a gazelle's start can pass between two looks; poll the page that sees it
  often, shoot at once, and freeze the royale's countdown (`hostCmd(room, 22, 600)`) while a slow page loads.
- `EM_ASM` code is a macro argument: commas inside `{ ... }` or `[ ... ]` split it too. Wrap object and array literals in
  parentheses (`var o = ({ a: 1, b: 2 });`) and declare one `var` per line.
- Test hooks for the browser (`tools/web/testhooks.c`): `TestInfo()` returns the state as JSON, `TestCommand(n, a, b)`
  drives the game (1 teleport, 2 E, 3 F, 4 look, 5 shoot, 6 swing, 7 add item, 9 set hour, 13 hurt, 14 equip ...). Add
  fields there when a test needs to see something new. The server program's own (`tools/web/hosthooks.c`) answer
  `hostInfo(room)` and `hostCmd(room, n, a, b)` in `tools/web/lib.js` (9 hour, 20 start a royale round, 21 the round's
  clock, 22 the countdown, 30 the party's clock, 31 the hide-and-seek seeker finds a player), and `stopRoom(room)` stops a room as if it stood empty. `lib.js`'s `open(browser, name, url)`
  logs a fresh browser context in to its own account first; every run uses new names and rooms (`RUN`).
- Headless Chromium has no GPU: it runs about 5 frames a second, and the game clamps each frame to 0.05 s, so game time
  runs at a quarter of real time there (a 0.3 s windup takes over a second). Time-based checks need generous waits; use
  `waitFor` in `tools/web/lib.js` instead of fixed sleeps, and poll for what you expect.
- Asking a page for its state (`info()`) many times a second slows that page, and a seventh heavy page makes the whole
  machine crawl: `tools/web/mp6.js` tests the full game with six pages and asks the server itself (a raw WebSocket from
  a seventh account's light page) whether a seventh would be turned away.
- `TestInfo` answers `lobby: 0` before the title screen has even opened (storage still loading): wait for `day > 0` too.
- `tools/web/audio.js` counts empty audio blocks; in the 5 fps headless browser a few percent are normal (Web Audio shares the
  main thread with a very slow render loop), so it allows 5%. It has not been measured in a real browser on a real GPU.
- Without a sound device the game runs silently (raylib prints a warning). That is expected on servers and in CI.
- `N` is music, `M` is the map, `G` the graphics setting. Check `KEY_*` use in `game.c`, `player.c`, `ui.c` before giving a key a job.

## Graphics: how drawing works

The world's draw code (`render.c` primitives, `DrawGround`, `DrawWater`, `DrawRoads`, `DrawBoxAt`, `DrawResource`,
`DrawHuman`, the animals ...) is the same for every setting. What changes is where its vertices go.

- **The vertex stream** (`VBegin/VColor/VNormal/VVertex/VEnd` in `gfx.c`): every piece of world geometry is drawn
  through it, never with `rlBegin`/`rlVertex3f` directly (only transparent effects, rain and arrow trails, still use
  rlgl). Classic: it goes straight to rlgl. With the shaders it is caught into a list (`Catch`): positions turned by the
  current `rlPushMatrix` transform, a normal (0 = let the shader take the surface's slope), the color and a material.
- **`GfxOn()`**: while it is true, `Lit()` hands colors back as they are (the shaders do light, torches and fog), and
  the primitives add normals: smooth ones on blobs, cylinders and the ground, face normals on boxes.
- **Materials** (`MAT_*` in `game.h`, `SetMaterial`): thing, ground, water, glow (always set by `DrawGlowBox`),
  figure (people and animals: no brick or leaf patterns), built (city boxes: mud brick and plaster on light earthen
  colors), unlit. The world shader decides by material and color: green = leaves, dark warm brown = wood.
- **Still world**: `BuildStill` catches the ground, water and roads of the whole map once (`DrawingAll()` turns off
  their culling) and sorts each quad into 60 m chunks. Each chunk's boxes and resources (`TakeNew` adds new ones: a
  palm planted, a game loaded) are drawn into a mesh at three levels of detail (`BuildThings`, `LOD_D2`), made when
  first needed, a few milliseconds a frame (`ChooseLods`). A chunk's meshes are thrown away when one of its resources
  is cut or grows back (`ResSig`). Crop rows (`FieldBox`, `DrawFields`) and fish rings move, so they are not in them.
  About 220k vertices of ground and 600k of things in all; `GFX_STATS=1` prints the numbers.
- **What moves** (`CatchMoving`): people, animals, boats, caravans, players, arrows, flames, every frame, into
  dynamic buffers of 65536 vertices. Everything is quads, so every buffer shares one index buffer.
- **A frame** (`GfxDrawWorld`): the sun's view into a 2048 (Low: 1024) shadow map, depth packed into the colors
  (`SH_DEPTH_FS`), over a 130 m (90 m) square ahead of the camera stepped by whole texels; the sky dome; the chunks
  in view and what moves, with the world shader; effects and rain through rlgl with the same shader; then glow
  (High), FXAA, a little saturation and a vignette to the screen. The UI is drawn after, untouched.
- **Light** (`Sky()`): sun by day, moon (dim, blue) by night, colors from the hour, the season and the rain. Torches:
  `MakeTorches` builds a grid of 8 m squares listing the (up to 8) torches that reach each, which the shader reads
  (`torchGrid`, `torchData`); hand torches (`G.dynLights`) go as uniforms. Their strength follows `G.night`.
- **Settings** (`GfxTier`): Auto picks Low where `OnPhone()`, or after `WatchSpeed` finds High too slow. Classic also runs when the GL version is not 3.3/ES 3
  or a shader fails to build (`failed`). The browser test build (`SUMER_TEST` + Emscripten) draws Classic unless the
  page asks (`?gfx=high`), because the suites run in software GL; the native harness draws High.

Traps:
- The browser build needs WebGL 2: raylib is built with `GRAPHICS_API_OPENGL_ES3` (`build-web.sh` rebuilds an older
  raylib-web, marked in `raylib-web/.graphics`) and linked with `-sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2`. The
  shaders are written once and get `#version 330` or `#version 300 es` in front (`Compile`).
- Shadow depth is packed into an RGBA8 texture instead of a depth texture: WebGL 2 is fussy about depth texture
  formats and rlgl asks for an unsized one.
- Headless Chromium draws the shaders in software at about 1 frame a second: use `?gfx=classic` (the test build's
  default) for anything about game logic, and give screenshots long timeouts.
- `EM_ASM` blocks are macro arguments: a comma outside parentheses (`var a = 1, b = 2;`) breaks the build.
- In `ScenePass` the textures sit in units 10-13, out of rlgl's way; rlgl's batch (effects) keeps them bound.

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
- **Settings file** `sumer.music` (0 off .. 3 high) is written next to the saves and ignored by git; the sound effects'
  volume is `sumer.sfx`.
- Sound effects live in `sfx.c` (see "When a swing lands" above); keep the "no files, nothing to license" property.

## Ideas that were mentioned and not done

- Firefox works (the owner tested it); Safari is untested. No gamepad input. Touch controls cover only the main keys
  (the owner asked for move, attack, parry, use, crouch, look, first/third person, then jump and previous/next
  weapon; Bag and Menu were added so the panels and the pause menu can be reached): no chat, torch or map keys on touch.
- Graphics, later steps (the owner wants CC0-only art, a 25-50 MB download at most, and to weigh each effect's cost):
  buildings and props, then effects (wind on cloth and palms, light shafts, ambient occlusion, water reflections,
  sharper far shadows, grass and reeds). Animals are still boxes and blobs. Free model and texture sites (poly.pizza,
  kenney.nl, quaternius.com, opengameart.org, polyhaven.com, ambientcg.com) were still denied by the cloud sandbox's
  network policy in October 2026 (403 from the proxy), so nothing has been downloaded; the owner must allow those
  hosts in the environment's network settings first. Since then the owner opened them (poly.pizza still 403s); four
  CC0 textures went in as surface detail (see "Photo detail", then "Trees and fields"). Next by what is seen most: palm
  and tree models (Quaternius nature packs are CC0 low-poly with flat colors, which suits baking into the static
  meshes), then a second ground (grass) map. There is no model loader yet.
- More sound effects: footsteps, the bow, animals, ambience (blows, parries and arrows have theirs).
- Accounts cannot change their password or delete themselves yet, and a lost password cannot be recovered (there is
  no e-mail by design). The guest is trusted with its own things (records come from its game), so a changed page could
  give itself anything; experience is only shown, which is why that was left so.
- `web/` is rebuilt by hand; there is no CI. The desktop branch needs its own catching up if it is ever revived.
