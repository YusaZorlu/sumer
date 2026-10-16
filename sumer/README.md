# Sumer

Open-world prototype set in Ur, c. 2100 BC. C99 + raylib, primitive shapes only.

## Run

**Ubuntu 20.04 or newer (x86_64)**

    ./sumer-prebuilt

or build from source:

    sh run.sh

The first build installs the needed packages with apt (asks for your sudo password), builds raylib 5.5 into `raylib-linux/`, then compiles and launches. Later runs only recompile the game.

Old GPU or a VM without OpenGL 3.3 (the game prints a hint if it can't open a window):

    GL21=1 sh run.sh

**macOS (Apple Silicon)**

    sh run.sh

Installs raylib with Homebrew if missing (or builds it from source without Homebrew), compiles `src/*.c`, launches.

## Controls

| Key | Action |
|---|---|
| WASD, Shift | Move, run |
| Space | Jump (stands up if crouching) |
| C | Toggle crouch: slower, much harder for animals to notice |
| Mouse / arrow keys, wheel | Look, zoom (zoom all the way in for first person) |
| P | First person on/off |
| 1 / 2 / 3 | Bow / melee weapon / hands free (also buttons in Tab) |
| Left mouse | Bow: hold to draw, release to shoot (the camera follows the arrow). Melee: strike |
| V | Strike with the melee weapon |
| Right mouse or Q | Parry with the melee weapon |
| Z | Arrow camera on/off |
| E / F | Use, talk, pick up, climb a ladder, search a body, fill or pour a bucket, open the coop / alternate action (plant a date, pick another ferry fare) |
| R | Refill quiver from carried arrow baskets |
| Tab | Baskets (inventory), equipment and armor |
| T (hold) | Fast-forward time |
| F5 | Save |
| F11 or Alt+Enter | Borderless fullscreen on/off |
| M | Map zoom (whole world, x2.5, x6 around you) |
| H | Controls and tips window |
| Esc | Pause / close a panel (pause menu: save slots, language, save, new game, save & quit) |

Panels also take number keys (barter offer N, with 0, - and = for offers 10-12; use basket N; coop actions
1-4), so nothing depends on a click landing. The game pauses when the window loses focus (a screenshot tool, switching apps);
Resume or Esc takes the mouse back.

## Language

English and Turkish. In the pause menu (Esc), the "Language / Dil" button (or L) switches between them;
the choice is kept in `sumer.lang` next to the saves. Türkçe için Esc menüsündeki "Language / Dil"
düğmesine (ya da L'ye) bas.

## Where things are

The city follows the plan of Ur around 2000 BC: an oval mud-brick wall with towers, the temple precinct
(temenos) with the ziggurat, a harbor basin fed from the Euphrates, canals around the walls, and barley
fields outside. Gates: south (main road), east, west, north, plus the harbor water gate. The map is
720 m across; the city is about 200 m by 170 m.

- Home (blue lintel): bed, storage jars (a bucket is in there), quern, kneading trough, oven, workbench
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
- Caravans walk the southern trade road and set up stalls in the temple courtyard for a while
- Eastern hills: copper mine and woodcutters' forest; more forest along the river and in the hills
- Gazelles south-east, onagers north-east, boars in the south-west marsh, geese on the river
- Wolves roam everywhere outside the walls; where the packs start is different every game. A den
  sits by the ford at the far north end
- Two raider camps on the city side of the river: one beside the southern trade road, one to the north
- The ferry west of the city crosses the big river for a fare (bread, fish, a pot, wood, reeds or
  dates; F picks another fare you can pay). A shallow ford at the far north end can be walked, through
  wolf country
- The west bank is empty for now except for gazelles, onagers, boars and a wolf pack
- Ladders lead up to the wall walk at seven points around the city
- Guards at the temple gate, the south city gate and on patrol in the east and west quarters; clerics in
  the temple courtyard and on the ziggurat
- Torches line the streets, gates and temple at night; guards and caravans carry torches
- At night distant things fade into the dark; torchlit places stay visible
- People are solid: you can't walk through anyone, and townsfolk step aside or wait when someone blocks their way

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
  cattle herder gives one date for each. The herder is the only one who gives dates, and only for waste.
- Spare seeds sell to the cattle herder (6 seeds for a jug of milk); wheat sells to the herder, the
  farmer and the caravans.
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

Everything costs food: idling and walking little, running more, and work by how hard it is. Mining
copper costs the most (7), then cutting wood (4), digging clay (2), smithing and forging (2-3), fishing,
cutting reeds, grinding, sowing and picking dates the least. Heavy work (2 food or more) is refused
when you are too hungry for it.

Health comes back on its own while your food is at 20% or more and nothing is attacking you. If you are
brought down you wake at home the next morning with 30% health and the same food, without your
carcasses (pelts are kept).

## Saving

Three save slots: `sumer.sav`, `sumer2.sav`, `sumer3.sav` in the folder you run the game from
(`run.sh` always runs it from the game folder). The game saves to the slot in play when you sleep,
when you quit, and on F5, and loads the last slot used at start. The pause menu shows each slot's day
and time: "Switch to this" saves the current game and loads that slot; on an empty slot a new game
begins there. "New game" (click twice) erases only the slot in play.

Each file is under 1 KB: day and time, your position, health and food, baskets, storage, quiver, bow,
tools, weapon, armor, temple access, blessing, farm plots and water, coop and hens, weather, hunted
animals and regrowing resources. Saves from earlier versions still load into slot 1 (the old 3x3 farm
keeps its rows in the wider field, old hens start a fresh life). The world itself is rebuilt from a
fixed seed, so it is never stored.

## Combat

- Arrows leave a glowing trail; where an arrow lands you get a marker with the distance for 3 seconds.
- Animals show health bars once they notice you or are hurt. Shots at unaware animals do double damage.
- Melee weapon: Wooden club (14), Copper mace (24), Bronze sickle-sword (38), one strike a second.
  Forge upgrades at the smelter in the craft quarter with F: 3 copper ore + 2 wood, then 3 bronze + 2 wood.
- Bow damage: 20 / 28 / 40 by bow, +10 with bronze arrowheads.
- Parry: before a person strikes, a yellow mark shows over their head. Press right mouse or Q with
  your melee weapon in hand while facing them: the mark turns green, the blow does nothing, and they
  are open for a moment (your next hit does a quarter more). If it lands, the mark turns red. There
  is no shield yet.
- Only what is in your hand is drawn and used: 1 bow, 2 melee weapon, 3 hands free.

## People, crime and raiders

Everyone has a health bar. People are tough: a townsperson takes about 9 club blows, a guard
(armored, 320 health) about 35 club blows or 13 with the bronze sickle-sword. An unaware target takes
half again as much damage.

Nobody attacks you unless you attack first. If you hurt someone who wasn't fighting you and others see
it, guards and the braver townsfolk come for you, the rest run, and the guards stay after you for two
minutes. Traders won't deal with you while you are wanted. If no one else sees, only the victim
reacts. At night people see less far, unless you stand in torchlight. At dawn everyone is back and
grudges are forgotten.

Search a dead body with E. Townsfolk carry a little food, clerics bread and dates, workers a few of
what they gather, guards a good weapon (the sickle-sword if yours is worse) and arrows.

Raiders camp south and north of the city and attack on sight. Each is stronger than a townsperson
(190 health, light armor, heavy blows): with the starting club and no armor you lose a 1v1 even if you
strike first; with the copper mace and leather armor you win with about half your health, more if you
parry. Killing them is no crime. Guards (and caravan guards) attack any raider that has left its camp,
so leading raiders back to the city brings help; the guards return to their posts afterwards. Each
camp's chest holds bronze, tin, ore, arrows and the like; it is restocked each dawn, and dead raiders
return the next day while you are away from the camp.

## Armor

| Armor | Made | Blows hurt |
|---|---|---|
| Wolf-pelt cloak | Workbench at home, F: 3 wolf pelts | 15% less; wolves leave you alone until you hit one |
| Leather armor | Workbench at home, E: 2 cowhide + 2 sheepskin | 25% less |
| Bronze armor | Smith: 5 bronze + 3 sheepskin + 3 cowhide | 40% less |

The cattle herder trades cowhide and sheepskin; wolves give a pelt each (E on a dead wolf). New armor
is put on when it is better than what you wear. Switch armor in Tab.

## Jobs

| Job | Where | You get |
|---|---|---|
| Wood | Any forest tree | 2 wood (4 with a bronze axe), costs 4 food |
| Clay | Clay pits on the river bank | 4 clay, costs 2 food |
| Fishing | River bank, pier west of the city, harbor | 2 fish (grill 2 in the oven) |
| Mining | Copper veins at the eastern mine | 2 copper ore (4 with a bronze pick), costs 7 food |
| Pottery | Kiln, craft quarter | 3 clay + 1 wood -> 2 pots |
| Bronze | Smelter, craft quarter | 2 copper ore + 1 tin + 2 wood -> 1 bronze |

Reed beds and clay pits come back 2 days after you take from them, fishing spots and date palms after
1 day, felled trees after 16 hours and copper veins after 10 hours.

Tin comes only from the caravans. The smith turns bronze into an axe, a pick, arrowheads (+10 damage)
or bronze armor. Workers do these jobs from 6:00 to 18:30 and go home at night.

## Wolves and animals

Wolves hunt you only when you are alone outside the walls: no worker, villager or caravan within about
20 m. By day the job sites are safe; at night, or out in the empty steppe, a pack will chase you.
Each bite hurts, and the pack circles between bites. Hitting a wolf turns its pack on you, even near
people; a badly wounded wolf runs off. Run back to the city, crouch to be noticed later, or wear a
wolf-pelt cloak. Gazelles, onagers and geese notice you less at night; boars and wolves don't.

Animals you kill stay gone until the next spring, when their herds are full again.

## Source layout

| File | Contents |
|---|---|
| `src/game.h` | Shared types, constants, palette, global state |
| `src/game.c` | Init, update loop, clock, sleep, hunger, health, gathering, interaction targets, slot switching |
| `src/terrain.c` | Hills height field, ground mesh and colours, ray vs terrain |
| `src/city.c` | City walls, towers, wall walk and ladders, gates, harbor, river and ford, canals, bridges, seasonal fields, house generator |
| `src/temple.c` | Temenos, ziggurat stages, rooms, stairways, gatehouse offering, shrine |
| `src/combat.c` | Health, armor and damage for people, witnesses and alarms, chasing, guards against raiders, looting, dawn reset |
| `src/raiders.c` | Two raider camps, their idle routine, the restocked chests |
| `src/world.c` | Box store, collision queries, palms, reeds, forests, resources, sky and season light, culled drawing |
| `src/jobs.c` | Craft quarter, docks, ferry, mine, woodcutters' camp, farmhands, daylight workers, smith |
| `src/caravan.c` | Trade-road caravans, their stalls and offers |
| `src/farm.c` | Seasons, weather and rain, farm plots, ditch and well, buckets, coop and hens, city farms, crop stages |
| `src/village.c` | Home stations, pen, market traders and offers, villagers, recipes, blessing |
| `src/wildlife.c` | Animal herds and stats, awareness AI, wolf packs, spring return, arrows |
| `src/items.c` | Item table and stack sizes, baskets, storage, barter checks, armor table |
| `src/player.c` | Movement, jumping, stairs, crouch, ladders, equipment, bow, camera |
| `src/ui.c` | HUD, minimap, inventory, storage, trade, coop, help and pause panels |
| `src/render.c` | Shaded box, cylinder and blob primitives, humanoid model, fog, colour helpers |
| `src/people.c` | Solid bodies for everyone: player pushout, NPC blocking checks |
| `src/lights.c` | Torches, precomputed static light, moving hand torches |
| `src/effects.c` | Particles, impact rings, arrow trails |
| `src/save.c` | Compact binary save/load, save slots |
| `src/lang.c` | Language setting, text lookup, the pixel font extended with the Turkish letters |
| `src/turkish.c` | Turkish text for every English string in the game |

Balance knobs: offers and recipes at the top of `village.c`, `jobs.c` and `caravan.c`; gather yields
and food costs in `GATHER` in `game.c`; hunger rates at the top of `game.c`; growth, water, hen and
rain numbers at the top of `farm.c` and in `game.h`; animal herds in `HERDS` and stats in `DEFS` in
`wildlife.c`; stack sizes and food values in `ITEMS` in `items.c`; hills in `terrain.c`; health,
armor, damage and sight for people in `ROLES` in `combat.c`.
