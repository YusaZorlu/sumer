# Sumer

Open-world prototype set in Ur, c. 2100 BC. C99 + raylib, built from primitive shapes and lit by its own
shaders: sun and moon shadows, torchlight at night, brick, soil and water worked into the surfaces, sky,
fog and glow (see Graphics).

## Run

**Ubuntu 20.04 or newer (x86_64)**

    ./sumer-prebuilt

or build from source:

    sh run.sh

The first build installs the needed packages with apt (asks for your sudo password), builds raylib 5.5 into `raylib-linux/`, then compiles and launches. Later runs only recompile the game.

Old GPU or a VM without OpenGL 3.3 (the game prints a hint if it can't open a window):

    GL21=1 sh run.sh

(with OpenGL 2.1 the game draws the classic way, without the shaders).

**macOS (Apple Silicon)**

    sh run.sh

Installs raylib with Homebrew if missing (or builds it from source without Homebrew), compiles `src/*.c`, launches.

## Play in a browser, alone or together

The game also runs in a web browser, and there up to six people can share one world. Start the
server (Python 3.8 or newer, nothing else to install):

    python3 server.py

It prints the addresses to open: `http://localhost:8000/` on this computer, `http://<this computer's
address>:8000/` for the others on your network (`--port 9000` picks another port). The browser
version is already built in `web/`; `sh build-web.sh` rebuilds it (needs Emscripten).

The title screen asks for your name, then:

- **Play alone**: the same game as the desktop version. Its three save slots are kept in the browser.
- **Host a game**: starts the shared world, or carries on the one you hosted before. The host's
  browser runs the world and keeps its save: the world and every player's things, saved every
  minute, when a night passes, when someone leaves, and on F5. Keep the host's tab open and in front
  while others play (browsers slow down hidden tabs). "New world" beside it (click twice) erases the
  hosted world so the next game starts fresh.
- **Rules when you host**, the two switches under Host:
  - **PvP** (on unless you switch it off): whether players can hurt each other with blows and
    arrows. A player who starts a fight inside the city's canals is wanted by the guards; hitting
    back, or hitting someone already wanted, is no crime. In a PvP game everyone starts somewhere
    else in the city (the spot is picked on open ground, away from the other players), and so does
    anyone brought down, instead of waking at home. The guards on the ziggurat's stairs and
    terrace are gone, so anyone can climb to the priest on top for the day's +20% speed boost: the
    welcome says so.
  - **Combat mode** (off unless you switch it on, and only with PvP): everyone fights with the best
    of everything (bronze sickle-sword, the war mace, the best bow with bronze arrowheads, bronze
    armor) and arrows never run out. The guards stay out of fights between players, and a player
    brought down comes to at home with full health (somewhere in the city in a PvP game, as always). Your own gear is untouched: it is what your
    record keeps, and what you have again in a game without combat mode.
  The rules show in everyone's Esc menu and when they join.
- **Join a game**: joins the game hosted in the same room. Join again under the same name and your
  things and your house are waiting.

Everyone who opens the same address plays in the same room; add `?room=any-name` to the address to run
another game on the same server. To play over the internet, run `server.py` on a machine the others
can reach (its port open); behind HTTPS the page connects securely by itself.

Playing together:

- Each player has a house of their own and wears its color: blue for the host, then red, green,
  yellow, purple and teal. The five guest houses stand in a row south of the east-west street, doors to the
  street, each with its own bed, storeroom, quern, trough, workbench and oven. The farm, the coop, the
  hens and the eggs are shared.
- The world never stops: the Esc menu only takes your hands off the keys, and time can't be
  fast-forwarded (T).
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
  Anyone wanted by the guards shows how long the search still lasts. When a player brings another
  down, everyone else hears of it.
- Blows between players are seen and felt. A swing at another player takes 0.3 seconds to land
  (a person's takes 0.55), and for that moment a yellow mark shows over the swinger; raise your
  melee weapon toward them in time and it turns green (turned aside: they are stunned a moment and
  your quick answer hits a quarter harder), otherwise it turns red as it lands. A player who is
  hit flinches, flushes red and the view jolts; red glows on the edges of your screen show where
  the blow came from (top ahead of you, bottom behind, left and right), for blows from people,
  animals and arrows too. Other players swing, raise and sweep their weapons as you do yours, and
  lean into the blow.

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
drawn with the screen's own pixels, so the letters stay sharp; taps work on every button. There are no
touch controls yet: play with a keyboard (a USB-C or Bluetooth one works; WASD to move, arrow keys to turn).

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
| E / F | Use, talk, pick up, climb a ladder, search a body, fill or pour a bucket, open the coop / alternate action (plant a date, pick another ferry fare) |
| R | Refill quiver from carried arrow baskets |
| Tab | Baskets (inventory), equipment and armor |
| T (hold) | Fast-forward time (playing alone) |
| F5 | Save |
| F11 or Alt+Enter | Borderless fullscreen on/off |
| M | Map zoom (whole world, x2.5, x6 around you); the map shows the main roads, both villages and the camps |
| N | Music volume: off, low, medium, high (also a button in the Esc menu and on the title screen) |
| G | Graphics: auto, high, low, classic (also a button in the Esc menu and on the title screen) |
| H | Controls and tips window |
| Esc | Pause / close a panel (pause menu: save slots, language, music, graphics, sounds, your look, save, new game, save & quit) |

Panels also take number keys (barter offer N, with 0, - and = for offers 10-12; use basket N; coop actions
1-4), so nothing depends on a click landing. The game pauses when the window loses focus (a screenshot tool, switching apps);
Resume or Esc takes the mouse back.

## Language

English and Turkish. In the pause menu (Esc), the "Language / Dil" button (or L) switches between them;
the choice is kept in `sumer.lang` next to the saves (the music volume is in `sumer.music`, the graphics setting in `sumer.gfx`, the sounds' volume in `sumer.sfx`). Türkçe için Esc menüsündeki "Language / Dil"
düğmesine (ya da L'ye) bas.

## Music

Three tunes play, and the game makes every note itself: a small synthesizer in `src/music.c`, so there are
no sound files, samples or recordings, and nothing to license. Press **N** (or use the button in the Esc
menu or on the title screen) to step the volume through off, low, medium and high; the choice is kept in
`sumer.music` next to the saves. It starts on medium.

| Tune | Plays | Sounds like |
|---|---|---|
| **Morning in Ur** (72 beats a minute) | By day, and on the title screen | A reed pipe singing over a low drone, a lyre picking a steady pattern of notes, a soft frame drum; calm and open |
| **Raiders at the Gate** (148) | While something is out to hurt you | Kettle drums and a rattle driving a gallop, an animal horn, a shrill pipe and a low plucked riff in a scale with a flat second; urgent and rough |
| **Stars over the Euphrates** (50) | From dusk to dawn, and while you sleep | A slow pad of open fifths, a soft harp picking five notes with long silences, a distant flute, lots of echo; relaxed |

The fight tune starts when a hostile guard, villager, raider, caravan guard, boar or wolf pack within
50 m is after you, when you are hit, and (in a game with PvP) when another player nearby is swinging a weapon or
aiming, or anyone is within 60 m in combat mode. It carries on for 12 seconds after the last such moment, then
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
- Craft quarter (west, by the harbor): potters, kiln, smelter, smith
- Caravans walk the southern trade road and set up stalls in the temple courtyard for a while; each
  travels with two spear-armed guards
- Main roads (drawn on the map): the southern trade road; the east road from the east gate through the
  hills to the hunters' village; the herders' road, which leaves the trade road south of the fields and
  crosses the downs to the herders' village; the north road from the north gate to the ford; the ferry
  road from the west gate; and tracks along the west bank. A few travellers walk them day and night
  (with torches after dark), and wolves leave you alone while you walk in their company
- Hunters' village, in a basin beyond the eastern hills: hide tents, drying racks, a smoking rack, an
  archery butt. The hunter butchers game better than your oven and trades pelts, meat, arrows, a large
  quiver and the composite bow
- Herders' village, on the downs south-west of the city: mud houses and a sheepfold with its flock.
  The shepherd is the only one who sells sheepskin and sheep, and takes plant waste as fodder
- Fishers' village, across the river by the ferry's west landing: reed houses, fish drying on racks,
  boats drawn up on the bank, and good fishing in the river just below. The fisher sells the only fine
  fishing net (8 reeds and 2 copper ore), which doubles every cast (4 fish instead of 2), and trades
  fish: 3 fish for 4 dates, 3 grilled fish for 3 fish and a piece of wood, arrows, herbs, or bronze
  for 8 fish
- Every village keeps a guard with a hunting bow. When the village turns on you, its guard shoots at
  you from where it stands
- Eastern hills: copper mine and woodcutters' forest; more forest along the river, in the hills and
  around the hunters
- Gazelles south-east and around the hunters' basin, onagers north-east, boars in the south-west marsh
  and the eastern woods, geese on the river
- Wolves roam everywhere outside the walls in packs of four on the city side, six across the river;
  where the packs start is different every game. A den sits by the ford at the far north end
- A few snakes lie hidden in long grass outside the walls (river meadows on both banks)
- Two raider camps on the city side of the river: one beside the southern trade road, one to the north
- The ferry west of the city crosses the big river for a fare (bread, fish, a pot, wood, reeds or
  dates; F picks another fare you can pay). A shallow ford at the far north end can be walked, through
  wolf country
- The west bank is green, rolling hill country: woods, flowering meadows rich in herbs, a reed-fringed
  pond with fish. Gazelles, onagers, boars and two wolf packs live there, and two barbarian war camps,
  north-west and south-west
- Ladders lead up to the wall walk at seven points around the city
- Guards: two at the temple gate and two inside each of the city's five gates (south, north, east, west and
  on the quays of the harbor's water gate), more on patrol in the east and west quarters and about the
  temple; clerics in the temple courtyard and on the ziggurat. A PvP game only takes away the three on the
  ziggurat's first terrace; every gate keeps its guards
- Torches line the streets, gates and temple at night; guards and caravans carry torches
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

Three save slots: `sumer.sav`, `sumer2.sav`, `sumer3.sav` in the folder you run the game from
(`run.sh` always runs it from the game folder). The game saves to the slot in play when you sleep,
when you quit, and on F5, and loads the last slot used at start. The pause menu shows each slot's day
and time: "Switch to this" saves the current game and loads that slot; on an empty slot a new game
begins there. "New game" (click twice) erases only the slot in play.

In the browser the slots live in the browser's storage for the game's address (a game played alone
there is also saved every two minutes); the host's shared game is kept the same way, in `sumer-mp.sav`.

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
torchlight. At dawn everyone is back and grudges are forgotten.

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
  bow, a spear, a staff, a tool or a torch.

**Your look:** the first time you play on a computer or in a browser, a "Your look" window comes up before
the game: hair, beard, clothes and jewellery, four choices each, shown on your own body (the arrow keys turn
the view; 1-4 step through a row). Change it any time with "Your look" in the Esc menu. It is kept in
`sumer.look`; the other players see it, in your player color. Armor is worn over the clothes: the wolf pelt
over the shoulders with its head as a hood, leather in laced bands, bronze armor as the soldiers' studded
cloak (and helmet in combat mode).

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

## Wolves and animals

Wolves hunt you only when you are alone outside the walls: no worker, villager, traveller or caravan
within about 20 m. By day the job sites and the villages are safe; at night, or out in the empty
steppe, a pack will chase you. Packs are four strong on the city side (five packs, two of them roaming the open east plain) and
five or six across the river (three packs), and a
wolf takes 60 damage to kill. Each bite hurts, and the pack circles between bites. Hitting a wolf turns
its pack on you, even near people; a badly wounded wolf runs off. Run back to the city, crouch to be
noticed later, or wear a wolf-pelt cloak. Gazelles, onagers and geese notice you less at night; boars
and wolves don't.

Arrows that land close to an animal or whistle past it count too: prey bolts, and wolves come for you
with their whole pack (even past a wolf-pelt cloak).

Boars are tough (135 health). Hurt one and, like a wolf pack, its whole sounder turns on you: they
charge, gore, wheel round and charge again for about 40 seconds (longer if you keep hitting them). A
boar gives up only when it is nearly dead, when you get far away (about 80 m), or when you reach the
city.

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
| `src/jobs.c` | Craft quarter, docks, ferry, mine, woodcutters' camp, farmhands, daylight workers, smith |
| `src/caravan.c` | Trade-road caravans, their stalls and offers |
| `src/farm.c` | Seasons, weather and rain, farm plots, ditch and well, buckets, coop and hens, city farms, crop stages |
| `src/village.c` | Home stations, pen, market traders and offers, villagers, recipes, blessing |
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
| `src/main.c` | The program: window, the frame loop on the desktop and in the browser |
| `src/actions.c` | Prompts and the actions behind E and F, taking from the shared world (a guest asks the host first) |
| `src/players.c` | Every player as the world sees them: positions, damage to a player on another machine, other players drawn |
| `src/mp.c` | Multiplayer messages: the host's snapshots, guests' requests and replies, messages in each player's language |
| `src/net.c` | The WebSocket to `server.py` |
| `src/lobby.c` | The browser's title screen: name, play alone, host, join, the hosting rules, music, graphics, language |
| `src/web.c` | What differs in a browser: saves in IndexedDB, the captured mouse, taps and clicks, the canvas's pixels, phone detection |
| `src/shell.html` | The page around the browser game |
| `src/music.c` | The synthesizer, the three tunes, picking the mood, the volume, feeding the audio stream |
| `src/sfx.c` | Sound effects made at start (swishes, thuds, a clang, a thump, an arrow's crack), their volume |
| `src/lang.c` | Language setting, text lookup, the pixel font extended with the Turkish letters |
| `src/turkish.c` | Turkish text for every English string in the game |
| `tools/` | Test harness and its tests, browser test suites, the music renderer, the translation checker, the prebuilt-binary script (see `robot.md`) |

Balance knobs: offers and recipes at the top of `village.c`, `villages.c`, `jobs.c` and `caravan.c`;
gather yields and food costs in `GATHER` in `game.c`; hunger rates at the top of `game.c`; herb healing
in `game.h`; growth, water, hen and rain numbers at the top of `farm.c` and in `game.h`; animal herds in
`HERDS`, stats in `DEFS` and boar, snake and war-mace numbers at the top of `wildlife.c`; stack sizes
and food values in `ITEMS` in `items.c`; hills in `terrain.c`; roads in `roads.c`; health, armor,
damage and sight for people (raiders and barbarians included) in `ROLES` in `combat.c`. The tunes, their tempos, scales and instrument levels are the track tables near the top of `music.c`.
The light by the hour (sun, moon, sky colors, how bright the night is, fog) is `Sky()` in `gfx.c`; torch
color and reach, shadow area and picture size per setting are in `gfx.c` too; the materials (brick size,
how rough things are, ripples) are in `src/shaders.c`.
