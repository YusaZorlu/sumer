# Sumer

Open-world prototype set in Ur, c. 2100 BC. C99 + raylib, built from primitive shapes and lit by its own
shaders: sun and moon shadows, torchlight at night, brick, soil and water worked into the surfaces, sky,
fog and glow (see Graphics).

## Run

The game runs in a web browser, served by `server.py` (Python 3.8 or newer, nothing else to install):

    python3 server.py

It prints the addresses to open: `http://localhost:8000/` on this computer, `http://<this computer's
address>:8000/` for the others on your network (`--port 9000` picks another port). `web/` holds the built
game (`sh build-web.sh` rebuilds it; needs Emscripten) and `host/sumer-host` the game's server program
(built for 64-bit Linux; `sh build-host.sh` rebuilds it on any machine with a C compiler).

The desktop version (Linux and macOS, `run.sh` and the ready-built `sumer-prebuilt`) is kept on the
`desktop` branch, as it was on 20 October 2026, with the Sumerian characters. Work goes on in the browser
version only; the desktop branch is not updated for now.

## Accounts

The title screen asks you to **log in**, or to make an account on the **Create account** tab: a name (3 to
15 letters, digits and `_ - .`) and a password (6 characters or more), typed twice. The page's own fields
take them, so a password manager or a phone's keyboard works as anywhere else; Enter sends them. The
account stays logged in in that browser for 60 days, or until **Log out** (bottom right of the title).
**Your account** (top right of the title) changes the password (the current one first; other browsers
logged in to the account are logged out) or deletes the account, with everything the server keeps for it
(asked twice). **Privacy** (top right) opens `web/privacy.html`: who runs the game, what is kept and who
else handles it (netcup hosts the server, Cloudflare proxies it).

The server keeps only the account's name, a hash of its password (scrypt; never the password itself) and
the game: your level and K/D, your three singleplayer save slots, and your things in the survival servers
and in the PvP servers (one set for each kind, whichever server of it you play on). No e-mail, nothing
personal. A new account costs the browser a second or two of work (a proof of work), and each address can
make only a few a day, so nobody can make thousands of them; addresses are kept in memory only, never
written down.

### Levels

Every account has a level, 1 to 100, shown on the title screen and under your health in the game: level 1
takes 100 experience to reach level 2, level 2 another 200, level 3 another 300, and so on. Experience is
only shown, never spent, and the same level counts for every way to play:

- 1 for a beast or a townsman brought down, 3 for a guard, raider, barbarian or bandit;
- 10 for another player brought down (also counted in your K/D: players brought down / times brought down
  by a player);
- winning a battle royale round: 10 for every player the round began with;
- finishing a Sumer Party in the top three: 10, 5 or 2 for every player in it.

"+3 XP" rises over the crosshair as you gain it, and reaching a level is news.

## Ways to play

After logging in: **Sumer Survival**, **Sumer Royale** (the battle royale, below) and **Sumer Party** (five
minigames, below). Sumer Survival offers:

- **Singleplayer**: your own world, in three save slots. They are kept in the browser and with your
  account, so another browser logged in to it plays on from the same saves.
- **The servers**: the game's shared worlds, run by the server day and night whoever is in them. Each is
  listed with what it is and who is on it (or "nobody here; joining opens it": an empty server sleeps
  until someone joins). Up to six players each.
  - **Survival 1, Survival 2**: PvP off: players cannot hurt each other.
  - **PvP 1, PvP 2**: players can hurt each other with blows and arrows. Everyone fights with the best of
    everything (bronze sickle-sword, the war mace, the best bow with bronze arrowheads, bronze armor; your own
    things are untouched and kept), arrows never run out, someone brought down comes to with full health, and the
    guards stay out of fights between players (attacking townsfolk or guards still angers them). Everyone starts somewhere in the city (on open ground, away from the other players), and so does
    anyone brought down, instead of waking at home. The guards on the ziggurat's stairs and terrace are gone,
    so anyone can climb to the priest on top for the day's +20% speed boost: the welcome says so.

  Your things come with you from server to server of the same kind (the survival servers share one set,
  the PvP servers another), handed in every few seconds and as you leave; the world itself stays with its
  server. One account plays on one server at a time.

### Sumer Royale (battle royale)

**Sumer Royale** on the title screen, then **Royale**. Up to twenty players.

- **The map**: the same land with no guards anywhere (city, villages or gate towers), raiders, barbarians, bandits or caravans;
  the townsfolk, villagers, traders and wild animals (partridges too) stay. About 150 more trees stand out in the land. Houses
  are of no use: beds, storerooms, querns, troughs, workbenches and ovens do nothing. Nothing is kept
  between rounds: everyone starts each one with the best of everything (bronze sickle-sword, the war mace,
  the best bow with bronze arrowheads, bronze armor) and arrows that never run out.
- **A round** starts once everyone in (two or more) is ready: press Enter, Ready in the Esc menu, or the
  Ready button under the line at the top while the mouse is free (Enter again: not ready). Ten seconds later the round begins (the line at the top of the screen counts down;
  someone becoming unready, or joining, stops it). Anyone not ready who does not move for three minutes is
  sent back to the title screen; those who are ready are left alone. Each player is set down somewhere out in the land,
  240-420 m from the city and far from the others, with full health.
- **The sandstorm**: a wall of dust that closes in on the city in stages. It waits a minute, closes to
  380 m (4 minutes in), rests, to 190 m, rests, to 90 m, rests, and ends 25 m round the middle of the city
  13 minutes in. Inside it you lose health every second (2 at first, up to 12 at the end; armor softens
  it) and the screen turns dusty. The map shows its edge as an orange circle; the line at the top of the
  screen counts down to its next move and how many still stand of how many began. The map shows no other
  players.
- **Statues**: fifteen gypsum worshippers on brick plinths (five in the city, ten out in the land; gold
  marks on the map, a burning light over each while its blessing is there). Press E to pray; the first one
  there takes the blessing for the rest of the round: Ninurta's +5% attack speed (swings and drawing the
  bow), Enlil's +5% running speed, or Gula's +10% healing. Blessings add up.
- **Out**: whoever is brought down is out for the round and watches someone still in it (click or Space:
  the next one). Someone who joins while a round is on watches too. The last one standing wins, is
  congratulated, and gains 10 experience for every player the round began with; the next round starts
  when everyone is ready again.
- The Esc menu lists everyone in two columns with their kills and who is out.

### Sumer Party (minigames)

**Sumer Party** on the title screen, then **Party**. Two to twelve players.

- **The map**: the city and its lands with no guards and no caravans. Food stays full, houses are of no
  use (more players than houses), and nothing is kept: everyone plays with the party's own gear (the best
  of everything, unlimited arrows) and anything gathered stays in the party. The streets are busier than
  usual: three times as many townsfolk walk them. The townsfolk and villagers never fight back or
  raise the alarm in a party: arrows fly about in its games.
- **Starting**: as in Sumer Royale, everyone in (two or more) presses Enter, Ready in the Esc menu or the
  Ready button under the line at the top (shown with the vote's while the mouse is free, as you join and on
  a phone), and ten seconds later the party begins. Anyone not ready who does not move for three minutes is sent away.
- **The vote**: while waiting, the Esc menu's **Vote a game out** (V) lists the five minigames. Each player
  may vote one out (and take the vote back); the one with the most votes is left out (a tie, or no votes:
  one of them by chance) and the other four are played in a random order.
- **The games**: four minutes each, with half a minute back in the city between them. The line at the top
  says which game is on, its rule and the time left; the board at the right always shows the best three
  (and you, if you are not among them) with this game's points and the party's total. Each game's top
  three gain +10, +6 and +3 more. Someone brought down is back in the game 5 seconds later and keeps what
  they carry. Someone who joins during a game watches and plays from the next one, with 0 points.
- **The end**: after the fourth game the party's top three gain experience (10, 5 and 2 for every player
  in it), and the scores start again from nothing for the next party.

The five minigames:

- **Floor is lava**: everyone starts on a roof, apart. Anything lower than half a metre above the ground
  is lava (shown red, in this game only) and burns 8 health a second. Players can fight: +6 for bringing
  someone down, -2 for being brought down; back on a roof 5 seconds later.
- **Date stashing**: gather dates (palms, bodies) and keep them: at the end, a point for every 5 dates you
  carry, 30 at most (the board counts them as you go). Players can fight; whoever is brought down keeps
  their dates but comes back in the city.
- **One-shot archers**: bows only, and one arrow brings anyone down. +6 a kill, -2 a death.
- **City defence**: every raider, barbarian and bandit comes at the city from just outside its walls,
  after the nearest player; one brought down is back outside the walls a minute later. With fewer than four
  players the raiders stay at their camps and only the barbarians and bandits come. +2 for each one you
  bring down. Players cannot hurt each other.
- **Hide and seek**: one player (by chance, never the same twice in a row) seeks with the party's gear;
  the others hide among the townsfolk, who hurry about the streets. Hiders look like townsfolk, wear no
  armor, carry nothing, cannot run, cannot hurt the seeker, and lose health outside the city's walls;
  nobody's name shows over their head, the map shows no players, no one carries a torch and the fight's
  music never plays (it would say who is near). One blow or arrow from the seeker
  finds a hider, who is sent to the village farthest from the city to hold out against the wild beasts
  until the game ends. A hider gains a point for every half minute hidden (8 at most) and 6 more for never
  being found; the seeker gains 6 for each hider found, but loses the game by bringing down 5 townsfolk.
  The game ends early once every hider is found.

### Games at the alehouse

In survival, PvP and single-player games (not in battle royale or a party) the alehouse has three game tables.
Walk up to a stool and press E to sit; play another player who sits across, or press Enter (or the button) to
play a townsman. Both games use the same dice, cubes numbered 1 to 6. Space throws. The pieces you can move then
glow: point at one (on a phone, tap it) to see the squares it would go through and where it lands, marked
"Throws again", "Takes" or "Off" where that applies; click it (on a phone, tap it again or tap where it lands) to
move it. The keys 1-7 pick the glowing pieces from left to right. Your way round is numbered square by square
with arrows between, pieces walk to their square, the last move stays marked by an arrow for a few seconds, and a
line beside the dice says what it did ("The townsman moved 3 squares onto a rosette and throws again"). E or Esc
steps away from the panel while you keep your place, and getting up (the button, or walking off) gives up the
game. Look on at a full table with E. Rules (R, or the button) shows the game's rules in short in place of the board.
The only stake is experience for the winner: +20 for beating a player, +10 for beating a townsman. Getting up
from a game against a player loses it only once twenty moves have been made, so a quick sit-and-stand earns
nobody anything; if you sit and do nothing for 45 seconds, the game plays your turn for you.

- **The Royal Game of Ur** (one table downstairs, one upstairs). Boards like it were found in the Royal Cemetery at Ur (about 2600 to
  2400 BC); the rules follow Irving Finkel's reading of a Babylonian tablet of 177 BC, thrown with one die here.
  Each side has seven pieces. A piece enters at square 1 on your own row, runs four squares, then eight down the
  shared middle row (5 to 12), then 13 and 14 back on your own row, and goes off with any throw that reaches past
  14. Landing on an opponent in the middle row sends it back to the start; you cannot land on your own piece. The
  rosettes (squares 4, 8 and 14) give another throw, and a piece on the middle rosette (8) is safe. First to bring
  all seven off wins.
- **Fifty-eight holes** ("hounds and jackals", the other table downstairs). Boards of this game are known from Egypt to
  Mesopotamia from about 2000 BC; the rules are lost, so these are a modern reconstruction. Each side has five pins
  and throws two dice, adding them. Pins go up your own side's 29 holes to the shared goal; any throw past 29 brings
  a pin home. Lines join hole 6 with 20 and 8 with 10: land on 6 or 8 and the pin goes up the line (the green end),
  land on 20 or 10 and it goes down (the red end). No two of your pins share a hole. First to bring all five home wins.

### Playing together

- Each player has a house of their own and wears its color: blue for the first in, then red, green,
  yellow, purple and teal. The houses stand by the farm and in a row south of the east-west street, doors to the
  street, each with its own bed, storeroom, quern, trough, workbench and oven, under a flat roof (inside, the camera stays under it). The farm, the coop, the
  hens and the eggs are shared.
- The world never stops: the Esc menu only takes your hands off the keys, and time can't be
  fast-forwarded (T).
- T opens a chat line: type, and Enter sends it to everyone (Esc puts it away, keeping what you typed).
  The last lines stay at the bottom left for 15 seconds, each name in its player's color, and all of
  them show again while you type. The server passes every line on: up to 100 characters, two a second.
- The night passes when every player lies in bed (E at your bed; E, Space or a step gets you up).
  Someone brought down comes to at home at once (somewhere in the city in a PvP game), with 30% health,
  and the message says who did it (a player by name).
- Wolves hunt whoever is alone outside the walls; another player close by counts as company. Guards go
  after whoever attacked someone in the city's lands, raiders after whoever comes near.
- A carcass, a tree's wood, a plot or the eggs go to whoever gets there first.
- Names float over the other players' heads, with their health when they are hurt; the map shows them
  as dots in their colors.
- With PvP on, every player's score shows over their head, in the Esc menu's list of players and (your
  own) under your health: "K/D 3/1 (3.0)", players brought down and times brought down by a player.
  Anyone wanted by the guards (for attacking townsfolk) shows how long the search still lasts. When a player brings another
  down, everyone else hears of it.
- Blows between players are seen and felt. A swing at another player takes 0.3 seconds to land
  (a person's takes 0.55), and for that moment a yellow mark shows over the swinger; raise your
  melee weapon toward them in time and it turns green (turned aside: they are stunned a moment and
  your quick answer hits a quarter harder), otherwise it turns red as it lands. A player who is
  hit flinches, flushes red and the view jolts; red glows on the edges of your screen show where
  the blow came from (top ahead of you, bottom behind, left and right), for blows from people,
  animals and arrows too. Other players swing, raise and sweep their weapons as you do yours, and
  lean into the blow.

## Running the game's server

`server.py` serves the game, keeps the accounts and runs the servers listed on the title screen. Each
server's world runs in a program of its own (`host/sumer-host`), which `server.py` starts when the first
player joins it and stops once it has stood empty five minutes (`--idle`), saving its world first; a
survival or PvP world carries on from there next time. One such program takes about 5 MB of memory and a
few percent of one core with six players in it.

    python3 server.py --port 8000 --bind 127.0.0.1 --data /opt/sumer/data

- `--data`: where the accounts (`sumer.db`, SQLite; a copy in `backups/` each day, the last seven kept)
  and the servers' worlds (`rooms/<id>/sumer-mp.sav`) are kept. By default `data/` next to `server.py`.
- `--rooms`: the servers, as `id:kind:Title` separated by commas, kind `survival`, `pvp`, `royale` or `party`. The
  default is `survival-1:survival:Survival 1,survival-2:survival:Survival 2,pvp-1:pvp:PvP 1,pvp-2:pvp:PvP 2,royale:royale:Royale,party:party:Party`.
- `--max-active` (default 6): how many servers may run at once. On a small machine set 2 or 3: the first
  ones to open run, and the rest show "cannot open now: all servers busy" until one empties.
- `--host-bin`: the game's server program, if not `host/sumer-host`.
- `--work` (default 18): how many zero bits the proof of work for a new account must find (each one more
  doubles the work).
- `--test`: for the test suites only (no work, no limits, rooms made on demand, `/test/...` requests).

Behind a proxy (Caddy, Cloudflare) `server.py` takes the player's address from `CF-Connecting-IP` or
`X-Forwarded-For` for its limits, and marks the session cookie Secure when the page came over HTTPS. The
browser and the server program must be built from the same sources: a page from another version is told
to reload. To change a server's world for good, stop `server.py` and delete `data/rooms/<id>`.

## Graphics

Four settings, switched with **G**, the "Graphics" button in the Esc menu or on the browser's title screen,
and kept in `sumer.gfx` next to the saves:

- **Auto** (the default): High on a computer, Low on a phone or tablet, and Low for the rest of the session
  on a computer where High cannot keep up (most frames slower than 24 a second for six seconds; it says so).
- **High**: everything is lit on the graphics card. The sun casts soft shadows (the moon at night); torches,
  fires and the guards' hand torches light what is round them, with a flicker; walls show their mud bricks
  and patches of plaster, the ground its grain, wood its grain, and water ripples and mirrors the sky and
  the sun. Sky with drifting clouds, stars and the moon at night, haze in the distance, rain that darkens the
  ground and makes it shine, a glow round fire and bright light, smoothed edges. Round things (people,
  trees, columns) are shaded round.
- **Low**: the same look made cheaper for phones: a smaller picture scaled up, sharper-edged shadows over a
  smaller area, no clouds or glow, fewer torches at once, buildings far away left out sooner.
- **Classic**: the old way, colors lit on the processor. It also runs by itself where the shaders cannot
  (OpenGL 2.1, or a browser whose graphics refuse them).

The browser version needs WebGL 2 (any current Chrome, Edge, Firefox; Safari 15 or newer). Adding `?gfx=high`,
`?gfx=low` or `?gfx=classic` to the page's address overrides the setting for that visit.

**On a phone**, held sideways: the title screen, the HUD and every panel shrink to fit a short screen and are
drawn with the screen's own pixels, so the letters stay sharp; taps work on every button. Touch controls,
see-through so the world shows behind them, cover the main keys:

- **Moving**: the stick under the left thumb (it starts wherever the thumb comes down on the left half of the
  screen); pushed to its edge you run.
- **Looking around**: drag a finger anywhere else (aiming the bow too, while the other thumb holds Attack).
- **Attack** (strike; hold to draw the bow, let go to shoot), **Parry**, **Jump**, **Use** (E) and **Crouch** under
  the right thumb. Only Attack strikes: a tap anywhere else is looking, never a blow.
- **< Weapon >** beside the map: the previous or next of hands free, bow, melee weapon and war mace.
- **Bag**, **View** (first person or behind you) and **Menu** at the top; with a panel open the Menu button reads
  **Close** and puts it away.
- **Your name** on the title screen: a tap on the field opens the browser's box, and with it the keyboard.
- **Baskets**: a tap eats, drinks, chews a herb or refills the quiver; a finger held on a basket for two
  seconds (a red bar fills along it) drops what is in it.

On a phone the game never takes the pointer and never pauses by itself (so touching the screen, in Firefox too,
cannot open the pause menu); Menu pauses. A touch also puts the game in full screen, hiding the browser's bars
(Android browsers; leave it with the back gesture, the next touch brings it back). With the bow drawn, sliding the
finger that holds Attack aims. In Game settings, **Buttons: left hand** puts the thumb's buttons on the left and
the stick on the right.

The rest (torch, map, chat, music and graphics keys) needs a keyboard, or the buttons in the Esc menu and the bag.
The controls show on a phone or tablet, and on any other screen once a finger touches the game.

## Controls

| Key | Action |
|---|---|
| WASD, Shift | Move, run |
| Space | Jump (stands up if crouching) |
| C | Toggle crouch: slower, much harder for animals to notice |
| Mouse / arrow keys, wheel | Look, zoom (zoom all the way in for first person) |
| P | First person on/off |
| 1 / 2 / 3 / 4 | Bow / melee weapon / war mace, once you own one / hands free (also buttons in Tab) |
| Left mouse | Bow: hold to draw, release to shoot (the camera follows the arrow). Melee or mace: strike |
| V | Strike with the melee weapon |
| Right mouse or Q | Parry with the melee weapon or the mace |
| Z | Arrow camera on/off |
| E / F | Use, talk, pick up, climb a ladder, search a body, fill or pour a bucket, open the coop, feed a cat or a dog / alternate action (plant a date). E also closes what it opened (barter, storeroom, coop, a game board) |
| R | Refill quiver from carried arrow baskets |
| B | Torch on/off (it burns from 17:00, brighter as dusk turns to night; also a button in Tab, see Combat) |
| Tab | Baskets (inventory), equipment and armor |
| T (hold) | Fast-forward time (playing alone) |
| T | Chat, in a shared game or a battle royale (the dead watching too): type a line, Enter sends it to everyone, Esc puts it away |
| Enter | Battle royale and party, between rounds: ready / not ready |
| F5 | Save |
| F11 or Alt+Enter | Borderless fullscreen on/off |
| M | Map zoom (whole world, x2.5, x6 around you); the map shows the main roads, both villages and the camps |
| N | Music volume: off, low, medium, high (also a button in the Esc menu and on the title screen) |
| G | Graphics: auto, high, low, classic (also a button in the Esc menu and on the title screen) |
| H | Controls and tips window |
| Esc | Pause / close a panel (pause menu: save slots, game settings, your look, save, new game, save & quit) |
| S (in the pause menu) | Game settings: language, graphics, music, sounds, arrow camera, first person or behind you |

Panels also take number keys (barter offer N, with 0, - and = for offers 10-12; use basket N; coop actions
1-4), so nothing depends on a click landing. The game pauses when the window loses focus (a screenshot tool, switching apps);
Resume or Esc takes the mouse back.

In a browser only Esc gives the mouse back. A window opened over the game (Tab, barter, the storeroom, the coop, a
game board, chat) keeps it and shows the game's own arrow, which the mouse moves; closing the window with Tab or E goes
straight back to looking round, with no click. Esc in a window closes it and gives the mouse back (a click takes it
again); Esc in play opens the pause menu.

## Game settings

Esc, then "Game settings" (or S): language, graphics, music, sounds, the arrow camera (the view follows each
arrow), the view (first person or behind you), the frame limit (60 a second at first; 30 keeps a phone cooler,
"none" runs as fast as the screen shows frames) and, on a touch screen, which hand the buttons are for. They are kept in this browser and change nothing about how
the game plays. The keys L, G, N, Z and P do the same while playing.

## Language

English and Turkish. In the pause menu (Esc), the "Language / Dil" button (or L) switches between them;
the choice is kept in `sumer.lang` next to the saves (the music volume is in `sumer.music`, the graphics setting in `sumer.gfx`, the sounds' volume in `sumer.sfx`). Türkçe için Esc menüsündeki "Language / Dil"
düğmesine (ya da L'ye) bas.

## Music

**Music of places.** In the game, a recorded track plays for the part of the map you are in, faded across
as you go from one to another: the city (hand drums), a village (a plucked melody), the open land (a sparse
tune over the wind, a tense one, so it plays there only from dusk to the end of dawn, 17:00 to 08:00; by day the
game's own main theme plays in the open land) and the halls of the ziggurat (a slow drone). They are CC0 (public domain) tracks from
OpenGameArt, listed in the credits below, in `assets/music/` (4.7 MB). When a fight starts the game's own
fight tune takes over, and the place's track comes back after it.

The game's own tunes are made by a small synthesizer in `src/music.c` (no samples or recordings): the
morning tune on the title screen, the fight tune, and the night tune (heard in a game only if the music
files are missing). Press **N** (or use the button in the Esc
menu or on the title screen) to step the volume through off, low, medium and high; the choice is kept in
`sumer.music` next to the saves. It starts on medium.

| Tune | Plays | Sounds like |
|---|---|---|
| **Morning in Ur** (72 beats a minute) | By day, and on the title screen | A reed pipe singing over a low drone, a lyre picking a steady pattern of notes, a soft frame drum; calm and open |
| **Raiders at the Gate** (148) | While something is out to hurt you | Kettle drums and a rattle driving a gallop, an animal horn, a shrill pipe and a low plucked riff in a scale with a flat second; urgent and rough |
| **Stars over the Euphrates** (50) | From dusk to dawn, and while you sleep | A slow pad of open fifths, a soft harp picking five notes with long silences, a distant flute, lots of echo; relaxed |

The fight tune starts when a hostile guard, villager, raider, caravan guard, boar or wolf pack within
50 m is after you, when you are hit, and (in a game with PvP) when another player nearby is swinging a weapon or
aiming, or anyone is within 60 m in a battle royale. It carries on for 12 seconds after the last such moment, then
the day or night tune fades back in; being brought down ends it at once. Every player's game decides this
for itself from what it already knows, so in a shared game each person hears their own fight, and there is
nothing extra to send over the network.

**How it is made.** The scales are built the way the Old Babylonian tuning tablets describe lyre tuning,
from pure fifths and fourths (a whole tone is 9/8 and the small step 256/243), so the intervals are the
old Pythagorean ones, not the piano's: morning uses a seven-note scale like Dorian, the fight one with a flat second
and sixth, the night a five-note scale with no thirds. The instruments are a plucked string (the lyre of Ur), a reed pipe,
a flute, an animal horn, a frame drum, a big kettle drum, a rattle, clapping and a drone, with a little room echo. Each tune is a
handful of "parts" (an instrument and a score written in a tiny text notation); every part
loops on its own length, so the whole tune takes about two minutes to come round again. The tunes are written
for this game, and no one knows what Sumerian music really sounded like: no Sumerian melody survives, and the
oldest written tune (the Hurrian hymn, about 1400 BC) is much later and read in several ways. These are imagined, in
the spirit of what is known, not reconstructions.

**Changing the music.** The tunes are the three tables of parts near the top of `src/music.c`, with the notation
explained above them (scale degrees with `'` and `,` for octaves, `:beats` for length, `-` for a rest, `1+5` for a
chord, letters for drums). `sh tools/music/render.sh` renders all three to WAV files and spectrograms and prints their
loudness, so a change can be checked without playing the game; `robot.md` explains the rest.

### Credits and licence

The music is original to this project: no existing recording, melody or sample is used, so it carries no outside
copyright, and it may be used, changed and shared along with the rest of the project. It was written with these in mind, as
background reading (nothing is copied from them): the Old Babylonian tuning texts on clay tablets from Ur and Nippur
(for example UET VII 74 and CBS 10996); the lyres from the Royal Cemetery of Ur (about 2500 BC); the Hurrian hymn to Nikkal
(about 1400 BC); A. D. Kilmer, R. L. Crocker and R. R. Brown, *Sounds from Silence* (1976); M. L. West, "The Babylonian Musical Notation and
the Hurrian Melodic Texts", *Music & Letters* 75 (1994); and R. Dumbrill, *The Archaeomusicology of the Ancient Near East*.
raylib (zlib licence) draws the graphics and opens the audio device.

Surface detail comes from four photo textures from [Poly Haven](https://polyhaven.com), all **CC0** (public domain, no
attribution needed, free for any use): `dry_ground_rocks` (the ground), `white_sandstone_bricks_03` (mud-brick walls),
`wood_planks_dirt` (wood) and `rock_boulder_dry` (rock). The game keeps its own colors and takes only their light and dark,
packed into `assets/detail.png` (512x512, 0.8 MB) by `python3 -I tools/assets/make_detail.py`.

People's clothes and the roads take their grain from four textures from [ambientCG](https://ambientcg.com), all **CC0**:
`Fabric019` (woven cloth), `Carpet016` (fleece skirts, pelts and felt), `Leather037` (leather armor, caps and
sandals) and `Pathway004` (the roads' packed earth), in `assets/detail2.png`. The grain moves with each body, so
it does not slide over people as they walk. Torch and fire flames are two frames from Kenney's
[Particle Pack](https://kenney.nl/assets/particle-pack) (**CC0**), in `assets/flame.png`.

The music of places is four **CC0** tracks from [OpenGameArt](https://opengameart.org), made smaller and
even in loudness by `sh tools/assets/make_music.sh` (curl, ffmpeg): the city plays "Darbuka Delight" by
Adiutorium, a village "Hyyoshi Arabian Style Background Music RPG" (submitted by Tozan), the open land
"Caught in the Desert Plains" (Tozan) and the ziggurat "Egyptian meditation music" by HaelDB.

Trees and fields: `palm_tree_bark` (bark on every trunk), `forest_leaves_02` (leaves and shrubs) and
`dry_mud_field_001` (the tilled earth of the fields and garden plots) from Poly Haven, and `ThatchedRoof001A`
(straw: the standing crop, stubble and sheaves) from ambientCG, all **CC0**, in `assets/detail3.png`. The same
script builds all of them. Everything else is drawn in code.

The game boards' wood frame is `wood_table_001` from Poly Haven (**CC0**), darkened into `assets/board.png` by
`python3 -I tools/assets/make_detail.py board`.

## Where things are

The city follows the plan of Ur around 2000 BC: an oval mud-brick wall with towers, the temple precinct
(temenos) with the ziggurat, a harbor basin fed from the Euphrates, canals around the walls, and barley
fields outside. Gates: south (main road), east, west, north, plus the harbor water gate. The map is
1080 m across; the city is about 200 m by 170 m.

- Home (blue lintel): bed, storage jars (a bucket is in there), quern, kneading trough, oven, workbench.
  Five more houses like it, with red, green, yellow, purple and teal lintels, stand south of the east-west
  street for the other players of a shared game
- Farm east of home: 15 plots, a water ditch along its north edge, a well west of it. Hen coop
  behind the house. Cattle herder at the pen east of the farm
- Four neighbours inside the walls have small farms; their wheat follows the seasons
- Ziggurat: three stages. The ground floor holds the market hall (bowyer, farmer, basket weaver) and
  two storerooms. Three stairways meet at a gatehouse on the first terrace; its guards let you up only
  after a food offering (2 bread, 2 roast meat, 2 grilled fish, 4 eggs or 2 milk), and the
  way stays open until dawn. Behind it: the priests' halls inside the second stage, then the shrine of
  Nanna on top, where the priest blesses you once a day: full health and, for 24 hours, running and
  attacking 20% faster
- City houses: every one roofed. Some have a cloth awning over the door (high enough to walk under, and
  solid: drop onto it from the roof), jars and baskets by it. The roofs are for running across: many houses
  have mud-brick stairs up one side or a crate by a pile of crates to climb from, and a room up on a roof
  has a crate beside it to get on top of it; beside some such rooms a reed sun shade stands on four posts,
  a step up from the room's top and solid to walk on. Anyone after you (guards, raiders, barbarians) comes
  up the stairs of houses and gate towers too
- Craft quarter (west, by the harbor): potters, kiln, smelter, smith
- Caravans walk the southern trade road and set up stalls in the temple courtyard for a while; each
  travels with two spear-armed guards
- Main roads (drawn on the map): the southern trade road; the east road from the east gate through the
  hills to the hunters' village; the herders' road, which leaves the trade road south of the fields and
  crosses the downs to the herders' village; the north road from the north gate to the ford; the road
  from the west gate down to the river's old landing stage; and tracks along the west bank. A few travellers walk them day and night
  (with torches after dark), and wolves leave you alone while you walk in their company
- Hunters' village, in a basin beyond the eastern hills: hide tents, drying racks, a smoking rack, a
  gazelle hung from a tripod to be skinned, a reed windbreak, logs to sit on round the fire, an archery
  butt, and people at work by the fire. The hunter butchers game better than your oven and trades pelts, meat, arrows, a large
  quiver and the composite bow
- Herders' village, on the downs south-west of the city: mud houses, a walled yard with a clay bread oven
  and dung cakes drying on its wall, beehive granaries, baskets and jars by the doors, someone baking and
  someone shearing, and a sheepfold with its flock. Cows and sheep (the city farm's and the flock) come to
  no harm, but you cannot walk through them.
  The shepherd is the only one who sells sheepskin and sheep, and takes plant waste as fodder
- Fishers' village, across the river from the city: reed houses, a reed windbreak, fish drying on racks,
  nets hung on poles, a stack of cut reeds, boats drawn up on the bank, people mending nets, and good fishing in the river just below. The fisher sells the only fine
  fishing net (8 reeds and 2 copper ore), which doubles every cast (4 fish instead of 2), and trades
  fish: 3 fish for 4 dates, 3 grilled fish for 3 fish and a piece of wood, arrows, herbs, or bronze
  for 8 fish
- Every village keeps a guard with a hunting bow, always in hand. When the village turns on you, its
  guard shoots at you from where it stands. City guards with bows carry them slung on the back and take
  them in hand when the one they chase is out of a spear's reach
- Eastern hills: copper mine (see below) and woodcutters' forest; more forest along the river, in the hills and
  around the hunters
- Gazelles south-east and around the hunters' basin, onagers north-east, boars in the south-west marsh
  and the eastern woods, geese on the river
- Wolves roam everywhere outside the walls in packs of four on the city side, six across the river;
  where the packs start is different every game. A den sits by the ford at the far north end
- A few snakes lie hidden in long grass outside the walls (river meadows on both banks)
- Two raider camps on the city side of the river: one beside the southern trade road, one to the north
- Two bands of bandits roam the eastern hills, one south-east and one north-east, far from the city and
  the villages
- Water is waded: a canal or the harbor slows you by 60%, the big river's current by 85% (your head stays
  above the water). Bridges, piers and the harbor quays are walked at full speed. There is no ferry any more; the old landing stages stand on both banks west of the
  city. A shallow ford at the far north end can be walked at full speed, through wolf country
- The west bank is green, rolling hill country: woods, flowering meadows rich in herbs, a reed-fringed
  pond with fish. Gazelles, onagers, boars and two wolf packs live there, and two barbarian war camps,
  north-west and south-west
- Ladders lead up to the wall walk at seven points around the city
- The towers either side of each gate are hollow: a door from the city at the foot (jars, sacks and a torch
  inside), stairs round the inside walls, a middle floor level with the wall walk where a guard sits at a table
  by the spears (its door opens onto the wall), and a roof with a brazier where an archer keeps watch. The
  archers stay up there and shoot at anyone the guards are after
- Guards: two at the temple gate and two inside each of the city's five gates (south, north, east, west and
  on the quays of the harbor's water gate), more on patrol in the east and west quarters and about the
  temple; clerics in the temple courtyard and on the ziggurat. A PvP game only takes away the three on duty
  on the ziggurat's first terrace; every gate keeps its guards, and the two off duty in the ziggurat stay
- Inside the ziggurat: the market hall with its three traders on the ground floor, the granary (heaps of
  barley, a storekeeper and a porter) and the treasury (a balance for weighing metal, the treasurer) in
  its wings. On the middle terrace the guards' room (cots, a rack of spears and shields, two guards at
  supper), the tablet house (shelves of clay tablets, scribes at work) and the sanctuary, where
  worshippers kneel before the god. The priest's shrine is on top
- The big fields outside the walls have earthen banks round them, a feeder ditch with a shaduf (the
  water-lifting sweep) and sheaves at the corner; garden plots are tilled in ridges under the rows
- Trees have lumpy, clumped crowns, lit leaf-cluster by leaf-cluster, with bark and leaf grain
- Out on the land: feathery tamarisk by the water, low camelthorn on the dry steppe, fallen limbs and
  stumps of dead wood, flat stone slabs and scatters of stones on the rises. None of it is in your way
- The alehouse, beside the potters' quarter in the west of the city with its door on the street (a reed
  awning and two jars mark it): a long hall of two floors, with stairs up to an upper room of rugs and low
  tables. Two long tables where townsfolk sit and talk, a counter of big jars, lamps on the walls, and
  three game tables (two below, one upstairs). The oldest such place we know of stood in the potters'
  district of Lagash, about 2700 BC. Weapons are put away as you step in (you cannot take them up inside)
  and come back to hand as you step out, and nothing from outside can hurt you in there: no arrow, blow or
  bite from the street reaches a player inside
- Torches line the streets, gates and temple, lit from five in the evening and brighter as the dusk turns to night; guards and caravans carry torches
- At night distant things fade into the dark; torchlit places stay visible
- Townsfolk walk all over the city: along the streets from crossing to crossing, keeping to the right,
  stopping now and then, and turning back at the gates and the quay
- People are solid: you can't walk through anyone, and townsfolk step round you, or turn back if you
  keep blocking their way

## Seasons and farming

Each season lasts 9 days: spring, summer, fall, winter (day 1 is the first day of spring). The HUD
shows the season and day.

- Wheat ripens after 10 days with water and gives 4 wheat and 2 seeds. It can't be sown in winter or
  when fewer than 10 days are left before winter (in practice: until the 8th day of summer).
- The farm takes one date palm; a date pit grows into a fruiting palm in 2 days with water. Every palm
  gives dates again a day after picking. Nothing grows in winter, and palms bear nothing then.
- The farm holds 6 days of water. Take the bucket from the storeroom, fill it at the well or the river
  (E), and pour it at the clay basin by the farm ditch (E): each bucket adds 2 days. You get a warning
  at 2 days left and when it runs dry; without water nothing grows.
- Rain fills the farm and the coop to the full 6 days. Chance of rain each day: 20% in spring and fall,
  1% in summer, 35% in winter (winter rain fills, but nothing grows until spring). Rain that falls while
  you sleep or rest still fills them, and you are told when you wake. A rain icon and "Raining" show in
  the HUD while it rains.
- The fields outside the walls and the neighbours' farms follow the seasons: green in spring, ripe in
  summer, cut in fall, bare in winter.
- Six dry days kill growing wheat (palms survive). Clear a withered plot with E for plant waste; the
  shepherd in the herders' village takes it as fodder (2 for a jug of milk).
- Spare seeds sell to the cattle herder (6 seeds for a jug of milk); wheat sells to the cattle herder
  (cowhide), the shepherd (sheepskin), the farmer and the caravans.
- The basket weaver makes more buckets (6 reeds).

## Hens

Up to 5 hens; the farmer sells them. E at the coop opens the coop panel: water, feed, eggs, and each
hen's age, life left and next meal.

- Each hen eats 1 feed every 3 days and lays an egg a day while fed and watered. Seeds give 1 feed,
  wheat 2 (up to 30 stored).
- The coop holds 6 days of water like the farm; pour a bucket there too.
- A hen lives 12 seasons (108 days) from the day you get it. It dies after 6 days without food or
  6 days without water.
- After a season a hen can be killed for meat; the button asks twice. The oven roasts it into 2 roast
  meat.

## Food and health

Everything costs food: idling and walking little, running more, and work by how hard it is. Picking
dates is free; all other gathering costs food, and the prompt shows how much. Mining copper costs the
most (16), twice what cutting wood costs (8), then digging clay (2), smithing and forging (2-3), and
fishing (1.5), cutting reeds and gathering herbs (1) the least. Heavy work (2 food or more) is refused
when you are too hungry for it.

Health comes back on its own while your food is at 20% or more and nothing is attacking you. If you are
brought down you wake at home the next morning with 30% health and the same food, and you lose your
carcasses and everything you gained that day that you had not put in your storage at home: dates,
ore, wood, bought goods and the rest. What you carried when the day began, and anything you took out
of storage, is kept (pelts too). The message says what brought you down and what you lost.

Herbs grow in green places: the river meadows, the pastures around the herders, the hunters' glade,
and above all the west bank (look for low sage-green clumps with purple, yellow or white flowers).
Gathering one gives 2 herbs; the plant grows back in a day and a half. Click herbs in your baskets
(Tab) to chew one: +10 health at once, then 2.5 health a second for 20 seconds, even in a fight and on
an empty stomach (the HUD shows "Herbs" with the seconds left). Herbs also barter: the farmer gives
seeds, the hunter arrows, the shepherd sheepskin and the Dilmun merchant tin.

## Saving

Three save slots: `sumer.sav`, `sumer2.sav`, `sumer3.sav`, kept in the browser's storage. The game saves to the slot in play when you sleep,
when you quit, and on F5, and loads the last slot used at start. The pause menu shows each slot's day
and time: "Switch to this" saves the current game and loads that slot; on an empty slot a new game
begins there. "New game" (click twice) erases only the slot in play.

In the browser the slots live in the browser's storage for the game's address (a game played alone
there is also saved every two minutes), and every save is sent to your account on the server too; on the
title screen the account's saves come back into the browser, so you play on from them anywhere. Saves
made in a browser before accounts go to the first account logged in there. The servers' shared worlds
are kept by the server (`data/rooms/<id>/sumer-mp.sav`), your things in them with your account.

Each file is a few KB at most: day and time, your position, health and food, baskets, storage,
quiver, bow, tools, weapon, war mace, armor, temple access, blessing, farm plots and water, coop and
hens, weather, hunted animals and regrowing resources. Saves from earlier versions still load (the old
3x3 farm keeps its rows in the wider field, old hens start a fresh life); saves from before the bigger
map keep everything you carry and own, but forget which animals were hunted and which trees and pits
were regrowing, since those belonged to the smaller world. The world itself is rebuilt from a fixed
seed, so it is never stored.

## Combat

- Arrows leave a glowing trail; where an arrow lands you get a marker with the distance for 3 seconds.
- Animals show health bars once they notice you or are hurt. Shots at unaware animals do double damage.
- Melee weapon: Wooden club (14), Copper mace (24), Bronze sickle-sword (38), one strike a second.
  Forge upgrades at the smelter in the craft quarter with F: 3 copper ore + 2 wood, then 3 bronze + 2 wood.
- War mace (key 3): the smith sells one for 2 bronze, 3 copper ore and 4 wood. Each blow (60, one every
  1.4 seconds) sweeps a wide arc and strikes every animal and every foe in front of you, which helps
  against a wolf pack or a band of raiders. A bystander is only hit if the blow was aimed at them. It
  is too heavy to parry with: take up your melee weapon (2) for that.
- Bow damage: 20 / 28 / 40 by bow, +10 with bronze arrowheads. Arrows strike where the crosshair
  points, near or far, and over the shoulder too: the archer allows for the arrow's drop. While the
  bow is drawn you stand still (players and archers alike); loose or lower it to move again.
- Parry: before a person strikes, a yellow mark shows over their head. Press right mouse or Q with
  your melee weapon in hand while facing them: the mark turns green, the blow does nothing, and they
  are open for a moment (your next hit does a quarter more). If it lands, the mark turns red. There
  is no shield yet. Other players give the same warning, over their head, with less time to react
  (see Playing together).
- Only what is in your hand is drawn and used: 1 bow, 2 melee weapon, 3 war mace, 4 hands free.
- **Your torch** (B, or the button in Tab): with bare hands, the sword or the mace it rides in your other
  hand and burns from five in the evening, brighter as the dusk deepens, lighting the ground round you. Turn it off if you like. Drawing the bow needs
  both hands: the bow draws the moment you press, the torch drops from your hand at once, and it is back a
  second after the shot. Lighting it or putting it away with B takes a second, and so does changing weapons. A burning torch gives you away to animals at night: wolves notice you
  from about half as far again, and gazelles, onagers and geese up to twice as far as in the dark without
  one. Other players see your torch and its light.
- A swing lands when the weapon comes down on its target, 0.3 seconds after you press: you raise it,
  bring it down hard, and the blow meets whatever is in front of you at that moment (the war mace
  sweeps across and meets everything in front as it passes straight ahead). People's own swings and
  spear thrusts come down, or out, the moment their blow lands on you.
- A blow that lands is felt: your swing holds still for a split second where it struck, the view
  kicks (harder with the war mace), blood and dust fly, whoever was struck flashes pale and is driven
  back a step, and you hear it thud. A swing leaves a pale streak through the air; a blow turned aside
  rings like bronze on bronze and throws sparks; one you take thumps. Arrows crack as they strike.
- Sounds are made by the game itself, like the music (no sound files). "Sounds" in the Esc menu and on
  the title screen sets their volume (off, low, medium, high) apart from the music's; it is kept in
  `sumer.sfx`.

## People, crime and raiders

Everyone has a health bar. People are tough: a townsperson takes about 9 club blows, a guard
(armored, 320 health) about 35 club blows or 13 with the bronze sickle-sword. An unaware target takes
half again as much damage.

Nobody attacks you unless you attack first. What happens next depends on where you are:

- **In the city's lands** (everything inside the ring of canals): if others see you hurt someone who
  wasn't fighting you, guards and the braver townsfolk come for you, the rest run, and the guards
  want you for two minutes. Traders won't deal with you while you are wanted.
- **In a village**: hit one of its people and the whole village turns on you and fights you for two
  minutes. The city's guards never hear of it.
- **Anywhere else** (the roads, the fields past the canals): whoever saw it fights or runs, but no one
  tells the city's guards.

If no one else sees, only the victim reacts. At night people see less far, unless you stand in
torchlight. At dawn everyone is back at their posts and grudges are forgotten, except a fight still going
on: a guard (or anyone) still after you keeps at it while you are on your feet and within 80 m of them,
twice a guard's longest bowshot, and you stay wanted. The fallen come back at dawn as before. Guards'
bows (city, village and caravan) carry 40 m by day, a little farther than other archers' 34 m.

Nobody chases you far from home. The city's guards keep to the city's lands and to about 40 m from
their posts, caravan guards stay within about 14 m of their caravan, village folk around their
village and townsfolk near where they were. Run far enough and they let you go, still angry, and walk
back. People who chase you, or walk home after a fight, jump the canals as you do, and go round the
harbor basin or over its bridge.

Search a dead body with E. Townsfolk carry a little food, clerics bread and dates, workers a few of
what they gather, guards a good weapon (the sickle-sword if yours is worse) and arrows.

Raiders camp south and north of the city and attack on sight. Each is much stronger than a
townsperson (260 health, light armor, blows of 19): with the starting club you stand no chance; with the
copper mace and leather armor you lose unless you parry well; with the bronze sickle-sword and leather
armor you win, narrowly unless you parry. Chewing herbs mid-fight helps. Killing them is no crime. Guards (and caravan guards) attack any raider that has left its
camp, so leading raiders back to the city brings help; the guards return to their posts afterwards.

The gate guards (at the temple gate and every city gate) also carry composite bows. They chase you
on foot as far as their reach; once you are past it (over a canal, say) they stand and shoot instead.
Archers stop to draw: a second or so before each shot, which is your moment to step aside.
Each camp's chest holds bronze, tin, ore, arrows and the like; it is restocked each dawn, and dead
raiders return the next day while you are away from the camp.

Two bands of bandits roam the eastern hills, three each: two swordsmen with bronze sickle-swords and an
archer with a composite bow, in dark cloth and leather. They are tougher than raiders and weaker than
barbarians (390 health, some armor, blows of 28), keep well away from the city, the villages, the camps and
the edge of the world, wander from spot to spot and now and then sit down a while, never on a road. They
set on anyone who comes near. The fallen come back the next day where their band is.

Barbarians hold two war camps across the river, north-west and south-west, behind palisades with a
lookout, a war banner and skulls on poles. They are twice as strong as the raiders (520 health, blows
of 38) and wear horned helmets and carry bronze axes; each camp also has a bowman with a war bow who
shoots from range. Fight them from range, parry, wear your best armor, or bring the war mace. No guards come out there. Their bodies and chests hold about twice the
loot.

## People and your look

Everyone is drawn in code, like the rest of the world (no model files): shaped bodies whose knees and
elbows bend as they walk, crouch and fight, the large eyes and strong nose of Sumerian statues, and the
clothes of the time.

- **Clothes:** the tufted fleece skirt (kaunakes), a short linen kilt, a fringed shawl wrapped over the left
  shoulder, a long robe.
- **Hair and beards:** shaved, short, long with a band round the head, a bun at the nape; no beard, short,
  full, or the long waved beard of the statues.
- **Jewellery:** lapis and carnelian beads, a gold band with earrings, gold arm rings.
- **Townsfolk** differ one from the next in all of these, in skin, and in hair (some grey). About four in ten
  are women, in shawls and robes. Priests are shaved and bare-chested in fleece; priestesses wear a robe and a
  tall headdress. Merchants wear robes, gold and the long beard. City guards wear a bronze helmet and a cloak
  studded with bronze, carry a spear, and a shield by day; village wardens wear leather and a cap. Raiders
  from the east wear leather and a grey cap; barbarians from the west, wolf pelts or leather and horned
  helmets.
- **Hands hold things:** the arm follows the sword, club and mace through a swing (both hands on the mace), the
  bow (in both hands, across the body, ready to draw), a spear, a staff, a tool or a torch. Your hands sway
  a little as you walk and settle while you aim. In first person you see your own hands: the bow and the
  drawing hand, the weapon, and the torch.

**Your look:** the first time you play on a computer or in a browser, a "Your look" window comes up before
the game: body (a man or a woman), hair, beard (none for a woman), clothes and jewellery, shown on your own body (the arrow keys turn
the view; 1-5 step through a row). Change it any time with "Your look" in the Esc menu. It is kept in
`sumer.look`; the other players see it, in your player color. Armor is worn over the clothes: the wolf pelt
over the shoulders with its head as a hood, leather in laced bands, bronze armor as the soldiers' studded
cloak (and helmet in a battle royale).

## Armor

| Armor | Made | Blows hurt |
|---|---|---|
| Wolf-pelt cloak | Workbench at home, F: 3 wolf pelts | 15% less; wolves leave you alone until you hit one |
| Leather armor | Workbench at home, E: 2 cowhide + 2 sheepskin | 25% less |
| Bronze armor | Smith: 5 bronze + 3 sheepskin + 3 cowhide | 40% less |

The cattle herder in the city trades cowhide (and milk); sheepskin comes only from the shepherd in the
herders' village. Wolves give a pelt each (E on a dead wolf). New armor is put on when it is better than
what you wear. Switch armor in Tab.

## Jobs

| Job | Where | You get |
|---|---|---|
| Wood | Any forest tree | 2 wood (4 with a bronze axe), costs 8 food |
| Clay | Clay pits on the river bank | 4 clay, costs 2 food |
| Fishing | River bank, pier west of the city, harbor, the west-bank pond | 2 fish (grill 2 in the oven), costs 1.5 food |
| Mining | Copper veins at the eastern mine | 2 copper ore (4 with a bronze pick), costs 16 food |
| Herbs | Green meadows, most of all across the river | 2 herbs, costs 1 food |
| Pottery | Kiln, craft quarter | 3 clay + 1 wood -> 2 pots |
| Bronze | Smelter, craft quarter | 2 copper ore + 1 tin + 2 wood -> 1 bronze |

Reed beds and clay pits come back 2 days after you take from them, herbs after a day and a half,
fishing spots and date palms after 1 day, felled trees after 16 hours and copper veins after 10 hours.

Tin comes only from the caravans. The smith turns bronze into an axe, a pick, arrowheads (+10 damage)
or bronze armor. Workers do these jobs from 6:00 to 18:30 and go home at night.

People who come after you, or go back to their posts, find their way: round houses and walls, up and
down the ziggurat's stairs, over bridges, and across a canal with a jump, instead of pressing into
whatever stands in the straight line.

## Cats, dogs and the mine

Cats keep to the city's streets (the harbor quay, by the alehouse, the west and east streets, the main street)
and round the fishers' village; dogs live in the villages (the hunters' slim salukis, the herders' big flock dogs,
the fishers' dog) and two lie by the temple gate. Like the sheep and cattle they are solid but come to no harm. Give
a cat a fish (or a grilled one) or a dog roast meat (E) and it eats, then follows you about for a couple of minutes
before it wanders home. Everyone in a shared game sees whose friend it is. There are none in a battle royale or a
party (no guards, no townsfolk's animals), so they cost those games nothing.

The copper mine in the eastern hills is worked as such mines were in the third millennium BC, at Timna and Feinan
or in Magan (today's Oman), whence Sumer had its copper: a rocky ridge with green malachite seams, an adit timbered
at its mouth, a shaft with a notched log for a ladder and a frame to haul baskets up; ore broken with stone hammers
on anvil stones and sorted by hand under a reed shade, rich green from grey waste (the spoil heap); two bowl furnaces
of clay blown through clay pipes by pot bellows, charcoal and wood beside them, the black slag thrown on a heap, and
the copper cast in bun-shaped ingots.

## Wolves and animals

Wolves hunt you only when you are alone outside the walls: no worker, villager, traveller or caravan
within about 20 m. By day the job sites and the villages are safe; at night, or out in the empty
steppe, a pack will chase you. Packs are four strong on the city side (five packs, two of them roaming the open east plain) and
five or six across the river (three packs), and a
wolf takes 78 damage to kill. Each bite hurts, and the pack circles between bites. Hitting a wolf turns
its pack on you, even near people. A badly wounded wolf fights on while its pack is beside it and limps
away only once no other wolf of its pack is on its feet nearby. Wolves (and all animals) go round trees,
rocks and water in their way and keep a body apart from one another. Run back to the city, crouch to be
noticed later, or wear a wolf-pelt cloak. Gazelles, onagers and geese notice you less at night; boars
and wolves don't.

Arrows that land close to an animal or whistle past it count too: prey bolts, and wolves come for you
with their whole pack (even past a wolf-pelt cloak).

Boars are tough (135 health). Hurt one and, like a wolf pack, its whole sounder turns on you: they
charge, gore, wheel round and charge again for about 40 seconds (longer if you keep hitting them). A
boar gives up only when it is nearly dead, when you get far away (about 80 m), or when you reach the
city.

Partridges keep to four of the woods (two east of the river, two across it) in coveys of three. They
run about under the trees and burst up on whirring wings when you come close, so stalk them crouching or
shoot them as they rise. One is a light load (they stack four to a basket slot): two roast to a meal in
your oven, and the hunters give four arrows for one.

Snakes lie hidden in tufts of long grass outside the walls; there are only a handful in the whole
world. Come within about 3 m (less when crouching, more when running) and one rears up with a hiss as a
warning; stay and it strikes (8 damage) every couple of seconds. Back away and it settles down again;
an arrow landing close sends it slithering off to new grass. A dead snake gives nothing.

Animals you kill stay gone until the next spring, when their herds are full again.

## Source layout

| File | Contents |
|---|---|
| `src/game.h` | Shared types, constants, palette, global state |
| `src/game.c` | Init, update loop, clock, sleep, hunger, health, gathering, interaction targets, slot switching |
| `src/terrain.c` | Hills height field (east, the downs, the west bank), ground mesh, colours and normals, view culling, ray vs terrain |
| `src/city.c` | City walls, towers, wall walk and ladders, gates, harbor, river and ford, canals, bridges, seasonal fields, house generator |
| `src/temple.c` | Temenos, ziggurat stages, rooms, stairways, gatehouse offering, shrine |
| `src/combat.c` | Health, armor and damage for people, witnesses and alarms, chasing, guards against raiders, looting, dawn reset |
| `src/raiders.c` | Two raider camps and two barbarian war camps, their idle routine, the restocked chests |
| `src/world.c` | Box store, collision queries, water and ponds, palms, reeds, forests, west-bank greenery, herbs, resources, sky and season light, culled drawing (boxes at a level of detail, one resource at a time) |
| `src/roads.c` | Main roads: their routes, terrain-following strips, walking heights for travellers, distance queries, the map overlay |
| `src/villages.c` | The hunters', herders' and fishers' villages, their traders' offers and village guards, the sheepfold flock, travellers |
| `src/jobs.c` | Craft quarter, docks, the old landing stages, mine, woodcutters' camp, farmhands, daylight workers, smith |
| `src/caravan.c` | Trade-road caravans, their stalls and offers |
| `src/farm.c` | Seasons, weather and rain, farm plots, ditch and well, buckets, coop and hens, city farms, crop stages |
| `src/village.c` | Home stations, pen, market traders and offers, villagers, recipes, blessing, the alehouse |
| `src/pets.c` | Cats and dogs: homes, wandering, feeding and following, their part of the snapshot, drawing |
| `src/boards.c` | The alehouse games (Royal Game of Ur, fifty-eight holes): rules, the townsman's play, seats, experience, their snapshot, the boards drawn on the tables |
| `src/wildlife.c` | Animal herds and stats, awareness AI, wolf packs, boar charges, snakes, reactions to arrows, spring return, arrows, melee and war-mace strikes |
| `src/items.c` | Item table and stack sizes, baskets, storage, barter checks, armor table |
| `src/player.c` | Movement, jumping, stairs, crouch, ladders, equipment, bow, camera |
| `src/ui.c` | HUD, minimap, inventory, storage, trade, coop, help and pause panels; the UI's scale on small and sharp screens |
| `src/render.c` | Shaded box, cylinder and blob primitives (with normals for the shaders), fog, colour helpers |
| `src/figures.c` | People's bodies, faces, hair, beards, clothes, jewellery and armor; every role's look; hands that hold things; the player's chosen look |
| `src/gfx.c` | The shader renderer: the vertex stream every primitive draws through, the still world cut into chunks on the GPU at three levels of detail, what moves caught each frame, the sun's shadow, sky and light by the hour, glow and smoothing, the graphics setting |
| `src/shaders.c` | The GLSL: world materials and light, sky, shadow depth, glow, smoothing and the final picture |
| `src/people.c` | Solid bodies for everyone: player pushout, NPC blocking checks |
| `src/lights.c` | Torches, precomputed static light (classic drawing), moving hand torches |
| `src/effects.c` | Particles, impact rings, arrow trails, swing trails, how a landed blow feels (hold, kick, flash) |
| `src/save.c` | Compact binary save/load, save slots |
| `src/main.c` | The program: window and the browser's frame loop |
| `src/actions.c` | Prompts and the actions behind E and F, taking from the shared world (a guest asks the server first) |
| `src/players.c` | Every player as the world sees them: positions, damage to a player on another machine, other players drawn |
| `src/mp.c` | Multiplayer messages: the server program's snapshots, players' requests and replies, messages in each player's language, players' things and experience handed to `server.py` |
| `src/net.c` | The connection to `server.py`: a WebSocket in the browser, the standard input and output in the server program |
| `src/lobby.c` | The browser's title screen: the account, the ways to play, the lists of servers, music, graphics, language |
| `src/account.c` | The account: logging in, level and K/D, the saves and experience sent to the server, the list of servers |
| `src/progress.c` | Levels and experience |
| `src/sha256.c` | SHA-256, for the proof of work a new account costs |
| `host/main.c` | The game's server program: one room's world, run for `server.py` |
| `server.py` | Serves the game, keeps the accounts (SQLite), runs the rooms' server programs and passes their messages |
| `src/web.c` | What differs in a browser: saves in IndexedDB, the captured mouse, taps and clicks, the canvas's pixels, phone detection |
| `src/touch.c` | Touch controls: the stick, looking by dragging, the see-through buttons |
| `src/shell.html` | The page around the browser game |
| `src/music.c` | The synthesizer, the three tunes, picking the mood, the volume, feeding the audio stream |
| `src/sfx.c` | Sound effects made at start (swishes, thuds, a clang, a thump, an arrow's crack), their volume |
| `src/lang.c` | Language setting, text lookup, the pixel font extended with the Turkish letters |
| `src/turkish.c` | Turkish text for every English string in the game |
| `tools/` | Test harness and its tests, browser test suites, the music renderer, the translation checker (see `robot.md`) |

Balance knobs: offers and recipes at the top of `village.c`, `villages.c`, `jobs.c` and `caravan.c`;
gather yields and food costs in `GATHER` in `game.c`; hunger rates at the top of `game.c`; herb healing
in `game.h`; growth, water, hen and rain numbers at the top of `farm.c` and in `game.h`; animal herds in
`HERDS`, stats in `DEFS` and boar, snake and war-mace numbers at the top of `wildlife.c`; stack sizes
and food values in `ITEMS` in `items.c`; hills in `terrain.c`; roads in `roads.c`; health, armor,
damage and sight for people (raiders and barbarians included) in `ROLES` in `combat.c`. The tunes, their tempos, scales and instrument levels are the track tables near the top of `music.c`.
The light by the hour (sun, moon, sky colors, how bright the night is, fog) is `Sky()` in `gfx.c`; torch
color and reach, shadow area and picture size per setting are in `gfx.c` too; the materials (brick size,
how rough things are, ripples) are in `src/shaders.c`.
