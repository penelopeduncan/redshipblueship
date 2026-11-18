# Known issues

**Applies to:** `v0.5.2-alpha` ("Ellen Charlie"). It is `v0.5.1-alpha` plus the changes under "Since v0.5.1-alpha" in
[`release-notes-next.md`](release-notes-next.md). Saves are save format 6, so files from `v0.5.1-alpha` open. Seeds
come from generator 6, so seed packages from `v0.5.1-alpha` and earlier are refused. Each older release is described
in its own section of [`release-notes-next.md`](release-notes-next.md). Dated records further down this file keep
the page names and build hashes of their day.
**Last updated:** 2026-11-17 (the `v0.5.2-alpha` draft: a "not yet played" entry for each new feature; "Read this
before you play" cut down to what applies to a file or seed package this version can open). **Playtesting this
build?** Start with [`playtest-2026-09.md`](playtest-2026-09.md).

RedShipBlueShip is **alpha** (`v0.5.2-alpha` is Ellen Charlie). It boots Ocarina of Time and Majora's Mask from
one executable, round-trips between them through the Happy Mask Shop ↔ Clock Tower
portal, and generates a paired randomizer world in which items cross both ways.
This document is the honest list of what is broken or missing, grouped by what it
does to you as a player rather than by issue number. Every entry links its tracking
issue; entries that have been fixed since the last revision are struck through and
name the pull request, so old reports can be matched to the fix.

If you hit something not on this list, please file it. If you hit something on
this list, the issue link is the place to add detail.

---

## Not yet played (new in v0.5.2-alpha and v0.5.1-alpha)

Everything below passed the automated gates. None of it has been checked by a person in a running game.

### New in v0.5.2-alpha

- With Shuffle Deku Stick Bag or Shuffle Deku Nut Bag on, Majora's Mask now starts without sticks or nuts too
  (issue #1652). Until the bag is found in either game, its drops, shops and checks give none. A check that holds
  them gives junk. A shop shelf that sells them as usual will not sell them, and a shuffled shelf sells junk once
  and stocks the item again after the bag. A stick or nut pack from the other game becomes junk. A bag found in
  Termina opens both games at once. Later Progressive Deku Stick and Nut upgrades still cross, so Infinite Upgrades
  keeps working. Files made before this version keep the old rule (Ocarina of Time only). To try it: play a paired
  file with either shuffle on, find the bag in Termina, then cross. Only tests without a running game cover this.
- Each item waits for its own bag (issue #1691). Before the Deku Stick Bag, a Deku Baba (or a withered one, or a Mini
  Baba in Majora's Mask) leaves no stick in either game, and Ocarina of Time's Business Scrubs do not sell
  sticks. Before the Deku Nut Bag, the Scrubs do not sell nuts, and an unshuffled Majora's Mask chest that holds
  nuts (the Treasure Chest Shop's prize for winning again as Deku Link in the same cycle) gives a blue rupee
  instead. Checked without a running game only: kill a Deku Baba in each game before the bag and look. In a file
  made before this version, Ocarina of Time keeps the old behaviour: before the bag a Deku Baba's stick plays
  the "Deku Stick" get-item scene and gives nothing, and a Business Scrub still sells sticks or nuts that give
  nothing.
- The Blast Mask in water (issues #1701, #1714): with the Blast Mask (OoT) rule on, the blast goes off wherever
  OoTMM allows it in water: swimming at the surface, under water after falling in, in a dive, and in the Iron
  Boots. It does not go off while Link comes up for breath or reels from a hit in water. Checked without a
  running game only. Nobody has seen what the blast breaks or hurts under water. Out of the water this build is
  still looser than OoTMM: the blast also goes off in moves where OoTMM's does not, such as being knocked back
  on land, being shocked or frozen (under water in the Iron Boots too), a sword swing, a jump attack and most of
  a roll.
- The Check Tracker's Majora's Mask pane (issues #1127, #1706): an enemy drop listed under every area where the
  enemy lives, and the checks inside shops, houses, Great Fairy Fountains and other rooms listed under the area
  their door opens onto. Each copy of a drop counts in its own area's numbers, so the areas add up to more than the
  pane's total. A few checks that are not enemy drops are still listed differently from Majora's Mask's own Check
  Tracker: the Bombers' Notebook, the Clock Town postbox and stray fairy, the Keaton quiz, the Oath to Order, the
  Moon's Tear and the inverted Stone Tower checks (issue #1711). Checked by automated tests only. Inside the Stock
  Pot Inn the Hint Tracker's Majora's Mask pane now scrolls to East Clock Town's hints.
- Moon Crash Behavior's New Cycle default (issue #1671). One automated run lets the moon fall on Majora's Mask's
  debug save, and another lets it fall in a paired file where Shuffle Ocarina forces New Cycle. No test lets it
  fall in a paired file that has New Cycle only from the new default, and nobody has played one. To try it: let
  the moon fall in a new paired file. You should wake on the Dawn of the First Day with your items.
- Shuffle Ocarina (issue #1671). An automated run drives the start without the ocarina, midnight of the Final Day
  in North Clock Town and the moon crash. Nobody has played such a file. Not driven by any run: a song check
  reached before the ocarina, finding the ocarina in either game, and letting the Clock Tower roof's timer run
  out with no ocarina. See "Shuffle Ocarina: what is not yet seen in a running game" under "Majora's Mask
  specific".
- A shared item found in the other game is yours at once (issue #1551): tested without a running game only. To try
  it, use a seed with a shared option on (for example Shared Shields) and pick up the other game's copy at a check
  in the game you are playing, such as a Hero's Shield from Majora's Mask on an Ocarina of Time check (with the rule on it
  is named without a game tag). It should be yours there without a crossing.
- Shared masks and songs named without a game tag in hints and gossip stones, the spoiler log's playthrough and
  hint entries, the check trackers, get-item and shop text (issues #1560, #1688, #1698): checked by automated
  tests only.
  The spoiler logs' location lists keep the tag. Look at a gossip stone, a shop shelf and a pickup message for a
  shared mask or song.
- Shuffle Chest Minigame and Shuffle Silver Rupees in a paired world (issue #1530): worlds were generated and
  checked without a running game. Nobody has played the Treasure Chest Game or a silver rupee room in a paired
  file.
- These randomizer changes were checked by generation and logic tests only, and nobody has finished a seed made
  with them: Link's Pocket never taking a shared song (issue #1552), the Two Spikes Room grass logic (2Ship2Harkinian
  #1893), the Death Mountain Crater and Fire Trial logic, the Voidout Collection trick (Ship of Harkinian #6294,
  #6336, #6487, #6453) and fixed placements left out of the playthrough (Ship of Harkinian #6146). The Voidout
  Collection description names a Water Temple Gold Skulltula that logic does not use the trick for yet.
- The item tracker's rows for the cross-game masks and their Shared rows (issue #1570), and its Shared Deku Sticks
  and Deku Nuts rows under the bag shuffles (issue #1652): checked by automated tests only. Open the item tracker
  and its layout editor in a file with the mask rules on.
- Majora's Mask's Active Item on B, Faster Hookshot and Fix Owl Save Scene Layers (2Ship2Harkinian #1870, #1951,
  #1952) were tested without a running game only: the first two only for being wired up, the owl fix on a staged
  save. Active Item on B replaces Great Fairy Sword B-Button Attack, and the old setting is not carried over. Fix
  Owl Save Scene Layers is on by default: to try it, load an owl save at Great Bay Coast after Gyorg from Majora's
  Mask's own file select.
- Ocarina of Time's dev-tool and Boss Rush changes (Actor Viewer, Mod Menu Clear List, Collision Viewer Scarecrow
  Spawn, the debug warp screen's time savers, Boss Rush's PAL name) and the ruined Market's fog and sky with
  Disable 2D Pre-Rendered Scenes have no automated test and have not been seen in a running game.
- The exit autosave fix (issue #1689) was tested without a running game only. With Autosave on, quit a randomizer
  file after entering a dungeon, load it again, and look at whether the check tracker still shows that dungeon's
  checks.

### From v0.5.1-alpha, still not played

- The three new Ocarina of Time shuffles (Wonder Items, Icicles and Red Ice, Rocks and Boulders;
  issues #1405, #1408 and #1409) have been generated and tested without a running game only. Nobody
  has collected their checks in a paired playthrough.
- Majora's Mask's new dungeon item defaults (issue #1548) have not been played. Nobody has finished
  a dungeon with its keys and chest fairies confined to it.
- The fix for a Majora's Mask chest replaying its message after a cycle reset (issue #1601) was
  tested with the game's own reset and re-open code, not by playing through a Song of Time.
- Majora's Mask's Message Viewer, Collision Viewer and Event Log (issues #1575, #1576 and #1572)
  have not been used in a running game.
- The new first-run extraction messages (issue #1590) have not been seen on a machine without a file
  picker or with a failing extraction.
- ~~Found while playing this release: with the Blast Mask (OoT) rule on, the mask's blast does not cut shrubs, although logic
  expects it to, and in the Market guard house the blast appears in one fixed place (issue #1615). Rocks and
  pots break as they should. Until it is fixed, do not rely on the Blast Mask for a check behind a shrub.~~
  Fixed by PR [#1617](https://github.com/penelopeduncan/redshipblueship/pull/1617): the blast's hitbox stayed at
  the room's origin, so it now starts at Link's feet and cuts the shrubs around him. The blast now also hurts
  Link, as Majora's Mask's does. Tested without a running game only: nobody has played it yet.
- Masks as Adult (issue #1647) has been tested without a running game only. With it on, logic may expect adult Link
  to wear the masks, and nobody has finished such a seed. Logic counts the Deku Theater's Skull Mask and Mask of Truth
  rewards as an adult's too, but nobody has checked that the theater's audience is there when adult Link enters the
  grotto: until someone has, get those two rewards as child Link.
- Adult Link wearing the Blast and Stone Masks (issue #1647) has been tested without a running game only. Nobody has
  looked at how the two masks sit on adult Link's face (they may float, sink or clip through his hair, with custom
  adult models too). Nobody has seen the Stone Mask hide an adult from the Gerudo guards or from the enemies only an
  adult meets (Dark Link, Like Likes, Freezards), although with Masks as Adult on logic may expect it to get him past
  the Gerudo guards: until someone has, do not rely on it there. Nobody has checked in play that the Blast Mask
  takes B from a held Megaton Hammer, or that it refuses to go off with the Hookshot in hand or in flight. With
  Timeless Equipment on, adult Link can wear both masks even with Masks as Adult off.
- Shared Epona's Song, Song of Storms and Song of Time under Shuffle Songs: Off (issue #1561) were tested without a
  running game only. Each stays at its usual check in both games, and the second of the two you reach gives junk.
  Nobody has reached both of a song's usual checks in a paired playthrough, so the junk at the second one, and what
  the check trackers show then, have not been seen.
- Found while playing `v0.5.1-alpha`'s release candidate: with Plentiful item pools and a Shared Items rule on, this build places one copy
  of the shared item where OoTMM places two (issue #1653). OoTMM adds one more shared copy of the Lens of Truth, the
  magic arrows, the ocarina buttons, several masks and the scale; this build leaves the spare out for now.
- Quitting with a Majora's Mask item found in Ocarina of Time not yet sent (issue #1635) was tested by an automated
  quit and reload only, while the item waited for the item queue. An item held for a full cross-game outbox, which
  takes 2048 items waiting to cross, was tested without a running game only. Nobody has quit with an item waiting
  and played on.
- Majora's Mask's Hookshot Anywhere option (issue #1574) has not been used in a running game, and no world has been
  generated with it at On or Logical: only the logic it adds was checked. Nobody has aimed the Hookshot at Termina's
  walls with it on. With Logical, logic expects the Hookshot in fewer places than OoTMM does, so some routes OoTMM
  counts are not counted here; that makes logic more cautious, not less.
- Majora's Mask's new Mayor Argues Until Moonfall option (2Ship2Harkinian #1972) has not been tried in a running
  game. Nobody has stayed in the Mayor's Residence past 6 pm on the Final Day with it on, so it is not known that the
  meeting really goes on. Logic never expects it.
- These Majora's Mask changes were tested without a running game only, and nobody has played them: the new logic
  fixes (2Ship2Harkinian #1954, #1984, #2006, #2017, #2018), the ocarina dive (2Ship2Harkinian #1842, #1847, #1853),
  Hot Water Never Cools (2Ship2Harkinian #1989), text mods (2Ship2Harkinian #2000, #2013; no real mod has been
  tried) and the randomizer polish (2Ship2Harkinian #1614, #1450, #1706, #1775, #1478, #1769): the grey look of an
  enemy whose soul you lack, the enemy soul and pot models, the file select's randomizer box colour and the "Seed
  Failure:" toast.
- The Temple of Time altar's new goal line (Ship of Harkinian #7341) has not been seen on screen. It is shown without
  the Triforce icon that Ship of Harkinian puts in front of it.
- Ocarina of Time's new Save Editor layout (Ship of Harkinian #6474, #7383) has not been opened in a running game,
  nor has the player name been checked after a change of Player Name Language. It is a developer tool: back up
  `Save/` before you edit a file with it.
- These Ocarina of Time changes were tested without a running game only, and nobody has seen them in play: the
  grotto and void-out fixes (Ship of Harkinian #6529, #6721, #6854, #6888, #7271, #7276), the Big Octo platform's
  cutscene in Jabu-Jabu (Ship of Harkinian #7158), the shuffled enemy drop that no longer vanishes (Ship of Harkinian
  #7183), the Dark Link guards under Crowd Control and Sail (Ship of Harkinian #7054, #7272), King Zora's camera with
  Skip Story Cutscenes switched on or off while he moves aside in Zora's Domain (issue #1591), the crawlspace camera
  (Ship of Harkinian #6709), the dungeon map colours (Ship of Harkinian #6351), the Audio Editor's Ending tab and a
  row set to None (Ship of Harkinian #5685, #7372), a finished file's gameplay timer (Ship of Harkinian #7385,
  #7404), the Silver Gauntlets chest moving Nabooru's story on (issue #1602) and the new Bunny Hood jumps in logic
  (Ship of Harkinian #7032, #7145, #7488).

## Read this before you play

**Files from `v0.5.1-alpha` open in this version: the save format is still 6. Once this version saves one, `v0.5.1-alpha` refuses it ("File made by another build"), so back up `Save/` first if you may go back. A file saved by `v0.5.0-alpha` or any earlier build, plain (non-randomizer) files included, is refused by this build ("Refused: File made by an older version") and is not converted: keep the old build in its own folder to finish it, or erase it to start a new file in that slot. Saves made with development builds between releases are not supported** (see "Files from `v0.5.0-alpha` and earlier are refused" under "Save loss and corruption").

**A seed package exported by `v0.5.1-alpha` or any earlier build is refused (seed generator version 6): generate the seed again. The same seed and settings can make a different world than `v0.5.1-alpha` made, with the default settings too. This does not affect saved files: a paired file keeps the world it was created with** (see "Seed packages from `v0.5.1-alpha` and earlier builds are refused" under "Randomizer generation").

**A file keeps the rules it was created with, and loading it puts its Majora's Mask settings back into the menu.** Moon Crash Behavior now defaults to New Cycle. A file made before this version keeps the Moon Crash Behavior it was made with (Last Save, unless you chose New Cycle or Shared Song of Time is on, which forces New Cycle), and in such a file Shuffle Deku Stick Bag and Shuffle Deku Nut Bag gate Ocarina of Time only. Loading such a file puts back the Moon Crash Behavior it was made with (for most older files that is Last Save) and turns Shuffle Ocarina off in the menu (a "Restored for Majora's Mask" message lists what changed). The next file you create uses those values, so check Randomizer > MM Randomizer before you create one.

### Cross-game randomization: what ships now, and what does not

Generating a randomizer seed makes one paired world: one fill places one bag of items over both games' checks
and proves the goal over both games' logic. An item that crosses to the other game is no longer in its home
game's pool.

**What ships in `v0.5.2-alpha`:**

- **One seed, one paired world, one bag, items crossing in both directions.** Generating an
  OoT randomizer seed also generates the paired Majora's Mask world. OoT items are
  placed on MM checks and MM items on OoT checks, and **a crossed item is no longer also
  in its home pool** (PR [#743](https://github.com/penelopeduncan/redshipblueship/pull/743)). Crossings land on chests, on shop shelves, on Ocarina of Time's Business Scrubs and
  merchants, on the Treasure Chest Game's prize, on Tingle's maps and, under Song Locations, on song checks (see
  the shops entry below). There is no limit on how many land in one game. Which items may cross depends on the
  item and on the Crossing Direction you choose.
- **Hearts and capacity upgrades are counted once for the pair.** Health is one shared
  bar. On the shipped profile the two pools hold 76 Pieces of Heart, 15 Heart Containers
  and 2 Double Defense; the bag keeps 44, 6 and 1 (pieces and containers OoT 14 + 4, MM 30 + 2;
  the Double Defense from either game), and the bar stops at 20 hearts (PR [#744](https://github.com/penelopeduncan/redshipblueship/pull/744)). That is 42 trimmed heart and
  double-defense rows. Capacity families whose top tier is the same in both games (magic,
  quiver, bomb bag, and the wallet when OoT has no tycoon's wallet) are trimmed to the
  shared maximum the same way; with them, 51 rows (OoT 22, MM 29) become filler in their
  home game. Families whose top tiers differ keep every copy and cost a known dead pickup:
  the hookshot (OoT 2, MM 1) always. ~~And the ocarina when the frozen rules arm it.~~ The
  ocarina is never armed since [#879](https://github.com/penelopeduncan/redshipblueship/issues/879)
  (PR [#889](https://github.com/penelopeduncan/redshipblueship/pull/889) retired the effect), so each game keeps its
  own and no ocarina pickup is dead. Stick
  and nut capacity exist only in OoT and are not trimmed. In a file made with this version, Shuffle Deku Stick Bag
  and Shuffle Deku Nut Bag gate Majora's Mask too: see "New in v0.5.2-alpha" above.
- **Both worlds are generated together, once, when you press Generate.** The world is stored as a seed package in
  the Randomizer folder, and creating the file uses that package. Arrival in Majora's Mask generates nothing. If
  generation fails, Generate shows a "Not generated:" toast and nothing is staged: no partial world and no plain
  (vanilla) Termina. A paired world's OoT spoiler dropped on the file select on its own is refused with a toast.
  One generation makes one file: once a file is created from it, "Start Randomizer" greys out and reads "This
  seed was used for file N."
- **Foreign items have their own identity.** An OoT item in Termina shows its real
  name, its OoT icon and its model in MM's get-item cutscene; an MM item in Hyrule shows
  its MM model in OoT's get-item cutscene. The text names the item's game, "(OoT)" in
  Termina and "(MM)" in Hyrule. While their Shared rule is on, the shared Gerudo, Skull and Spooky Masks and the
  shared warp songs (the Song of Soaring and Ocarina of Time's six warp songs) are named without a tag
  (issues #1560, #1688, #1698). Only the spoiler logs keep the tag: in their location lists, which the game reads
  back, and in the list of cross-game placements.
- **The combo is one game, frozen when you generate it.** Majora's Mask's randomizer options and the Cross-Game
  rules are part of the world, and changing them afterwards does not change it. Loading a paired file puts the
  file's own values back into the menu, and a toast names what changed ("Restored for Majora's Mask"). The next
  file you create uses those values, so check them first: a file made before `v0.5.2-alpha` sets Moon Crash
  Behavior to Last Save (unless you chose New Cycle) and turns Shuffle Ocarina off. A refused load or crossing
  marks the save slot refused rather than silently overwriting it. Set Majora's Mask's options before you press
  Generate: `Randomizer → MM Randomizer`, `Randomizer → MM Locations` and `Randomizer → MM Tricks`.
- **The pair is generated with Glitchless logic, and with "[Both Games] Logic" set to All Locations by default.**
  The fill proves the goal (Ganon & Majora by default) over both games' logic and checks that every location is
  reachable. It has a budget of 2500 fill rounds per attempt and 5000 in all, about 60 s on the development
  workstation. The budget counts work, not time, so a seed either generates on every machine or on none. A
  progress overlay ("Placing item n of N in both worlds") shows while you wait after pressing Generate: the
  window stays responsive and menu input is ignored while it shows.
- **The Happy Mask Shop door is never shuffled.** It is the OoT ↔ MM crossing, so
  OoT's own entrance randomizer now leaves both directions of it out of every
  shuffle pool; under interior (or any) entrance shuffle the door you walk
  through is still the door to Termina (PR [#691](https://github.com/penelopeduncan/redshipblueship/pull/691), [#661](https://github.com/penelopeduncan/redshipblueship/issues/661)).
  Since PR [#956](https://github.com/penelopeduncan/redshipblueship/pull/956) ([#948](https://github.com/penelopeduncan/redshipblueship/issues/948))
  the door also opens at night while a paired file is open, so child Link can cross at any hour. On a vanilla
  file it still shows its "Apply during the day" sign at night and does not open, as in Ocarina of Time, and
  the other night shops keep their signs. The crossing is child-only: as adult the Market is in ruins and has
  no Happy Mask Shop door.
- **The Happy Mask Shop's masks are always in the item pool** (PR [#974](https://github.com/penelopeduncan/redshipblueship/pull/974), [#973](https://github.com/penelopeduncan/redshipblueship/issues/973)).
  The shop's door is the crossing, so the shop's room never loads. A paired world is generated with Ocarina of
  Time's "Mask Quest" as "Shuffle", as in OoTMM: all eight masks are in the item pool, and the pause menu's mask
  slot cycles through the masks you have. "Mask Shop Hint" is generated off, because its sign is inside the
  shop. Both rows on the Randomizer settings page are greyed, show the value the world uses, and say why on
  hover. Following Ship of Harkinian's own "Shuffle", the Deku Theater's Mask of Truth check does not take one of
  the "Always" gossip-stone hints, which SoH gives it only under "Vanilla".
- **Paired MM worlds can enable individual Majora's Mask logic tricks.** MM now
  has a per-trick vocabulary (86 trick keys, frozen into the world's identity
  like OoT's). 43 of them are bound to a real logic edge; the rest are reserved for an Ocarina of Time item or
  cannot be bound yet. Every trick is off by default.
  **One of them is Deku-Stick combat** (PR [#729](https://github.com/penelopeduncan/redshipblueship/pull/729), [#719](https://github.com/penelopeduncan/redshipblueship/issues/719)). MM's logic used to assume you
  could fight 17 kinds of enemy with a Deku Stick, even though the matching trick is off by default.
  Those enemies now need `Deku Stick Fighting` enabled, or another weapon. Using the stick as a fire
  source is unchanged. None of the pinned test worlds moved. A
  trick whose edge is not bound draws disabled-with-reason on Randomizer → MM
  Tricks rather than enabled-and-inert ([#697](https://github.com/penelopeduncan/redshipblueship/issues/697) closed with PR [#763](https://github.com/penelopeduncan/redshipblueship/pull/763)). PR #763 also
  follows OoTMM on two tightenings, so with the tricks off logic kills Twinmold only with
  the Giant's Mask, magic and a sword ("Twinmold with Bow (MM)" admits the Bow), and the
  West Clock Town bank's heart piece needs the Giant's Wallet ("Bank Rewards Require One
  Less Wallet" admits the Adult Wallet).
- ~~**You may opt into one shared Ocarina across both games**~~ RETIRED by
  [#879](https://github.com/penelopeduncan/redshipblueship/issues/879) (PR [#889](https://github.com/penelopeduncan/redshipblueship/pull/889)): the row is gone, and a world created with it on loads with it ignored. Was: (Combo → Cross-Game
  Rules → "[Both Games] Shared Ocarina") — off by default, frozen at file creation
  like every other combo rule: obtaining an ocarina in
  either game grants it in the other (PR
  [#675](https://github.com/penelopeduncan/redshipblueship/pull/675)).
- **Majora's Mask starts a paired world with the Ocarina of Time and the Song of Time
  only.** The Kokiri Sword and the Hero's Shield are in MM's item pool, as in OoTMM. With Shared Song of Time on,
  MM starts with the Ocarina alone. With Shuffle Ocarina on (`Randomizer → MM Randomizer`, Additional Items; off
  by default), it starts without the ocarina: the ocarina is then in the item pool, no song can be played until
  you find it, and the Moon Crash is always New Cycle.
- **Moon Crash Behavior defaults to New Cycle** (issue #1671). It is always New Cycle with Shuffle Ocarina or
  Shared Song of Time on. New Cycle does not yet let you save on the Clock Tower roof.
- **Shared Items: one item for both games, rule by rule.** Randomizer → Cross-Game → "Shared Items" holds OoTMM's
  "Shared <Item>" rules in OoTMM's order: Fire, Ice and Light Arrows, Epona's Song, Song of Storms, Song of Time,
  Lens of Truth, Goron Mask, Zora Mask, Bunny Hood, Keaton Mask, Mask of Truth, Kokiri Sword, Shields, Ocarina
  Buttons, Bottles and Swim. Each is frozen into the world. All are off by default except Shared Swim, which is on
  and only matters when a game shuffles swimming. The Cross-Game Masks and Cross-Game Songs sections have more
  Shared rules. With a rule on, one copy is placed for both games, and finding it in either game gives it to the
  other game at your next crossing. Finding the other game's copy
  where you are playing also gives it to you there at once (issue #1551; the Kokiri Sword and bottles already
  worked this way). One exception: a Majora's Mask sword found after the shared sword is held still crosses,
  because Majora's Mask turns it into the Razor or Gilded Sword. With Shuffle Songs set to Anywhere and
  Link's Pocket set to Advancement, Link's Pocket no longer takes Epona's Song, the Song of Storms or the Song of
  Time when its Shared rule is on (issue #1552). What the rules do not do yet is under "Randomizer generation"
  below.
- **Shuffle Chest Minigame and Shuffle Silver Rupees work in a paired world** (issue #1530). The Treasure Chest
  Game keys (or key ring) and the silver rupees and pouches are placed in Ocarina of Time only, never on a
  Majora's Mask check.
- **You choose the goal, and meeting it ends the paired game.** Randomizer → Cross-Game
  → "[Both Games] Goal" offers OoTMM's goals: Ganon & Majora (the default), Any
  Final Boss, Triforce Hunt, Ganon and Majora. A goal that leaves a half unproved says so with a toast
  at creation ("Not proven: Majora's Mask may be unfinishable."; PR [#760](https://github.com/penelopeduncan/redshipblueship/pull/760)). The
  ending plays only in the game whose final-boss defeat meets the goal; a final boss
  beaten earlier is recorded and puts you back in its game, in Ganon's Tower or at a new
  cycle in South Clock Town (PR [#769](https://github.com/penelopeduncan/redshipblueship/pull/769)). A half's own triforce hunt no longer ends a
  paired world under a boss goal, and OoT's own "Win" mode is generated as "Ganon's
  Boss Key" there (PR [#775](https://github.com/penelopeduncan/redshipblueship/pull/775)).
- **Ocarina of Time's goal gates follow OoTMM** ([#1428](https://github.com/penelopeduncan/redshipblueship/issues/1428)).
  Rainbow Bridge defaults to Medallions with all six required, and Ganon's Boss Key to
  Start With, the closest Ship of Harkinian value to OoTMM's "Removed": you hold the key,
  so the door to Ganondorf opens, and the Tower Boss Key Chest holds a random item that
  the Check Tracker lists. The Light Arrow Cutscene still needs the Shadow and Spirit
  Medallions. With Triforce Hunt on, the hunt still gives you Ganon's Boss Key. The
  Standard and Beginner presets and "Reset to Default" use these, and the Standard and
  Beginner presets also shuffle the Master Sword and the Gerudo Membership Card, as
  OoTMM's defaults do, while the Ocarina of Time stays shuffled as before (you still
  start with the Fairy Ocarina) (operator ruling 2026-11-05, PR [#1486](https://github.com/penelopeduncan/redshipblueship/pull/1486));
  the Advanced and Hell Mode presets keep their own values.
- **Every randomizer file is a paired file.** Every OoT randomizer generation is a
  paired creation, so a single-game OoT randomizer file cannot be created (PR [#775](https://github.com/penelopeduncan/redshipblueship/pull/775)).

**What does not ship yet:**

- **Not yet played by hand:** a crossing after quitting and relaunching the game, a crossing after a save made
  during play, "[Both Games] Shared Kokiri Sword" on, MM's game-over prompt
  ([#694](https://github.com/penelopeduncan/redshipblueship/issues/694)), and the cross-game warp songs (the Song of
  Soaring in Hyrule and Ocarina of Time's warp songs in Termina, with their shared variants; all off by default
  and covered by automated runs). Older playtest notes are in [`playtest-2026-09.md`](playtest-2026-09.md) and
  under "Playtest, 2026-10-17, `7e44b8a0`" below.
  The playtest is [`playtest-2026-09.md`](playtest-2026-09.md).
- ~~**The in-game crossing views lag Majora's Mask.**~~ **Resolved** ([#799](https://github.com/penelopeduncan/redshipblueship/issues/799)).
  Randomizer > Check Tracker > Toggle Cross-Game Spoiler lists every crossing (the Combo Tracker's Crossings section was
  cut in PR [#1137](https://github.com/penelopeduncan/redshipblueship/pull/1137))
  in both directions, by name, with whether each host check was collected. That state
  comes from each game's own save. Before #799, Majora's Mask's was read only from the
  copy written at the last game switch or save, and arriving in Termina empties that
  copy, so while Majora's Mask was played the tracker's Majora's Mask panel read "No data
  yet" and every crossing hosted there read "?" until Majora's Mask's first save or the
  next switch. Now, while Majora's Mask is played, both windows read its running save,
  and a crossing collected there shows as collected at once. From
  Hyrule, Majora's Mask's is as of the last game switch or save (as of file creation
  until Majora's Mask is first entered), which the switch itself writes (#755, #757).
- **Paired-world hints are partial.** ~~OoT's hints have no pair-level Way of the Hero.~~
  **RESOLVED ([#1439](https://github.com/penelopeduncan/redshipblueship/issues/1439)):** both games' gossip stones
  name areas on the pair's Way of the Hero, OoTMM's largest default hint bucket: an area holding something the paired
  goal cannot do without, in either game ("They say that the Southern Swamp is on the way of the hero." in Hyrule,
  "They say Kokiri Forest is on the Way of the Hero." in Termina). It covers the items the single bag placed, not
  the ones each game's own restricted pass placed (Ocarina of Time's reward-spot songs under Dungeon Rewards, for
  example), and No Logic claims none. Songs are never path hints unless songs are shuffled Anywhere (OoTMM's rule).
  Foolish hints work again: each game calls one of its own areas foolish when none of
  its checks holds a crossing, an item the single bag placed or one the game counts as
  progression (OoTMM's rule without its probabilistic refinement; OoT's own Foolish
  entry, and a "plundering ... is a foolish choice" variant on Majora's Mask's free
  gossip stones, [#575](https://github.com/penelopeduncan/redshipblueship/issues/575) H5a).
  OoT's gossip stones may hint an OoT check that holds a Majora's Mask
  item, and name that item with " (MM)" (the shared Gerudo, Skull and Spooky Masks and the shared warp songs,
  while their Shared rule is on, have no tag), never the Blue Rupee the check holds
  ([#575](https://github.com/penelopeduncan/redshipblueship/issues/575) H3b). An OoT item that crossed is
  hinted by the Majora's Mask area that holds it, for example "the Southern Swamp"
  (PR [#743](https://github.com/penelopeduncan/redshipblueship/pull/743), [#575](https://github.com/penelopeduncan/redshipblueship/issues/575) H3a);
  ~~a file created before that still says "Termina".~~ Such a file is refused since
  PR [#895](https://github.com/penelopeduncan/redshipblueship/pull/895) (see "Back up your saves").
  Majora's Mask's own hints (gossip stones, and the Oath, remains, Hookshot and gold dust
  hints) name a check's area the way OoTMM does ("in the Southern Swamp", "around Goron
  Village"), not its scene, so a grotto no longer reads "Lone Peak Shrine & Grottos" and
  an enemy drop is never hinted. ~~An MM item that crossed into Hyrule is still hinted as
  "nowhere".~~ **RESOLVED (#575 H3c):** an MM item that crossed onto an Ocarina of Time
  check is named by that check's Hyrule area ("is hidden on Death Mountain Trail"), by
  the static hints and by the gossip stones, which now draw such items after Termina's
  own checks ([#575](https://github.com/penelopeduncan/redshipblueship/issues/575) H2, H3c).
- ~~**Triforce Hunt is not yet a paired goal.**~~ RESOLVED ([#1389](https://github.com/penelopeduncan/redshipblueship/issues/1389)):
  a paired world is generated under it, with OoTMM's [Both Games] Triforce Pieces (default 30, at most 255, the most
  Ocarina of Time's counter holds; OoTMM allows 999) and Triforce Goal (default 20). The pieces are split between the
  two games' own checks, one count across both, and the game ends in whichever game the last one is found. OoTMM's
  Triforce Quest is still not offered (PRs [#740](https://github.com/penelopeduncan/redshipblueship/pull/740), [#760](https://github.com/penelopeduncan/redshipblueship/pull/760), [#775](https://github.com/penelopeduncan/redshipblueship/pull/775)).
- ~~**Some settings re-seed the world without changing a rule.**~~ RESOLVED: the pool-size
  sliders ([#801](https://github.com/penelopeduncan/redshipblueship/issues/801)) and the
  "OoT Classes" / "MM Classes" rows ([#834](https://github.com/penelopeduncan/redshipblueship/issues/834))
  are retired, so every Cross-Game Rule left changes a rule (PRs [#817](https://github.com/penelopeduncan/redshipblueship/pull/817), [#839](https://github.com/penelopeduncan/redshipblueship/pull/839)).
- **Surplus filler can be dropped** when more MM items land in Hyrule than OoT items
  leave; only filler gives way (PR [#743](https://github.com/penelopeduncan/redshipblueship/pull/743)).
- **Netplay and crossings share the cross-game outbox.** It holds 2048 items, as many as one game can host.
  Crossings alone cannot fill it, but a netplay peer's grants take slots too. If it is full, a pickup is held with
  a toast and is not lost.
- **Generation can still abort.** A budget stop (2500 fill rounds per
  attempt and 5000 per creation since PR [#1285](https://github.com/penelopeduncan/redshipblueship/pull/1285);
  1250 and 2500 before it) fails Generate with a toast ("Not generated: try a new seed or other
  options."); before PR [#1116](https://github.com/penelopeduncan/redshipblueship/pull/1116) generation ran at file creation, and the same stop failed the creation
  ("Not created: ..."). The stop depends only on the seed and settings, never on the machine (PR
  [#959](https://github.com/penelopeduncan/redshipblueship/pull/959)). No creation in
  the 30-seed sample needed more than two fill batches, which bounds the failure rate
  below about 10%, not at zero. No partial or corrupt file is left behind. When one game's
  own items outnumber its checks, which no seed can fix, the toast names that game instead
  ("Not generated: too many Majora's Mask pieces.", [#1483](https://github.com/penelopeduncan/redshipblueship/issues/1483)).
- **Some MM randomizer options are drawn disabled on purpose.** "Majora Access: Remains" and "Final Hours Start
  Time" on Randomizer → MM Randomizer are retired ("Option is Retired"; ADR 0010 answer O1, and
  [#1229](https://github.com/penelopeduncan/redshipblueship/issues/1229)), and seven rows read "Not Yet Supported in
  Paired Worlds" ([#1403](https://github.com/penelopeduncan/redshipblueship/issues/1403)). No MM
  game-hook type is registered but undispatched in the single-executable build since PR
  [#673](https://github.com/penelopeduncan/redshipblueship/pull/673) closed [#438](https://github.com/penelopeduncan/redshipblueship/issues/438), so no
  other row is disabled for a missing hook; an option that is enabled and does nothing
  is a bug worth reporting.
- **MM's enhancement toggles sit beside their Ship of Harkinian twins** (since PR [#1248](https://github.com/penelopeduncan/redshipblueship/pull/1248); they were on Combo → Majora's Mask, named MM Enhancements until 2026-09-27).
  The game-over prompt and `BetterSongOfDoubleTime` are Game Over Prompt and Better Double Time in the "Majora's Mask"
  block of Enhancements → Quality of Life; `SkipSoTCutscenes` is Skip Time Song Cutscenes, right after Skip Song
  Cutscenes on Enhancements → Skips & Speed-ups; the shared `Autosave` checkbox on Quality of Life applies to both
  games, and (since PR [#730](https://github.com/penelopeduncan/redshipblueship/pull/730), [#693](https://github.com/penelopeduncan/redshipblueship/issues/693)) MM's own autosave interval slider, MM Autosave Interval
  (`gEnhancements.Saving.AutosaveInterval`, 1–60 minutes, default 5), sits directly under it while
  Autosave is on (PR [#695](https://github.com/penelopeduncan/redshipblueship/pull/695), [#682](https://github.com/penelopeduncan/redshipblueship/issues/682)). The slider sets
  Majora's Mask's interval only; Ocarina of Time's autosave interval is a fixed
  3 minutes. The rest of Majora's Mask's settings joined them on 2026-10-27 (issue
  [#1233](https://github.com/penelopeduncan/redshipblueship/issues/1233); release notes, "Majora's Mask settings in the shared menu").

**Problems found in earlier playtests** (all fixed; kept so that old reports can be matched to their fix. Open
issues are listed under the open-issues heading below):

**~~Shops sell only their own game's items~~ — plain shops, Business Scrubs, Tingle, OoT's merchants and the Treasure Chest Game RESOLVED** ([#800](https://github.com/penelopeduncan/redshipblueship/issues/800)).
#800's first pass makes plain shops cross-game in both directions. An Ocarina of Time shop
can sell a Majora's Mask item: with OoT's "Shop Shuffle" on, a shelf that shop shuffle
emptied can hold an MM item. The shelf shows the MM item's model (or the mystery item when
Ocarina of Time cannot draw that model yet), the textbox names it at the shelf's price, and
buying it sends the item to Majora's Mask like a cross-game chest. A Majora's Mask shop slot
(the shelves, the Bomb Shop owner's hand item, Gorman's milk and the Milk Bar) can hold an
Ocarina of Time item, shown with its own model and name, sold once for the whole game and
delivered on your next arrival in Hyrule. Two MM shop slots are shuffled in every world (the
Curiosity Shop's special and the Bomb Shop's fourth item), so this can happen even with MM's
"Shuffle Shops" off. #800's second pass adds OoT's Business Scrubs: with OoT's "Scrubs
Shuffle" on, a scrub can sell a Majora's Mask item at its own price, its offer names the item
(unless "Merchant Hint Text" is off or Mysterious Shuffle is on, as for its own items), and buying it sends the item
to Majora's Mask like a cross-game chest. It also adds MM's Tingle: with MM's "Shuffle Tingle
Maps" on, either or both of his two offers can be an Ocarina of Time item at its own price; his
offer names it, he sells it once for the whole game, and it is delivered on your next arrival in
Hyrule. It also adds OoT's merchants (the Magic Bean Salesman, Medigoron, Granny's Shop and the
carpet salesman): with OoT's "Shuffle Merchants" on, a merchant it shuffles can sell a Majora's
Mask item at its own price, its offer names the item (unless "Merchant Hint Text" is off or
Mysterious Shuffle is on), and buying it sends the item to Majora's Mask like a cross-game
chest. It also adds the Treasure Chest Game's prize: the final chest can hold a Majora's Mask
item in any world (no setting needed), the Lens of Truth shows that item's model above the
chest, and opening it sends the item to Majora's Mask like a cross-game chest. The game's other
chests never hold a Majora's Mask item. With Ocarina of Time's Shuffle Chest Minigame on, which now works in a
paired world, those chests and the host's paid item hold shuffled Ocarina of Time items, and the six Chest Game
keys (or the key ring) are placed in Ocarina of Time only. Shop
shuffle, scrub shuffle, merchant shuffle and Tingle map shuffle are off by default (OoT's
"Shop Shuffle", "Scrubs Shuffle" and "Shuffle Merchants", MM's "Shuffle Shops" and "Shuffle
Tingle Maps"). MM's shop stock joins
the shared bag when MM's "Shuffle Shops" is on, so an MM shop item can turn up in an OoT
chest or on an OoT shelf; OoT's shop stock never leaves Hyrule.

**~~The Check Trackers name "Blue Rupee" or "Junk" for a cross-game chest~~ — RESOLVED** ([#796](https://github.com/penelopeduncan/redshipblueship/issues/796), PR [#813](https://github.com/penelopeduncan/redshipblueship/pull/813)).
Fixed by PR #813: once a cross-game chest is collected, Ocarina of Time's Check Tracker lists it
with the item found and "(MM)", for example "(Bunny Hood (MM))", and the MM Check Tracker with
the item and "(OoT)". OoT's tracker search finds the chest by that name too. The original
report: both trackers named the placeholder the chest holds in its own game's tables, "(Blue
Rupee)" in OoT and "(Junk)" in MM, while the pickup message named the real item.

**~~Do not open Ocarina of Time's Save Editor or Message Viewer while you are in Majora's Mask~~ — RESOLVED** ([#797](https://github.com/penelopeduncan/redshipblueship/issues/797)).
Fixed: OoT's Save Editor, Value Viewer, Message Viewer, Gameplay Stats, Check, Entrance and
Item Trackers and Time Splits now draw only while Ocarina of Time is running, and so do
their pages in the menu (Dev Tools > Save Editor and the others). In Termina they stay open
but are not drawn; back in Hyrule they come back as they were. The original report:
~~The two games share one save buffer, and OoT's windows do not yet check which game is
running. In Termina, OoT's Save Editor rewrites health and magic values through OoT's save
layout on every frame it draws, and those bytes are Majora's Mask's save. Opening its page in
the menu (Dev Tools > Save Editor) is enough, even with the window closed. The Message
Viewer's "Display Message" crashes the game there, and clicking the last split in Time
Splits writes into MM's save too. OoT's Item Tracker, Gameplay Stats and Value Viewer show
garbage in Termina, and its Check and Entrance Trackers show "Waiting for file load...".
Use these windows in Hyrule only. MM's own Item and Check Trackers are not affected.~~

**~~Do not use Ocarina of Time's console commands while you are in Majora's Mask~~ — RESOLVED** ([#798](https://github.com/penelopeduncan/redshipblueship/issues/798)).
Fixed: every Ocarina of Time console command now runs only while Ocarina of Time is running.
In Termina the console answers that the command is an Ocarina of Time command and changes
nothing. Toggling "Fix Broken Giant's Knife Bug" in Termina, or anywhere outside gameplay,
now only changes the setting. The original report:
~~OoT's console commands `map`, `rupee`, `bottle`, `bItem`, `item`, `give_item` and
`entrance` do the same kind of damage in Termina as its Save Editor (they write MM's save or
crash), and so can toggling OoT's "Fix Broken Giant's Knife Bug" setting there. Use them in
Hyrule only.~~

~~**The Combo Tracker's Majora's Mask panel reads "No data yet" while you play Majora's Mask**~~ — RESOLVED ([#799](https://github.com/penelopeduncan/redshipblueship/issues/799)).
The panel read Majora's Mask only from the copy kept while you are in the other game, and
arriving in Termina uses that copy up, so from arrival until your first save in Majora's Mask
or until you left Termina again the panel said "No data yet", the crossings hosted in MM
checks showed a question-mark icon, and their list gave no collected count (seen on screen
before the fix). Now, while you play Majora's Mask, the panel and the crossing list read its
running save, and a collected check shows at once, with no save. In Hyrule it reads the copy,
as of the last game switch or save. (Since PR [#929](https://github.com/penelopeduncan/redshipblueship/pull/929)
the panel prints no note saying which of the two it shows.)

**~~The Combo Tracker's Ocarina of Time panel lists more checks than Ocarina of Time's Check Tracker~~ — RESOLVED** ([#1126](https://github.com/penelopeduncan/redshipblueship/issues/1126)). The panel listed every location the seed put an item on, excluded locations and unshuffled shop slots included, so its list and "Checks: n / m" did not match the Check Tracker's, and Always Show Gold Skulltulas and Hide Unshuffled Shop Item Checks did nothing there. Now, on a randomizer file, the panel lists what the Check Tracker lists; while you play Majora's Mask it keeps the list from your last time in Hyrule.

**~~The Combo Item Tracker lists Majora's Mask's items by name until Majora's Mask has started~~ — RESOLVED** ([#963](https://github.com/penelopeduncan/redshipblueship/issues/963), PR [#1043](https://github.com/penelopeduncan/redshipblueship/pull/1043); [#458](https://github.com/penelopeduncan/redshipblueship/issues/458)).
Fixed by PR #1043 (2026-10-20): the curated archive Ocarina of Time mounts at launch
(`redship-oot.o2r`, rebuilt from your extracted game data when its stamp is old) now carries
Majora's Mask's item icons, so the overlay draws them from the first frame, before Majora's Mask
has run. Not yet looked at by hand. In the same overlay, PR
[#1051](https://github.com/penelopeduncan/redshipblueship/pull/1051)
([#962](https://github.com/penelopeduncan/redshipblueship/issues/962)) moved the default position
from the HUD's hearts to the left edge just below the hearts and magic bar. A position an earlier
build saved in `imgui.ini` is kept: press Randomizer > Item Tracker > Move to Default Position once
(shown while Window Type is Floating; it was on Combo > Tracker Settings until PR [#1202](https://github.com/penelopeduncan/redshipblueship/pull/1202)). The original report:
~~Majora's Mask's item icons come from its own archive and are loaded the first time Majora's
Mask starts in a session. Until then (in a session that begins in Ocarina of Time, until your
first crossing into Termina) the overlay's Majora's Mask section shows each item's name
instead of its icon, dimmed while you do not hold it, with its count where it has one. The
items are the same ones the icons would show. Once Majora's Mask has started, its icons stay
loaded until you quit. Ocarina of Time's icons load at launch, so its section and the Shared
section always have icons.~~

### Open issues at `bc09231c`

**Closed since this list was refreshed, in `v0.5.1-alpha`:** [#1165](https://github.com/penelopeduncan/redshipblueship/issues/1165) (PR [#1584](https://github.com/penelopeduncan/redshipblueship/pull/1584)), [#1169](https://github.com/penelopeduncan/redshipblueship/issues/1169) (PR [#1559](https://github.com/penelopeduncan/redshipblueship/pull/1559); what it left is [#1552](https://github.com/penelopeduncan/redshipblueship/issues/1552), [#1560](https://github.com/penelopeduncan/redshipblueship/issues/1560) and [#1561](https://github.com/penelopeduncan/redshipblueship/issues/1561)), [#1363](https://github.com/penelopeduncan/redshipblueship/issues/1363) (PR [#1512](https://github.com/penelopeduncan/redshipblueship/pull/1512)), [#1364](https://github.com/penelopeduncan/redshipblueship/issues/1364) (PR [#1515](https://github.com/penelopeduncan/redshipblueship/pull/1515)), [#1374](https://github.com/penelopeduncan/redshipblueship/issues/1374), [#1398](https://github.com/penelopeduncan/redshipblueship/issues/1398) (PR [#1516](https://github.com/penelopeduncan/redshipblueship/pull/1516)), [#1403](https://github.com/penelopeduncan/redshipblueship/issues/1403) (PR [#1556](https://github.com/penelopeduncan/redshipblueship/pull/1556)), [#1404](https://github.com/penelopeduncan/redshipblueship/issues/1404) (PR [#1555](https://github.com/penelopeduncan/redshipblueship/pull/1555)), [#1405](https://github.com/penelopeduncan/redshipblueship/issues/1405) (PR [#1558](https://github.com/penelopeduncan/redshipblueship/pull/1558)), [#1406](https://github.com/penelopeduncan/redshipblueship/issues/1406) (PR [#1510](https://github.com/penelopeduncan/redshipblueship/pull/1510)), [#1407](https://github.com/penelopeduncan/redshipblueship/issues/1407) (PR [#1514](https://github.com/penelopeduncan/redshipblueship/pull/1514)), [#1408](https://github.com/penelopeduncan/redshipblueship/issues/1408) (PR [#1562](https://github.com/penelopeduncan/redshipblueship/pull/1562)), [#1409](https://github.com/penelopeduncan/redshipblueship/issues/1409) (PR [#1564](https://github.com/penelopeduncan/redshipblueship/pull/1564)), [#1410](https://github.com/penelopeduncan/redshipblueship/issues/1410) (PR [#1513](https://github.com/penelopeduncan/redshipblueship/pull/1513)), [#1414](https://github.com/penelopeduncan/redshipblueship/issues/1414) (PR [#1511](https://github.com/penelopeduncan/redshipblueship/pull/1511)), [#1447](https://github.com/penelopeduncan/redshipblueship/issues/1447), [#1453](https://github.com/penelopeduncan/redshipblueship/issues/1453) (PR [#1504](https://github.com/penelopeduncan/redshipblueship/pull/1504)), [#1465](https://github.com/penelopeduncan/redshipblueship/issues/1465) (PR [#1521](https://github.com/penelopeduncan/redshipblueship/pull/1521)), [#1475](https://github.com/penelopeduncan/redshipblueship/issues/1475) (PR [#1528](https://github.com/penelopeduncan/redshipblueship/pull/1528)), [#1479](https://github.com/penelopeduncan/redshipblueship/issues/1479) (PR [#1523](https://github.com/penelopeduncan/redshipblueship/pull/1523)), [#1481](https://github.com/penelopeduncan/redshipblueship/issues/1481) (PR [#1524](https://github.com/penelopeduncan/redshipblueship/pull/1524)), [#1482](https://github.com/penelopeduncan/redshipblueship/issues/1482) (PR [#1526](https://github.com/penelopeduncan/redshipblueship/pull/1526)), [#1483](https://github.com/penelopeduncan/redshipblueship/issues/1483) (PRs [#1522](https://github.com/penelopeduncan/redshipblueship/pull/1522) and [#1563](https://github.com/penelopeduncan/redshipblueship/pull/1563)), [#1484](https://github.com/penelopeduncan/redshipblueship/issues/1484) (PR [#1527](https://github.com/penelopeduncan/redshipblueship/pull/1527)), [#1487](https://github.com/penelopeduncan/redshipblueship/issues/1487) (PR [#1519](https://github.com/penelopeduncan/redshipblueship/pull/1519)), [#1498](https://github.com/penelopeduncan/redshipblueship/issues/1498) (PR [#1506](https://github.com/penelopeduncan/redshipblueship/pull/1506)), [#1499](https://github.com/penelopeduncan/redshipblueship/issues/1499) (PR [#1507](https://github.com/penelopeduncan/redshipblueship/pull/1507)), [#1500](https://github.com/penelopeduncan/redshipblueship/issues/1500) (PR [#1505](https://github.com/penelopeduncan/redshipblueship/pull/1505)), [#1501](https://github.com/penelopeduncan/redshipblueship/issues/1501) (PR [#1508](https://github.com/penelopeduncan/redshipblueship/pull/1508)) and [#1502](https://github.com/penelopeduncan/redshipblueship/issues/1502) (PR [#1531](https://github.com/penelopeduncan/redshipblueship/pull/1531)). Their lines below were written for `v0.5.0-alpha`; the release notes say what changed.

Every issue open at this commit that this project's agents filed, one line each, with what a
player sees. Epics and trackers are listed for what they leave out of this build. The Combo Tracker and the Combo Item
Tracker are named the Check Tracker and the Item Tracker since PR [#1192](https://github.com/penelopeduncan/redshipblueship/pull/1192); lines written before it keep the old
names. (Refreshed 2026-11-09 at `bc09231c`, the code of `v0.5.0-alpha` plus PRs [#1496](https://github.com/penelopeduncan/redshipblueship/pull/1496), [#1497](https://github.com/penelopeduncan/redshipblueship/pull/1497) and [#1503](https://github.com/penelopeduncan/redshipblueship/pull/1503) (the version bump, the release workflow and the renamed account; no game code): 49 issues are open. Filed since the `4f37bf18` list and still open: [#1363](https://github.com/penelopeduncan/redshipblueship/issues/1363), [#1364](https://github.com/penelopeduncan/redshipblueship/issues/1364), [#1374](https://github.com/penelopeduncan/redshipblueship/issues/1374), [#1393](https://github.com/penelopeduncan/redshipblueship/issues/1393), [#1394](https://github.com/penelopeduncan/redshipblueship/issues/1394), [#1398](https://github.com/penelopeduncan/redshipblueship/issues/1398), [#1403](https://github.com/penelopeduncan/redshipblueship/issues/1403), [#1404](https://github.com/penelopeduncan/redshipblueship/issues/1404), [#1405](https://github.com/penelopeduncan/redshipblueship/issues/1405), [#1406](https://github.com/penelopeduncan/redshipblueship/issues/1406), [#1407](https://github.com/penelopeduncan/redshipblueship/issues/1407), [#1408](https://github.com/penelopeduncan/redshipblueship/issues/1408), [#1409](https://github.com/penelopeduncan/redshipblueship/issues/1409), [#1410](https://github.com/penelopeduncan/redshipblueship/issues/1410), [#1414](https://github.com/penelopeduncan/redshipblueship/issues/1414), [#1417](https://github.com/penelopeduncan/redshipblueship/issues/1417), [#1418](https://github.com/penelopeduncan/redshipblueship/issues/1418), [#1419](https://github.com/penelopeduncan/redshipblueship/issues/1419), [#1447](https://github.com/penelopeduncan/redshipblueship/issues/1447), [#1452](https://github.com/penelopeduncan/redshipblueship/issues/1452), [#1453](https://github.com/penelopeduncan/redshipblueship/issues/1453), [#1465](https://github.com/penelopeduncan/redshipblueship/issues/1465), [#1467](https://github.com/penelopeduncan/redshipblueship/issues/1467), [#1475](https://github.com/penelopeduncan/redshipblueship/issues/1475), [#1479](https://github.com/penelopeduncan/redshipblueship/issues/1479), [#1481](https://github.com/penelopeduncan/redshipblueship/issues/1481), [#1482](https://github.com/penelopeduncan/redshipblueship/issues/1482), [#1483](https://github.com/penelopeduncan/redshipblueship/issues/1483), [#1484](https://github.com/penelopeduncan/redshipblueship/issues/1484), [#1487](https://github.com/penelopeduncan/redshipblueship/issues/1487), [#1498](https://github.com/penelopeduncan/redshipblueship/issues/1498), [#1499](https://github.com/penelopeduncan/redshipblueship/issues/1499), [#1500](https://github.com/penelopeduncan/redshipblueship/issues/1500), [#1501](https://github.com/penelopeduncan/redshipblueship/issues/1501) and [#1502](https://github.com/penelopeduncan/redshipblueship/issues/1502). Closed since it: [#1336](https://github.com/penelopeduncan/redshipblueship/issues/1336) to [#1346](https://github.com/penelopeduncan/redshipblueship/issues/1346) (the eleven backport packets, merged in the `v0.5.0-alpha` backport waves; release notes, "Upstream SoH (9.1.2–9.2.0) and 2Ship2Harkinian (3.0.0–4.0.1) Backports"), [#1351](https://github.com/penelopeduncan/redshipblueship/issues/1351) (PR [#1359](https://github.com/penelopeduncan/redshipblueship/pull/1359)), [#1196](https://github.com/penelopeduncan/redshipblueship/issues/1196) (PR [#1385](https://github.com/penelopeduncan/redshipblueship/pull/1385)), [#1286](https://github.com/penelopeduncan/redshipblueship/issues/1286) (PR [#1383](https://github.com/penelopeduncan/redshipblueship/pull/1383)), [#1145](https://github.com/penelopeduncan/redshipblueship/issues/1145) (PR [#1381](https://github.com/penelopeduncan/redshipblueship/pull/1381)), [#1189](https://github.com/penelopeduncan/redshipblueship/issues/1189) (PR [#1382](https://github.com/penelopeduncan/redshipblueship/pull/1382)), [#978](https://github.com/penelopeduncan/redshipblueship/issues/978) (PR [#1387](https://github.com/penelopeduncan/redshipblueship/pull/1387)), [#980](https://github.com/penelopeduncan/redshipblueship/issues/980) (PR [#1450](https://github.com/penelopeduncan/redshipblueship/pull/1450); what it left is [#1452](https://github.com/penelopeduncan/redshipblueship/issues/1452)), [#1434](https://github.com/penelopeduncan/redshipblueship/issues/1434) (PR [#1446](https://github.com/penelopeduncan/redshipblueship/pull/1446)), [#1170](https://github.com/penelopeduncan/redshipblueship/issues/1170) (not planned: by design, line below), and [#645](https://github.com/penelopeduncan/redshipblueship/issues/645) and [#500](https://github.com/penelopeduncan/redshipblueship/issues/500) (closed by the operator on 2026-11-03). The issues filed and closed between the two lists are the ones the `v0.5.0-alpha` release notes name. The list at `4f37bf18` was refreshed 2026-11-02 at the code of `v0.4.4-alpha` plus PRs #1353 to #1357, with 35 issues open. Filed since the `347ba755` list and still open: [#1286](https://github.com/penelopeduncan/redshipblueship/issues/1286) (the creation overlay reads near the budget while the fill is not), the eleven upstream backport packets [#1336](https://github.com/penelopeduncan/redshipblueship/issues/1336) to [#1346](https://github.com/penelopeduncan/redshipblueship/issues/1346), and [#1351](https://github.com/penelopeduncan/redshipblueship/issues/1351), one of the six defects found in `v0.4.4-alpha`'s code on 2026-11-02, with an open pull request ([#1359](https://github.com/penelopeduncan/redshipblueship/pull/1359)). The other five, [#1347](https://github.com/penelopeduncan/redshipblueship/issues/1347) to [#1350](https://github.com/penelopeduncan/redshipblueship/issues/1350) and [#1352](https://github.com/penelopeduncan/redshipblueship/issues/1352), were fixed on `main` after the tag by PRs [#1353](https://github.com/penelopeduncan/redshipblueship/pull/1353) to [#1357](https://github.com/penelopeduncan/redshipblueship/pull/1357) and are closed; they stay below, struck through, for what `v0.4.4-alpha` still shows. Closed on 2026-11-02, each with a closing comment: [#1233](https://github.com/penelopeduncan/redshipblueship/issues/1233) with its sections [#1235](https://github.com/penelopeduncan/redshipblueship/issues/1235), [#1236](https://github.com/penelopeduncan/redshipblueship/issues/1236) and [#1237](https://github.com/penelopeduncan/redshipblueship/issues/1237) (the Majora's Mask settings programme resolved 167 of 174 actionable rows; what remains is [#1238](https://github.com/penelopeduncan/redshipblueship/issues/1238), [#1231](https://github.com/penelopeduncan/redshipblueship/issues/1231) and [#1272](https://github.com/penelopeduncan/redshipblueship/issues/1272)), [#966](https://github.com/penelopeduncan/redshipblueship/issues/966) (not planned: superseded by #1233, and all 35 issues it filed are closed), [#976](https://github.com/penelopeduncan/redshipblueship/issues/976) (PR [#1002](https://github.com/penelopeduncan/redshipblueship/pull/1002) and its three deferred items, PRs #1088, #1094 and #1095) and [#977](https://github.com/penelopeduncan/redshipblueship/issues/977) (the groundwork of PR [#1112](https://github.com/penelopeduncan/redshipblueship/pull/1112); what is left moved to [#979](https://github.com/penelopeduncan/redshipblueship/issues/979) and [#980](https://github.com/penelopeduncan/redshipblueship/issues/980)), [#577](https://github.com/penelopeduncan/redshipblueship/issues/577) (every model phase merged; the optional libultraship fork change for `ResourceIdentifier.Parent` can ride the masks work), [#564](https://github.com/penelopeduncan/redshipblueship/issues/564) (the one-game audit's last row, V15, world identity on netplay grants, moved to [#574](https://github.com/penelopeduncan/redshipblueship/issues/574)), [#497](https://github.com/penelopeduncan/redshipblueship/issues/497) (delivered by PR [#687](https://github.com/penelopeduncan/redshipblueship/pull/687); the pages it left are moot since the Combo header's retirement), [#492](https://github.com/penelopeduncan/redshipblueship/issues/492) (every Phase 3.1 child closed), [#925](https://github.com/penelopeduncan/redshipblueship/issues/925) and [#824](https://github.com/penelopeduncan/redshipblueship/issues/824) (the per-process test directories of PR [#931](https://github.com/penelopeduncan/redshipblueship/pull/931) and PR [#1118](https://github.com/penelopeduncan/redshipblueship/pull/1118), locked by `TestScratchPoisonedCwd`), [#1151](https://github.com/penelopeduncan/redshipblueship/issues/1151) (not planned: the packages it would rescue carry generator version 1 and are refused before the size check it targeted), [#1301](https://github.com/penelopeduncan/redshipblueship/issues/1301) (already fixed here: `Logic.cpp`'s per-registration event guard since #580; the Snowhead bridge-room chest line of the same 2Ship2Harkinian commit is a candidate for the next Majora's Mask backport wave) and [#310](https://github.com/penelopeduncan/redshipblueship/issues/310) (not planned: the integration tier, the playtests of `v0.2.0-prealpha` to `v0.4.4-alpha` and the release AppImages cover it; a sanitizer pass and an AppImage smoke test on real hardware were not carried forward). Closed by the operator with no closing comment: [#1159](https://github.com/penelopeduncan/redshipblueship/issues/1159) (2026-10-29) and [#1063](https://github.com/penelopeduncan/redshipblueship/issues/1063) (2026-10-31). Closed since the `347ba755` list: [#1274](https://github.com/penelopeduncan/redshipblueship/issues/1274) (PR [#1315](https://github.com/penelopeduncan/redshipblueship/pull/1315)) and [#1070](https://github.com/penelopeduncan/redshipblueship/issues/1070) (2026-10-29, after the operator's retest; "Woodfall Temple does not rise" under "Majora's Mask specific"). Filed and closed between the two lists, each by the pull request in parentheses (release notes, `v0.4.1-alpha`): [#1276](https://github.com/penelopeduncan/redshipblueship/issues/1276) (PR [#1316](https://github.com/penelopeduncan/redshipblueship/pull/1316)), [#1277](https://github.com/penelopeduncan/redshipblueship/issues/1277) (PR [#1291](https://github.com/penelopeduncan/redshipblueship/pull/1291)), [#1278](https://github.com/penelopeduncan/redshipblueship/issues/1278) (PR [#1288](https://github.com/penelopeduncan/redshipblueship/pull/1288)), [#1279](https://github.com/penelopeduncan/redshipblueship/issues/1279), [#1280](https://github.com/penelopeduncan/redshipblueship/issues/1280) and [#1287](https://github.com/penelopeduncan/redshipblueship/issues/1287) (PR [#1293](https://github.com/penelopeduncan/redshipblueship/pull/1293)), [#1281](https://github.com/penelopeduncan/redshipblueship/issues/1281) (PR [#1295](https://github.com/penelopeduncan/redshipblueship/pull/1295)), [#1282](https://github.com/penelopeduncan/redshipblueship/issues/1282) (PR [#1290](https://github.com/penelopeduncan/redshipblueship/pull/1290)), [#1283](https://github.com/penelopeduncan/redshipblueship/issues/1283) (PR [#1297](https://github.com/penelopeduncan/redshipblueship/pull/1297)), [#1284](https://github.com/penelopeduncan/redshipblueship/issues/1284) (PR [#1304](https://github.com/penelopeduncan/redshipblueship/pull/1304)), [#1289](https://github.com/penelopeduncan/redshipblueship/issues/1289) (PR [#1292](https://github.com/penelopeduncan/redshipblueship/pull/1292)), [#1294](https://github.com/penelopeduncan/redshipblueship/issues/1294) (PR [#1309](https://github.com/penelopeduncan/redshipblueship/pull/1309)), [#1296](https://github.com/penelopeduncan/redshipblueship/issues/1296) (PR [#1312](https://github.com/penelopeduncan/redshipblueship/pull/1312)), [#1298](https://github.com/penelopeduncan/redshipblueship/issues/1298) and [#1302](https://github.com/penelopeduncan/redshipblueship/issues/1302) (PR [#1317](https://github.com/penelopeduncan/redshipblueship/pull/1317)), [#1299](https://github.com/penelopeduncan/redshipblueship/issues/1299) (PR [#1310](https://github.com/penelopeduncan/redshipblueship/pull/1310)), [#1300](https://github.com/penelopeduncan/redshipblueship/issues/1300) (PR [#1318](https://github.com/penelopeduncan/redshipblueship/pull/1318)), [#1303](https://github.com/penelopeduncan/redshipblueship/issues/1303) (PR [#1313](https://github.com/penelopeduncan/redshipblueship/pull/1313)), [#1305](https://github.com/penelopeduncan/redshipblueship/issues/1305) (PR [#1307](https://github.com/penelopeduncan/redshipblueship/pull/1307)), [#1306](https://github.com/penelopeduncan/redshipblueship/issues/1306) (PR [#1314](https://github.com/penelopeduncan/redshipblueship/pull/1314)) and [#1308](https://github.com/penelopeduncan/redshipblueship/issues/1308) (PRs [#1311](https://github.com/penelopeduncan/redshipblueship/pull/1311) and [#1319](https://github.com/penelopeduncan/redshipblueship/pull/1319), with [#1320](https://github.com/penelopeduncan/redshipblueship/pull/1320)). The list at `347ba755` was refreshed 2026-10-27 at the `v0.4.0-alpha` release commit. Filed since the earlier `347ba755` list: [#1274](https://github.com/penelopeduncan/redshipblueship/issues/1274). Closed since it: [#1134](https://github.com/penelopeduncan/redshipblueship/issues/1134) (PR [#1273](https://github.com/penelopeduncan/redshipblueship/pull/1273): the docs sweep after the Combo header's retirement, lane L8). [#1070](https://github.com/penelopeduncan/redshipblueship/issues/1070) passed its retest on 2026-10-27 and its line moved to "Woodfall Temple does not rise" under "Majora's Mask specific", marked resolved. The list at `347ba755` was refreshed 2026-10-27. Closed since the `3772e70d` list: [#1217](https://github.com/penelopeduncan/redshipblueship/issues/1217) (PR [#1226](https://github.com/penelopeduncan/redshipblueship/pull/1226): under Shuffle Jabber Nuts the Deku Theater leader gives the seed's item and no vanilla capacity upgrade), [#1222](https://github.com/penelopeduncan/redshipblueship/issues/1222) and [#1228](https://github.com/penelopeduncan/redshipblueship/issues/1228) (PR [#1250](https://github.com/penelopeduncan/redshipblueship/pull/1250): a full cross-game outbox holds a pickup with an "Outbox full" toast and gives it after the next crossing, a Majora's Mask shop or Tingle refuses the sale, and the Hags' delivered shelf no longer waits on its free potion; PR [#1264](https://github.com/penelopeduncan/redshipblueship/pull/1264) then grew the outbox to 2048 pickups), [#1141](https://github.com/penelopeduncan/redshipblueship/issues/1141) (PR [#1202](https://github.com/penelopeduncan/redshipblueship/pull/1202): the Item Tracker follows SoH's Item Tracker Settings and its rows live on Randomizer → Item Tracker), [#1143](https://github.com/penelopeduncan/redshipblueship/issues/1143) (PR [#1249](https://github.com/penelopeduncan/redshipblueship/pull/1249): it honours SoH's count modes, key counts, hookshot identifier, icon margins and text size), [#1142](https://github.com/penelopeduncan/redshipblueship/issues/1142) and [#1158](https://github.com/penelopeduncan/redshipblueship/issues/1158) (PR [#1263](https://github.com/penelopeduncan/redshipblueship/pull/1263): Ocarina of Time's dungeon items, souls, jabber nuts, overworld keys, ocarina buttons, Greg and the fishing pole, and the pooled heart pieces in the Shared column), [#1133](https://github.com/penelopeduncan/redshipblueship/issues/1133) (PR [#1248](https://github.com/penelopeduncan/redshipblueship/pull/1248): the Combo header is retired; Settings → OoT Mods and MM Mods, Dev Tools → Save Files), [#1239](https://github.com/penelopeduncan/redshipblueship/issues/1239) (PRs [#1242](https://github.com/penelopeduncan/redshipblueship/pull/1242), [#1257](https://github.com/penelopeduncan/redshipblueship/pull/1257), [#1259](https://github.com/penelopeduncan/redshipblueship/pull/1259), [#1261](https://github.com/penelopeduncan/redshipblueship/pull/1261), [#1266](https://github.com/penelopeduncan/redshipblueship/pull/1266), [#1269](https://github.com/penelopeduncan/redshipblueship/pull/1269) and [#1271](https://github.com/penelopeduncan/redshipblueship/pull/1271): Majora's Mask's Audio Editor, Cosmetics Editor and HUD rows) and [#1240](https://github.com/penelopeduncan/redshipblueship/issues/1240) (PR [#1270](https://github.com/penelopeduncan/redshipblueship/pull/1270) and the lanes before it: the 2Ship2Harkinian import carries every Majora's Mask setting with a row). The list at `3772e70d` was refreshed 2026-10-27. Closed since the `38a5138c` list: [#1210](https://github.com/penelopeduncan/redshipblueship/issues/1210) (PR [#1211](https://github.com/penelopeduncan/redshipblueship/pull/1211): a file in `presets/` that is not a preset is skipped with one log line, and a `.json` that fails to parse is logged and skipped), [#1172](https://github.com/penelopeduncan/redshipblueship/issues/1172) (PR [#1213](https://github.com/penelopeduncan/redshipblueship/pull/1213): Majora's Mask's masks and stray fairies in Hyrule's chests draw Ship of Harkinian's mask and fairy chests, Ocarina of Time's heart upgrades get the heart chest again, and Termina's dungeon keys draw 2Ship2Harkinian's key models), [#1173](https://github.com/penelopeduncan/redshipblueship/issues/1173) (PR [#1212](https://github.com/penelopeduncan/redshipblueship/pull/1212): the Hookshot reticle turns green on a hookshot surface in Termina too, and Hyrule's Bow, Slingshot and Boomerang reticles no longer do), [#1218](https://github.com/penelopeduncan/redshipblueship/issues/1218) (PR [#1223](https://github.com/penelopeduncan/redshipblueship/pull/1223): under Shuffle Master Sword a respawn out of the Ganon's Tower collapse no longer gives the unfound Master Sword), [#1191](https://github.com/penelopeduncan/redshipblueship/issues/1191) (PR [#1205](https://github.com/penelopeduncan/redshipblueship/pull/1205): the Double Cell and Steep Slope carpenters' keys award their own cells' checks; the operator accepted the save break it brings, under "Save loss and corruption"), [#1215](https://github.com/penelopeduncan/redshipblueship/issues/1215) and [#1221](https://github.com/penelopeduncan/redshipblueship/issues/1221) (PR [#1224](https://github.com/penelopeduncan/redshipblueship/pull/1224): the Razor Sword survives the Song of Time and no longer breaks, and the Oceanside Spider House reward can be collected on any day and is no longer lost to Mikau's grave squatter; #1221's third row, the Hags potion candidate, is [#1228](https://github.com/penelopeduncan/redshipblueship/issues/1228)), [#1203](https://github.com/penelopeduncan/redshipblueship/issues/1203) (PR [#1225](https://github.com/penelopeduncan/redshipblueship/pull/1225): the monkey no longer teaches the vanilla Sonata as you leave the Deku King's chamber, which closes the destroy-time residual of [#1070](https://github.com/penelopeduncan/redshipblueship/issues/1070)), [#1216](https://github.com/penelopeduncan/redshipblueship/issues/1216), [#1219](https://github.com/penelopeduncan/redshipblueship/issues/1219) and [#1220](https://github.com/penelopeduncan/redshipblueship/issues/1220) (PR [#1227](https://github.com/penelopeduncan/redshipblueship/pull/1227): the gallery and beaver rupees, the Doggy Racetrack, the Deku Playground, the lab fish and Flat's tomb no longer give a second item for one check) and [#1229](https://github.com/penelopeduncan/redshipblueship/issues/1229) (PR [#1230](https://github.com/penelopeduncan/redshipblueship/pull/1230): under Shuffle Time the moon falls at the end of the last owned half-day, and Final Hours Start Time is retired). The list at `38a5138c` was refreshed 2026-10-27. Closed between the `79540720` list and that one: [#1195](https://github.com/penelopeduncan/redshipblueship/issues/1195) (PR [#1206](https://github.com/penelopeduncan/redshipblueship/pull/1206): the symbol namespacer now skips string literals and comments and has a read-only `--audit-literals` mode; its sweep found one more lookup key damaged since January, Ocarina of Time's debug-print font resource path in `games/oot/src/code/gfxprint.c`, now restored, so a texture pack's replacement debug font is picked up in Ocarina of Time again. That closes the namespacing-damage class: Fast64's `gEmptyDL` marker (PR [#1197](https://github.com/penelopeduncan/redshipblueship/pull/1197)), the capital P's width (PR [#1198](https://github.com/penelopeduncan/redshipblueship/pull/1198)) and the gfxprint path (PR #1206); a Python invariant in `tools/tests/test_repo_invariants.py` fails on any key-shaped literal that names a game-prefixed symbol, which locks every restored literal and any new one; the prefixed literals left are console, crash-screen and debug messages), [#1187](https://github.com/penelopeduncan/redshipblueship/issues/1187) (PR [#1207](https://github.com/penelopeduncan/redshipblueship/pull/1207): a shuffled dungeon map's text box names its dungeon, the Deku Tree Compass shows the compass text, `CustomMessage::Replace` no longer loops forever when a replacement contains its search string, and Mido's hint and the boss-door hints have generation coverage in a paired world) and [#1204](https://github.com/penelopeduncan/redshipblueship/issues/1204) (PR [#1209](https://github.com/penelopeduncan/redshipblueship/pull/1209): no more "File[255] - does not exist" line per frame, about 113,000 in one session, and a foreign-item give that waits, for a scrub's text box for example, logs once with its retry count instead of once per frame). The list at `79540720` was refreshed 2026-10-27. Closed between the `d94ab061` list and that one: [#1175](https://github.com/penelopeduncan/redshipblueship/issues/1175) (PR [#1194](https://github.com/penelopeduncan/redshipblueship/pull/1194): a Powder Keg found in Hyrule arrives as one keg; the count of 0 was Majora's Mask's three-day reset, which the new Keep Items on Cycle Reset option, off by default, undoes), [#1190](https://github.com/penelopeduncan/redshipblueship/issues/1190) (PRs [#1197](https://github.com/penelopeduncan/redshipblueship/pull/1197), [#1198](https://github.com/penelopeduncan/redshipblueship/pull/1198) and [#1199](https://github.com/penelopeduncan/redshipblueship/pull/1199): the crash was a custom skeleton mod's Fast64 `gEmptyDL` limb marker, which the January namespacing had renamed in the code; the second mount of every mod archive is the intended re-apply after the curated archives, and a re-added `.otr` now releases the copy it replaced) and [#1132](https://github.com/penelopeduncan/redshipblueship/issues/1132) (L5, PR [#1192](https://github.com/penelopeduncan/redshipblueship/pull/1192)); filed and closed since: [#1200](https://github.com/penelopeduncan/redshipblueship/issues/1200) (PR [#1201](https://github.com/penelopeduncan/redshipblueship/pull/1201), a red `rando` row on `main`; see "Gates, 2026-10-26/27"). The list at `d94ab061` was refreshed 2026-10-27. Closed between the `af4d941f` list and that one: [#1176](https://github.com/penelopeduncan/redshipblueship/issues/1176) (PR [#1177](https://github.com/penelopeduncan/redshipblueship/pull/1177)), [#1161](https://github.com/penelopeduncan/redshipblueship/issues/1161) and [#1166](https://github.com/penelopeduncan/redshipblueship/issues/1166) (PR [#1180](https://github.com/penelopeduncan/redshipblueship/pull/1180)), [#1162](https://github.com/penelopeduncan/redshipblueship/issues/1162) (PR [#1181](https://github.com/penelopeduncan/redshipblueship/pull/1181)), [#1167](https://github.com/penelopeduncan/redshipblueship/issues/1167) and [#1171](https://github.com/penelopeduncan/redshipblueship/issues/1171) (PR [#1185](https://github.com/penelopeduncan/redshipblueship/pull/1185)), [#1163](https://github.com/penelopeduncan/redshipblueship/issues/1163) (PR [#1186](https://github.com/penelopeduncan/redshipblueship/pull/1186)), [#1131](https://github.com/penelopeduncan/redshipblueship/issues/1131) (PR [#1188](https://github.com/penelopeduncan/redshipblueship/pull/1188)), [#1168](https://github.com/penelopeduncan/redshipblueship/issues/1168) and [#1182](https://github.com/penelopeduncan/redshipblueship/issues/1182) (PR [#1178](https://github.com/penelopeduncan/redshipblueship/pull/1178)), [#1160](https://github.com/penelopeduncan/redshipblueship/issues/1160) (PR [#1184](https://github.com/penelopeduncan/redshipblueship/pull/1184)), [#1164](https://github.com/penelopeduncan/redshipblueship/issues/1164) (not a defect: under Single Bag, Bombchu Drops need only the Bombchu Bag, as Ship of Harkinian's own drop rule says; the row `BombchuDropRule` of PR [#1184](https://github.com/penelopeduncan/redshipblueship/pull/1184) pins it), [#607](https://github.com/penelopeduncan/redshipblueship/issues/607) (resolved by PR [#761](https://github.com/penelopeduncan/redshipblueship/pull/761); not yet looked at in play) and [#993](https://github.com/penelopeduncan/redshipblueship/issues/993) (option 1 built by PR [#1147](https://github.com/penelopeduncan/redshipblueship/pull/1147); the live refresh is [#1146](https://github.com/penelopeduncan/redshipblueship/issues/1146)). The list at `af4d941f` was refreshed 2026-10-23. Closed between the `4fc4fd41` list and that one: [#1129](https://github.com/penelopeduncan/redshipblueship/issues/1129) (PR [#1147](https://github.com/penelopeduncan/redshipblueship/pull/1147)), [#1140](https://github.com/penelopeduncan/redshipblueship/issues/1140) (PR [#1149](https://github.com/penelopeduncan/redshipblueship/pull/1149)) and [#1152](https://github.com/penelopeduncan/redshipblueship/issues/1152) (PR [#1153](https://github.com/penelopeduncan/redshipblueship/pull/1153)). The list at `4fc4fd41` was refreshed 2026-10-23. Closed between the `30b841c1` list and that one: [#964](https://github.com/penelopeduncan/redshipblueship/issues/964) (PR [#1113](https://github.com/penelopeduncan/redshipblueship/pull/1113)), [#996](https://github.com/penelopeduncan/redshipblueship/issues/996) (PR [#1114](https://github.com/penelopeduncan/redshipblueship/pull/1114)), [#1085](https://github.com/penelopeduncan/redshipblueship/issues/1085) (PR [#1111](https://github.com/penelopeduncan/redshipblueship/pull/1111)), [#599](https://github.com/penelopeduncan/redshipblueship/issues/599) (PR [#1117](https://github.com/penelopeduncan/redshipblueship/pull/1117)), [#1037](https://github.com/penelopeduncan/redshipblueship/issues/1037) (PR [#1119](https://github.com/penelopeduncan/redshipblueship/pull/1119)) and [#1044](https://github.com/penelopeduncan/redshipblueship/issues/1044) (PR [#1116](https://github.com/penelopeduncan/redshipblueship/pull/1116)); filed and closed after the tag: [#1120](https://github.com/penelopeduncan/redshipblueship/issues/1120) (PR [#1122](https://github.com/penelopeduncan/redshipblueship/pull/1122)), [#1121](https://github.com/penelopeduncan/redshipblueship/issues/1121) (PR [#1125](https://github.com/penelopeduncan/redshipblueship/pull/1125)), [#1123](https://github.com/penelopeduncan/redshipblueship/issues/1123) (PR [#1124](https://github.com/penelopeduncan/redshipblueship/pull/1124)), [#1126](https://github.com/penelopeduncan/redshipblueship/issues/1126) (PR [#1138](https://github.com/penelopeduncan/redshipblueship/pull/1138)), [#1128](https://github.com/penelopeduncan/redshipblueship/issues/1128) (PR [#1137](https://github.com/penelopeduncan/redshipblueship/pull/1137)), [#1135](https://github.com/penelopeduncan/redshipblueship/issues/1135) (PR [#1136](https://github.com/penelopeduncan/redshipblueship/pull/1136)) and [#1139](https://github.com/penelopeduncan/redshipblueship/issues/1139) (PR [#1144](https://github.com/penelopeduncan/redshipblueship/pull/1144)). The list at `30b841c1`, the code of `v0.3.0-prealpha`, was refreshed 2026-10-22. Closed between the `db8d38e8` list and that one: [#1069](https://github.com/penelopeduncan/redshipblueship/issues/1069) (PR [#1102](https://github.com/penelopeduncan/redshipblueship/pull/1102)), [#1072](https://github.com/penelopeduncan/redshipblueship/issues/1072) (PR [#1075](https://github.com/penelopeduncan/redshipblueship/pull/1075); see [#1070](https://github.com/penelopeduncan/redshipblueship/issues/1070) below) and [#1101](https://github.com/penelopeduncan/redshipblueship/issues/1101) (PRs [#1104](https://github.com/penelopeduncan/redshipblueship/pull/1104) to [#1108](https://github.com/penelopeduncan/redshipblueship/pull/1108)). Closed between the `7671196a` and `db8d38e8` lists: [#855](https://github.com/penelopeduncan/redshipblueship/issues/855) (PR [#1074](https://github.com/penelopeduncan/redshipblueship/pull/1074)), [#1055](https://github.com/penelopeduncan/redshipblueship/issues/1055) (PR [#1076](https://github.com/penelopeduncan/redshipblueship/pull/1076)), [#1059](https://github.com/penelopeduncan/redshipblueship/issues/1059) (PR [#1073](https://github.com/penelopeduncan/redshipblueship/pull/1073)), [#1061](https://github.com/penelopeduncan/redshipblueship/issues/1061) (PR [#1077](https://github.com/penelopeduncan/redshipblueship/pull/1077)), [#1065](https://github.com/penelopeduncan/redshipblueship/issues/1065) (PR [#1081](https://github.com/penelopeduncan/redshipblueship/pull/1081)), [#1066](https://github.com/penelopeduncan/redshipblueship/issues/1066) (PR [#1083](https://github.com/penelopeduncan/redshipblueship/pull/1083)), [#1068](https://github.com/penelopeduncan/redshipblueship/issues/1068) (PR [#1080](https://github.com/penelopeduncan/redshipblueship/pull/1080)), [#1071](https://github.com/penelopeduncan/redshipblueship/issues/1071) (PR [#1078](https://github.com/penelopeduncan/redshipblueship/pull/1078)), [#1005](https://github.com/penelopeduncan/redshipblueship/issues/1005) to [#1009](https://github.com/penelopeduncan/redshipblueship/issues/1009) (PR [#1079](https://github.com/penelopeduncan/redshipblueship/pull/1079)), [#1018](https://github.com/penelopeduncan/redshipblueship/issues/1018) to [#1022](https://github.com/penelopeduncan/redshipblueship/issues/1022) (PR [#1082](https://github.com/penelopeduncan/redshipblueship/pull/1082)), [#965](https://github.com/penelopeduncan/redshipblueship/issues/965) (PR #1091), [#967](https://github.com/penelopeduncan/redshipblueship/issues/967) (PR #1096), [#968](https://github.com/penelopeduncan/redshipblueship/issues/968) (PR #1088), [#969](https://github.com/penelopeduncan/redshipblueship/issues/969) (PR #1095), [#970](https://github.com/penelopeduncan/redshipblueship/issues/970) (PR #1094), [#971](https://github.com/penelopeduncan/redshipblueship/issues/971) (PR #1097), [#994](https://github.com/penelopeduncan/redshipblueship/issues/994) (PR #1098), [#997](https://github.com/penelopeduncan/redshipblueship/issues/997) (PR #1090), [#1010](https://github.com/penelopeduncan/redshipblueship/issues/1010) to [#1017](https://github.com/penelopeduncan/redshipblueship/issues/1017) (PR #1086), [#1023](https://github.com/penelopeduncan/redshipblueship/issues/1023) (PR #1089), [#1024](https://github.com/penelopeduncan/redshipblueship/issues/1024) to [#1026](https://github.com/penelopeduncan/redshipblueship/issues/1026) (PR #1087), [#1030](https://github.com/penelopeduncan/redshipblueship/issues/1030) to [#1034](https://github.com/penelopeduncan/redshipblueship/issues/1034) (PR #1093), [#1035](https://github.com/penelopeduncan/redshipblueship/issues/1035), [#1036](https://github.com/penelopeduncan/redshipblueship/issues/1036), [#1038](https://github.com/penelopeduncan/redshipblueship/issues/1038), [#1062](https://github.com/penelopeduncan/redshipblueship/issues/1062) and [#1064](https://github.com/penelopeduncan/redshipblueship/issues/1064) (PR #1099), [#1040](https://github.com/penelopeduncan/redshipblueship/issues/1040) (PR #1092), [#1060](https://github.com/penelopeduncan/redshipblueship/issues/1060) (PR #1100) and [#1067](https://github.com/penelopeduncan/redshipblueship/issues/1067) (PR #1084).)

- No issue: on Ocarina of Time's shop shelves and in its get-item cutscene, the items 2Ship2Harkinian draws with its own code (owl statues, boss and enemy souls, frogs, the swim ability, Triforce pieces, traps and ocarina buttons) still show the "?" stand-in instead of their model. Buying or getting one works. The stray fairies and the bottled fairy draw since PR [#1113](https://github.com/penelopeduncan/redshipblueship/pull/1113) ([#964](https://github.com/penelopeduncan/redshipblueship/issues/964), closed), as still figures with no wing flap or colour pulse ([#1165](https://github.com/penelopeduncan/redshipblueship/issues/1165) asks for the animation); the four boss remains, the Moon's Tear and the caught seahorse since PR [#1042](https://github.com/penelopeduncan/redshipblueship/pull/1042); Majora's Mask's songs, as notes in each song's colour, since PR [#1185](https://github.com/penelopeduncan/redshipblueship/pull/1185) ([#1167](https://github.com/penelopeduncan/redshipblueship/issues/1167), closed); Majora's Mask's clock items, as the Clock Tower clock, since PR [#1295](https://github.com/penelopeduncan/redshipblueship/pull/1295) ([#1281](https://github.com/penelopeduncan/redshipblueship/issues/1281), closed).
- No issue ([#966](https://github.com/penelopeduncan/redshipblueship/issues/966) closed 2026-11-02, superseded by [#1233](https://github.com/penelopeduncan/redshipblueship/issues/1233)): the settings on the shared pages that Majora's Mask does not honour. Every row #966 filed is fixed, [#1037](https://github.com/penelopeduncan/redshipblueship/issues/1037) (Damage Multiplier) last, by PR [#1119](https://github.com/penelopeduncan/redshipblueship/pull/1119). Skip Boss Introductions reaches Majora's Mask since PR [#1124](https://github.com/penelopeduncan/redshipblueship/pull/1124); Skip To File Select has no Majora's Mask effect (the title screen is Ocarina of Time's) and no issue of its own; the settings that stay Ocarina of Time only by design are under "Majora's Mask specific"; the class lock #966 asked for is carried in [#1272](https://github.com/penelopeduncan/redshipblueship/issues/1272).
- [#975](https://github.com/penelopeduncan/redshipblueship/issues/975) (epic) and its open phase [#979](https://github.com/penelopeduncan/redshipblueship/issues/979): cross-game masks as in OoTMM. In this build, all off by default: the five shared masks (P2, [#978](https://github.com/penelopeduncan/redshipblueship/issues/978), closed), the Blast Mask (PR [#1391](https://github.com/penelopeduncan/redshipblueship/pull/1391)) and the Stone Mask (PR [#1446](https://github.com/penelopeduncan/redshipblueship/pull/1446)) in Ocarina of Time, and the Gerudo, Skull and Spooky Masks in Majora's Mask (P5, [#980](https://github.com/penelopeduncan/redshipblueship/issues/980), closed, PR [#1450](https://github.com/penelopeduncan/redshipblueship/pull/1450); what it left is [#1452](https://github.com/penelopeduncan/redshipblueship/issues/1452)). Not built: Kamaro's Mask in Ocarina of Time (P4, [#979](https://github.com/penelopeduncan/redshipblueship/issues/979)).
- No issue ([#993](https://github.com/penelopeduncan/redshipblueship/issues/993) closed with its option 1, PR [#1147](https://github.com/penelopeduncan/redshipblueship/pull/1147)): Majora's Mask's own Check Tracker window still has no menu row (console: `set gWindows.CheckTracker 1`); the Combo Tracker shows both games' checks and reachability.
- No issue ([#1170](https://github.com/penelopeduncan/redshipblueship/issues/1170) closed, by design): Link's House Cow shows available once everything needed to win Malon's race is held, as SoH's own Check Tracker shows it: the logic assumes the race can be won (PR [#1178](https://github.com/penelopeduncan/redshipblueship/pull/1178) checked it against upstream).
- [#1157](https://github.com/penelopeduncan/redshipblueship/issues/1157) (after the alpha): Shuffle Jabber Nuts applies to Ocarina of Time only; Majora's Mask has no talking gate of its own.
- [#1165](https://github.com/penelopeduncan/redshipblueship/issues/1165) (minor): stray fairies on Ocarina of Time's shelves and in its get-item cutscene are still figures (see the first line of this list).
- [#1169](https://github.com/penelopeduncan/redshipblueship/issues/1169) (in progress): cross-game warp songs, as in OoTMM. In this build fourteen are offered, off by default, under Randomizer > Cross-Game > Cross-Game Songs: Song of Soaring (OoT), played in Hyrule as a child, and Minuet of Forest (MM) to Prelude of Light (MM), played in Termina, which take a child Link to the song's place in Hyrule (its warp pad, or where Shuffle Warp Songs sent it when the file was created); both games' logic count them. A warp song played in Termina is refused ("You can't use that song here") while Ocarina of Time's half is an adult, in a scene that forbids the Song of Soaring, or for a shuffled grotto destination before Ocarina of Time's tables are ready. The integration row IntMMWarpSongMinuet plays the Minuet in Majora's Mask and arrives in Hyrule; the other five songs and a shuffled destination are covered by ROM-free rows only. Since lane XW14 the seven shared variants (Shared Song of Soaring to Shared Prelude of Light) are offered too, off by default, each greyed until its song is added: under one the song is one item of both games, placed once, and named without "(OoT)" / "(MM)" in item messages and the trackers. Under Shuffle Songs: Off a shared warp song stays at its usual check (Sheik's songs with Sheik, the Song of Soaring in the Southern Swamp), and both games' logic count it once that check is reached; a started shared Minuet of Forest to Prelude of Light under Off is still credited to Majora's Mask only at that check ([#1552](https://github.com/penelopeduncan/redshipblueship/issues/1552)). The integration row IntMMWarpSongMinuetShared learns the Minuet in Ocarina of Time only and plays it in Majora's Mask; nobody has played a shared song by hand. Since lane XW11 the added songs have rows in the Combo Item Tracker and both check trackers count their warp travel; neither pause screen has a row for an added song. Three known gaps under a shared song's rule: Majora's Mask's logic does not credit a shared Minuet of Forest to Prelude of Light that the file starts with, and Ocarina of Time's logic does not credit a shared Song of Soaring that Majora's Mask's file starts with (under Shuffle Songs: Off only once the Southern Swamp check is reached), so a seed can be more conservative than it needs to be ([#1552](https://github.com/penelopeduncan/redshipblueship/issues/1552)); and the shared Gerudo, Skull and Spooky Masks still show "(MM)" after their names; and Ocarina of Time's hints, both spoiler logs and Majora's Mask's item tracker layout editor still name a shared warp song with its game's tag, for example "the Song of Soaring (OoT)" ([#1560](https://github.com/penelopeduncan/redshipblueship/issues/1560)); since fixed, they are named without the tag everywhere except the spoiler logs (issues #1560, #1688, #1698). Under Shuffle Songs: Song Locations the Song of Soaring (OoT) fits with Crossing Direction Both; with Crossing Direction set to Off or MM Items to OoT, the Song of Soaring (OoT) alone already needs an Ocarina of Time song shared or started, under Song Locations too, and under Dungeon Rewards it always does. Otherwise Generate refuses ("Not generated: too many songs for the song checks." or "... for the song spots.").
- No issue ([#1172](https://github.com/penelopeduncan/redshipblueship/issues/1172) closed, PR [#1213](https://github.com/penelopeduncan/redshipblueship/pull/1213)): Termina's dungeon key models are in `2ship.o2r`; the GitHub Actions builds carry them, and a locally staged tree needs the `Generate2ShipOtr` target run again, until when the keys keep their tinted vanilla model. A Majora's Mask key drawn in Hyrule keeps its current look. Nobody has seen the mask and fairy chests or the key models on screen.
- No issue ([#1173](https://github.com/penelopeduncan/redshipblueship/issues/1173) closed, PR [#1212](https://github.com/penelopeduncan/redshipblueship/pull/1212)): a customised Non-Hookshotable Reticle colour (Cosmetics Editor) applies to Hyrule's Bow reticle but not to Termina's, which stays red; a follow-up. Nobody has seen the Termina hookshot reticle on screen.
- No issue ([#1187](https://github.com/penelopeduncan/redshipblueship/issues/1187) closed): a file created before PR [#1181](https://github.com/penelopeduncan/redshipblueship/pull/1181) keeps its "No Hint" gossip stones (hints are saved with the file): create a new file.
- No issue ([#1189](https://github.com/penelopeduncan/redshipblueship/issues/1189) closed, PR [#1382](https://github.com/penelopeduncan/redshipblueship/pull/1382)): both games' lists in the Check Tracker share the window's one scroll, so when Auto Scroll To Current Area scrolls the list of the game you are playing, the other game's list moves with it; while both lists fit, the window grows to fit them and nothing scrolls. Nobody has checked it on screen. The rest of #1189 is fixed (release notes).
- [#1231](https://github.com/penelopeduncan/redshipblueship/issues/1231) (developer tool): there is no in-app way to edit a Termina save, so a Majora's Mask report needs a seed playthrough to reproduce; the issue asks for 2Ship2Harkinian's Save Editor as a Termina-only window. Ship of Harkinian's Save Editor draws nothing while Termina runs and must never be used on Majora's Mask.
- [#1238](https://github.com/penelopeduncan/redshipblueship/issues/1238): Majora's Mask's developer tools that still have no row in the shared menu, its viewer windows above all; PR [#1260](https://github.com/penelopeduncan/redshipblueship/pull/1260) brought frame advance that follows the running game and Majora's Mask's map select, warp point and actor switches. The rest of the 2026-10-27 audit ([#1233](https://github.com/penelopeduncan/redshipblueship/issues/1233), closed 2026-11-02 with its sections [#1235](https://github.com/penelopeduncan/redshipblueship/issues/1235) to [#1237](https://github.com/penelopeduncan/redshipblueship/issues/1237)) landed on 2026-10-27 in twenty pull requests ([#1242](https://github.com/penelopeduncan/redshipblueship/pull/1242) to [#1271](https://github.com/penelopeduncan/redshipblueship/pull/1271); release notes, "Majora's Mask settings in the shared menu"): each row sits on the Ship of Harkinian page that holds its twin, and Majora's Mask's three camera Y axes, which started inverted with no row, can now be turned off (Settings → Controls → Popout Bindings Window → Camera Controls). What the reviews left over is [#1272](https://github.com/penelopeduncan/redshipblueship/issues/1272). Nobody has played the rows yet (playtest guide, section W).
- [#1272](https://github.com/penelopeduncan/redshipblueship/issues/1272): the Majora's Mask settings programme's leftovers. Three remain. Two are the operator's: the 2Ship2Harkinian import runs once per install (an install that already booted gets the wider import of PR [#1270](https://github.com/penelopeduncan/redshipblueship/pull/1270), and the colours below, only after removing `"TwoShipImport": {"Completed": true}` from `shipofharkinian.json`), and 2Ship2Harkinian's About panel has no home. The third is a test lock folded in from [#966](https://github.com/penelopeduncan/redshipblueship/issues/966): every Ship of Harkinian row either has its Majora's Mask counterpart or is listed as Ocarina of Time-only with a reason, and every shared or converged key is written by a Ship of Harkinian row. The rest landed together in PR [#1388](https://github.com/penelopeduncan/redshipblueship/pull/1388): Termina's beating heart follows the Hearts and DD Hearts colours until its own row is set, a changed Arrow Effects Secondary colour tints the arrow hit flash in Termina as in Hyrule, the import carries 2Ship2Harkinian's changed colours (onto Ship of Harkinian's row where one colours both games), one Disable Auto-Centering in First-Person View row drives both games, Faster Song Playback sits right after Fast Ocarina Playback, Network → Anchor is greyed until [#574](https://github.com/penelopeduncan/redshipblueship/issues/574), and presets save and apply the Majora's Mask rows' modes, fixes, colours and HUD placement (Majora's Mask's In-Game Timer stays out of presets, as Ship of Harkinian's own timer display does). Also from the programme, by design or inherited: a rainbow colour in the Cosmetics Editor holds still in Majora's Mask, and Randomize All, Reset All and Lock All skip the Majora's Mask rows (PR [#1259](https://github.com/penelopeduncan/redshipblueship/pull/1259)); a recoloured Majora's Mask sword or form trail draws its inner edge opaque (PR [#1271](https://github.com/penelopeduncan/redshipblueship/pull/1271)); finishing, skipping or removing the last Majora's Mask time split marks the file complete, as in 2Ship2Harkinian, which stops Majora's Mask's playtime and In-Game Timer for that file (PR [#1258](https://github.com/penelopeduncan/redshipblueship/pull/1258)); Ammo Buyback sells the shared ammo, so a sale in Termina lowers Hyrule's count (PR [#1256](https://github.com/penelopeduncan/redshipblueship/pull/1256)); Time Moves When You Move may save a stopped clock when you save standing still (suspected, inherited, PR [#1256](https://github.com/penelopeduncan/redshipblueship/pull/1256)); Authentic Logo, Fix Console Crashes, 3rd Save File Slot, Persistent Owl Saves and Disable Save Delay have no row, since none can act in paired play ([#1247](https://github.com/penelopeduncan/redshipblueship/pull/1247), [#1252](https://github.com/penelopeduncan/redshipblueship/pull/1252), [#1254](https://github.com/penelopeduncan/redshipblueship/pull/1254)).
- No issue (PR [#1230](https://github.com/penelopeduncan/redshipblueship/pull/1230)): under Shuffle Time the time-left text still counts every owned half-day from the start of the cycle, not only those left (cosmetic, not [#1229](https://github.com/penelopeduncan/redshipblueship/issues/1229)).
- No issue: the Combo Tracker's Saved Hide never hides a Majora's Mask check. Since PR [#1137](https://github.com/penelopeduncan/redshipblueship/pull/1137) the Combo Tracker follows Check Tracker Settings' Hide options, but it gives every obtained Majora's Mask check the Collected state, never Saved, so Saved Hide alone leaves those checks showing, and Collected Hide hides them even when saved. Ocarina of Time's rows follow both options as its own Check Tracker does.
- [#1127](https://github.com/penelopeduncan/redshipblueship/issues/1127) (deferred): the Combo Tracker's Majora's Mask pane groups enemy drops and unconnected grottos differently from Majora's Mask's own Check Tracker. Since done for enemy drops and for the checks inside shops, houses and other rooms (PRs #1705 and #1707).
- [#1146](https://github.com/penelopeduncan/redshipblueship/issues/1146) (filed by the #1129 lane): the Combo Tracker's pane for the game you are not playing keeps the reachable set captured when you left that game and does not refresh as you collect things in the other game. Its "Available" counts also count every listed check, where SoH's leave out the checks its Hide options hide, so with Hide options on they can read higher than SoH's Check Tracker.
- No issue (PR [#1149](https://github.com/penelopeduncan/redshipblueship/pull/1149)): at 1280x800 the Item Tracker overlay draws at about 72% of SoH's size, because Majora's Mask's section is 18 table lines long. The owl faces fit their cells since PR [#1202](https://github.com/penelopeduncan/redshipblueship/pull/1202), and Majora's Mask's small keys follow SoH's Key Count Tracking since PR [#1249](https://github.com/penelopeduncan/redshipblueship/pull/1249). Not measured again since the 3-column layout of PR [#1328](https://github.com/penelopeduncan/redshipblueship/pull/1328).
- No issue (PR [#1192](https://github.com/penelopeduncan/redshipblueship/pull/1192)): the Check Tracker and Item Tracker replace SoH's windows (open them from Randomizer → Check Tracker and Randomizer → Item Tracker). Window positions saved for the old Combo Tracker and Combo Item Tracker are not carried (the windows take SoH's own windows' saved positions, if any), nor whether they were open: a tracker opened through the former Combo → Windows starts closed after updating. An SoH preset's Check Tracker position is applied to SoH's retired window, not the unified one. Some of SoH's Check Tracker settings govern nothing yet (Vanilla/MQ Dungeon Spoilers, for example), and the menu search for one of SoH's tracker settings lands on SoH's own page. Since PR [#1202](https://github.com/penelopeduncan/redshipblueship/pull/1202) SoH's Item Tracker Settings govern the Item Tracker; its notes, separate section windows, Circle and Horizontal display and Total Checks are not honoured.
- No issue (PR [#1263](https://github.com/penelopeduncan/redshipblueship/pull/1263)): in the Item Tracker, with both on, Greg and the Fishing Pole each start a line of their own in the Ocarina of Time column, where SoH's main window puts them on one line, and a section set to SoH's Misc window shows in the overlay's one window. Majora's Mask's Dungeon and Boss Souls groups follow its own Majora's Mask Groups rows, not SoH's section settings. Nobody has seen the new Ocarina of Time groups on screen. Not checked again since the 3-column layout of PR [#1328](https://github.com/penelopeduncan/redshipblueship/pull/1328).
- ~~No issue (PR [#1250](https://github.com/penelopeduncan/redshipblueship/pull/1250), PR [#1264](https://github.com/penelopeduncan/redshipblueship/pull/1264)): an Ocarina of Time pickup held for a full cross-game outbox at a quit or a file load, before a crossing, keeps its check collected and is not given; with 2048 slots this needs 2048 un-redeemed pickups in Hyrule.~~ Fixed by PR [#1639](https://github.com/penelopeduncan/redshipblueship/pull/1639) ([#1635](https://github.com/penelopeduncan/redshipblueship/issues/1635)): such a pickup is now kept in the save across a quit and is given once the outbox has room; nobody has played it yet. An Ocarina of Time shop still takes your rupees for an item that has to be held this way. Nobody has carried more than 64 pickups across in play (playtest guide, W14).
- [#1642](https://github.com/penelopeduncan/redshipblueship/issues/1642) (filed after this list was refreshed): when you quit to the file select right after collecting a check, the cross-game save can be written while Ocarina of Time is still saving that check, so the two could disagree about it. Nobody has seen this happen; it was found by reading the code.
- No issue ([#1434](https://github.com/penelopeduncan/redshipblueship/issues/1434) closed, PR [#1446](https://github.com/penelopeduncan/redshipblueship/pull/1446), merged after `4f37bf18`): under [Both Games] Logic No Logic, with Shuffle Master Sword and Time Travel Requires Master Sword on (both the default), nothing keeps the Master Sword off a check only an adult can reach, so a child start can be left unable to travel through time. Since [#1484](https://github.com/penelopeduncan/redshipblueship/issues/1484) the pedestal's own check is not one of them: with the sword shuffled it is a child check, handed over as the child walks up, as in OoTMM, whose no-logic has the same exposure for its other adult checks. Turn Time Travel Requires Master Sword off for a No Logic seed.
- [#1363](https://github.com/penelopeduncan/redshipblueship/issues/1363), [#1364](https://github.com/penelopeduncan/redshipblueship/issues/1364), [#1403](https://github.com/penelopeduncan/redshipblueship/issues/1403), [#1405](https://github.com/penelopeduncan/redshipblueship/issues/1405) to [#1410](https://github.com/penelopeduncan/redshipblueship/issues/1410) and [#1414](https://github.com/penelopeduncan/redshipblueship/issues/1414): upstream feature packets queued after `v0.5.0-alpha` (Shuffle Silver Rupees, the Treasure Chest Game shuffle, Majora's Mask pool options, Wonder Items, butterfly fairies, icicles and red ice, rocks and boulders, beehives and extra grass, and Ocarina of Time quality-of-life rows). ~~Nothing in this build.~~ All of them are in `v0.5.1-alpha` (PRs [#1510](https://github.com/penelopeduncan/redshipblueship/pull/1510) to [#1515](https://github.com/penelopeduncan/redshipblueship/pull/1515), [#1556](https://github.com/penelopeduncan/redshipblueship/pull/1556), [#1558](https://github.com/penelopeduncan/redshipblueship/pull/1558), [#1562](https://github.com/penelopeduncan/redshipblueship/pull/1562) and [#1564](https://github.com/penelopeduncan/redshipblueship/pull/1564); release notes). The new shuffles are options, off by default, except seven Majora's Mask options, which are in the menu but greyed out in paired worlds. Since [#1530](https://github.com/penelopeduncan/redshipblueship/issues/1530) (PR [#1682](https://github.com/penelopeduncan/redshipblueship/pull/1682)) Shuffle Silver Rupees and Shuffle Chest Minigame work in paired worlds: their silver rupees and the chest game's keys or keyring are placed only on Ocarina of Time checks, and the room chests, the chest game's shopkeeper and the silver rupee checks hold only Ocarina of Time items. Nobody has played either one in a paired world yet. The silver rupee rooms' newer logic applies with Shuffle Silver Rupees off, so the same seed and settings can make a different world.
- [#1404](https://github.com/penelopeduncan/redshipblueship/issues/1404) (PR [#1555](https://github.com/penelopeduncan/redshipblueship/pull/1555)): nobody has played the new Starting Items page or its new options by hand yet. Three things have not been checked in a paired world: starting with an item the two games can share (a wallet, bomb bag, quiver, magic, bottles, a shield, the Lens of Truth, a magic arrow or a mask) against Majora's Mask's starting inventory; Progressive Goron Sword when its sword is found in Termina or sent there; and Shuffle Weird Egg's Skip Waking Talon. The retired Skip Child Zelda and Kakariko Gate settings are still applied when something restores them, such as a spoiler or seed package.
- [#1398](https://github.com/penelopeduncan/redshipblueship/issues/1398) (raised 2026-11-09): Majora's Mask logic fixes from 2Ship2Harkinian (the Blast Mask's explosive rule, water enemies under Shuffle Swim, the Peahat by night, the postbox, the fisherman's game and others). ~~Not in this build.~~ In `v0.5.1-alpha` (PR [#1516](https://github.com/penelopeduncan/redshipblueship/pull/1516)); the Peahat's entry under "Majora's Mask specific" is resolved.
- [#1393](https://github.com/penelopeduncan/redshipblueship/issues/1393) and [#1394](https://github.com/penelopeduncan/redshipblueship/issues/1394) (after launch): Majora's Mask's transformation masks in Ocarina of Time, and Roc's Feather in Majora's Mask. Scoped only.
- [#1417](https://github.com/penelopeduncan/redshipblueship/issues/1417) to [#1419](https://github.com/penelopeduncan/redshipblueship/issues/1419) (parked): 2Ship2Harkinian mechanics and Ship of Harkinian quality-of-life rows and shuffles that the backport review set aside. Nothing scheduled.
- [#1452](https://github.com/penelopeduncan/redshipblueship/issues/1452): what the Gerudo, Skull and Spooky Masks in Majora's Mask still lack: their display in a borrowed spot ([#1500](https://github.com/penelopeduncan/redshipblueship/issues/1500)), Item Tracker rows, their model when Ocarina of Time draws one that crossed (a stand-in), being taken off at some respawns, and some of the logic credit OoTMM gives them (the logic asks for more, never less).
- [#1453](https://github.com/penelopeduncan/redshipblueship/issues/1453): in Majora's Mask's logic a night-only gate also lets the day half of a region through. With the Spooky Mask (MM) rule on (off by default), a day-only check past a Gibdo could be counted as reachable at night. Not seen in play.
- [#1467](https://github.com/penelopeduncan/redshipblueship/issues/1467): a very large Majora's Mask custom music pack can run out of sound font slots (its music goes silent, no crash), and some of Majora's Mask's file writers are not yet crash-safe.
- [#1475](https://github.com/penelopeduncan/redshipblueship/issues/1475): one Hint Tracker for both games. ~~Today the Hint Tracker window lists Ocarina of Time's hints only; Majora's Mask's hints are recorded (PR [#1480](https://github.com/penelopeduncan/redshipblueship/pull/1480)) but not shown.~~ Resolved by PR [#1528](https://github.com/penelopeduncan/redshipblueship/pull/1528): the window has an Ocarina of Time pane and a Majora's Mask pane and works in either game.
- [#1479](https://github.com/penelopeduncan/redshipblueship/issues/1479) (cosmetic): the "Restored from file" toast shortens rule names to "..." in the default 640x480 window, and a long list is cut short.
- [#1481](https://github.com/penelopeduncan/redshipblueship/issues/1481): a Progressive Time found in Ocarina of Time is shown as "Progressive Time" with the Day 1 clock; OoTMM names the half-day it grants.
- [#1482](https://github.com/penelopeduncan/redshipblueship/issues/1482): a shuffled Ocarina of Time shop shelf sells its item once; OoTMM restocks renewable items on every shuffled shelf.
- [#1483](https://github.com/penelopeduncan/redshipblueship/issues/1483): a Triforce Hunt with more pieces than one game's checks can hold now fails with a toast that names the game, for example "Not generated: too many Majora's Mask pieces." (a game whose own items cannot fit its checks for another reason: "too few checks in Majora's Mask."). Under All Locations and Beatable Only it says so only after the attempt ladder or a round budget stops, as before; under No Logic at once. Lower the piece count.
- [#1484](https://github.com/penelopeduncan/redshipblueship/issues/1484): the Master Sword pedestal is an adult check here and a child check in OoTMM.
- [#1498](https://github.com/penelopeduncan/redshipblueship/issues/1498), [#1499](https://github.com/penelopeduncan/redshipblueship/issues/1499) and [#1500](https://github.com/penelopeduncan/redshipblueship/issues/1500) (found in play on `v0.5.0-alpha`): enemy drops shown as available where they cannot be got, extra Tingle map copies sold as junk under the map's name, and a borrowed mask shown as its spot's own mask. Entries under "Majora's Mask specific".
- [#1501](https://github.com/penelopeduncan/redshipblueship/issues/1501): `v0.5.0-alpha`'s follow-ups, the macOS dmg's missing extraction tools above all ("macOS is untested" under "Platform and packaging").
- [#1374](https://github.com/penelopeduncan/redshipblueship/issues/1374) and [#1447](https://github.com/penelopeduncan/redshipblueship/issues/1447) (developers only): the record of a CI commit no build checked, and the generator-version rule and fill follow-ups from the D wave 2 re-pin. No effect in play.

- [#694](https://github.com/penelopeduncan/redshipblueship/issues/694): MM's game-over prompt (Enhancements → Quality of Life, Game Over Prompt in the Majora's Mask block, off by default) has never been seen in this build; its artwork may be missing. Awaiting a playtest.
- No issue ([#607](https://github.com/penelopeduncan/redshipblueship/issues/607) closed): an OoT item found in Termina shows its OoT icon in MM's get-item textbox (PR [#761](https://github.com/penelopeduncan/redshipblueship/pull/761)); not yet looked at in play.
- [#573](https://github.com/penelopeduncan/redshipblueship/issues/573): no cross-game entrance randomization; the Happy Mask Shop door is always the crossing.
- [#574](https://github.com/penelopeduncan/redshipblueship/issues/574): no multiworld over the combo. Ship of Harkinian's Network → Anchor page is present in release builds but unsupported on a paired file until this issue: it knows nothing of the pair, and a session is unsound. Since PR [#1388](https://github.com/penelopeduncan/redshipblueship/pull/1388) ([#1272](https://github.com/penelopeduncan/redshipblueship/issues/1272)) the page draws greyed, with a gray note and the disabled reason "Paired Worlds Not Supported Yet"; a config that already enabled Anchor still connects at boot, and the page then stays usable so it can be turned off. The one-game audit's last row, world identity on netplay grants (V15 of [#564](https://github.com/penelopeduncan/redshipblueship/issues/564), closed), is carried here.
- [#460](https://github.com/penelopeduncan/redshipblueship/issues/460): a netplay grant for the game you are in would wait for your next crossing; the combo's netplay relay is not built by default (`RSBS_NETPLAY` is OFF), so a player does not meet this.

The community reports [#634](https://github.com/penelopeduncan/redshipblueship/issues/634),
[#635](https://github.com/penelopeduncan/redshipblueship/issues/635) and
[#636](https://github.com/penelopeduncan/redshipblueship/issues/636) were closed on 2026-10-23;
their fixes are in this build (see their entries below).

### Back up your saves. Seriously.

Save files are save format 6. This version opens files made by `v0.5.1-alpha`. Files made by `v0.5.0-alpha` or
earlier are refused with "Refused: File made by an older version" and are never converted. The load that refuses
one renames its cross-game save (`.redsave`) aside, with a name ending in `.refused-oldversion.bak`, instead of
writing over it. A file that this version has saved is refused by `v0.5.1-alpha` ("File made by another build"),
so copy `Save/` before you try this version if you may go back. A future version may break saves again, so keep
copies of files you care about.

**Loading a file puts its Majora's Mask settings back into the menu** (a "Restored for Majora's Mask" message lists
them). A file made before `v0.5.2-alpha` sets Moon Crash Behavior to Last Save (unless you chose New Cycle) and
Shuffle Ocarina to off, and the next file you create uses those values: check Randomizer > MM Randomizer first.

**A moon crash starts a new cycle by default** (Moon Crash Behavior: New Cycle; always with Shuffle Ocarina or
Shared Song of Time on). A file that chose Last Save, or was made before `v0.5.2-alpha` with the old default, goes
back to its last save instead, and what you did in Majora's Mask since then is lost.

**A file whose `.redsave` belongs to another file** (a copy put at this slot's path, for example) is refused with
"Refused: Cross-game record is for another file". The file is not damaged: put back the `.redsave` that belongs
to it.

**A seed string does not make the same world on a different version.** In `v0.5.2-alpha` the same seed and
settings can make a different world than `v0.5.1-alpha` made, with the default settings too. A file keeps the
world it was created with; nothing in it is generated again. Seed packages exported by `v0.5.1-alpha` or earlier
are refused with "Seed package from another generator": generate the seed again.

**A file made by a newer version can be refused by an older one** ("Refused: File made by another build"). The
older version then renames the file's cross-game save aside, to a name ending in `.refused-version.bak` or
`.refused-tiersize.bak`, and the newer version will then refuse the file too ("Cross-game record is missing").
To play the file again in the newer version, rename that file back to its old name first, or put back your
copy of `Save/`. The file is not damaged.

---

## Pre-playtest smoke, 2026-09-27, `d928d67d`

Before anyone plays, the build under test was run through the checks that hosted CI
cannot run, because they need the ROMs. Code at `d928d67d` is the code at `d6c8f270`:
the two commits after it changed documentation only.

**How it was run.** A fresh Windows checkout at `d928d67d`, built from cold (Release,
MSVC). `oot.o2r` and `mm.o2r` were extracted from the ROMs by that build's own
extractor, and `soh.o2r`, `2ship.o2r` and `redship.o2r` were generated at the same commit.
The config was OpenGL only, and windows opened without taking focus. There was no `Save/`
folder from earlier play.

| Check | Result |
|---|---|
| `redship` tier | 138 of 138 passed, none skipped |
| `rando` tier | 40 of 40 passed, none skipped. All four golden-world rows passed with `tests/golden/` untouched, so the generated worlds are the pinned ones |
| `ui` tier (OpenGL, 1280x800, ROM archives mounted) | passed: 105 captures, every one `pass`, none skipped |
| `integration` | 5 of 6 passed. `IntSwitchOoTHmsToMm` timed out at 120 s, which was [#544](https://github.com/penelopeduncan/redshipblueship/issues/544) (fixed since; see "CI and quality gates" below). `IntBootOoT`, `IntBootMM`, `IntSwitchMmClockTownSouthToOoT`, `IntArchiveHotswapCycle` (4 arrivals) and `IntGameplayRoundtrip` passed |
| `integration-soak` | `IntGameplayRoundtripSoak` passed: "3 round trip(s), warp, and door transition survived 120 live frames per phase" (70.6 s) |
| Creation time, shipped defaults | 10 real paired creations (`RSBS_CSB_SAMPLE_EVENT=10`: Generate, the creation event with the spoiler written, the arm, `Randomizer_InitSaveFile`). 10 of 10 created on the first attempt with no budget stop. Mean 8.5 s, best 6.2 s, worst 13.0 s (the one seed whose fill needed a second batch), against the 30 s budget. Every world's 50 heart pickups filled the bar with none wasted |
| DX11 menus against OpenGL | See below: no visible difference |

**What the smoke did not cover then, and a row covers now: the first crossing into a
paired Termina with the real game data loaded.** At `d928d67d` no automated row ran the
whole path in one process: create a paired file through the creation event, then walk
into MM with the ROM archives mounted. The integration rows crossed with a vanilla debug
save, and their log said so on every MM arrival:
`[MM] pairing: skipped-because-no-paired-oot-world`. The rows that did run creation and
then an MM arrival in one process (`ComboCreationEvent`, `MMPairSwitchEntry`) ran with the
OTR archive init disabled (`RSBS_DISABLE_OTR_INIT=1`), so no real game data was loaded.

Since PR [#790](https://github.com/penelopeduncan/redshipblueship/pull/790) the
`integration` label has a row that does it: `IntPairedFirstCrossing`
(`redship --integration-test int-paired-first-crossing`). In one process, with the ROM
archives mounted and a real OpenGL window, it takes the title to the file select (SoH's
"Boot Sequence: File Select" path), generates the pinned world `RSBSSINGLEBAG1` on the
shipped defaults (checked: it fails if the config sets any OoT, MM or combo setting),
creates file 3 through OoT's own new-file seam (`OoT_Sram_InitSave`,
which runs the production creation event and writes the slot), loads that file back the
way the file select does, and plays it from the Market (0x01D1). The loaded file must carry
the identity the creation recorded before the load (seed, settings digests, crossing counts
and digest). It walks through the
Happy Mask Shop (0x0530) into South Clock Town (0xD800), then back through the Clock
Tower door (0xC010). It fails unless the MM arrival logs both lines §3 of the playtest
guide asks you to look for, the pairing is live in MM, the crossing store is frozen and
not empty, the MM half is the one the creation armed, nothing generated at the arrival,
and no refusal was logged or toasted. The return leg must restore OoT's half, not
regenerate it. The first green run (workstation, Windows, OpenGL, archives extracted
at the branch tip, 55.2 s) logged:

```
[MM] pairing: arrival profile matches the creation-frozen identity (13DE4C35)
[MM] pairing: HYDRATED from the frozen MM half (saveType=rando mmFinalSeed=8DDF1DEE crossingsInHyrule=51 crossingsInTermina=21) — nothing was generated at this arrival
[PFC] return leg PASS: OoT's half restored, not regenerated (file 3, OoT world seed 2852956488, sentinel deaths=777 survived), same identity: ...
```

Hosted CI cannot run it, like every `integration` row: it needs the ROM-derived archives.
It runs locally (see [`ci-gameplay-repro-postmortem.md`](ci-gameplay-repro-postmortem.md) §7)
and in the manual `integration-tests` workflow on a ROM-equipped runner. With
`RSBS_PFC_SKIP_CREATION=1` the row boots the debug save instead, as the older rows do, and
fails on the `skipped-because-no-paired-oot-world` line: that is its red half. The playtest's
first crossing (guide §3, step 3) is still the first time a person walks this path, and it
still comes first. What the row cannot see is what a person sees: it fires the door
transitions directly, and the Happy Mask Shop's Closed Forest gate is not in its path.

**The HYDRATED line's count.** At `d928d67d` every HYDRATED line ended with
`foreignPlacements=0`, even when the file had dozens of crossings. That count was the
retired item table from before the single-bag switch. Since PR
[#790](https://github.com/penelopeduncan/redshipblueship/pull/790) the line prints the
crossing store's two counts instead: `crossingsInHyrule` (Majora's Mask items in Ocarina of
Time checks) and `crossingsInTermina` (Ocarina of Time items in Majora's Mask checks). The
Cross-Game Spoiler reads the same store (the Combo Tracker lists no crossings since PR [#1137](https://github.com/penelopeduncan/redshipblueship/pull/1137)).

**Smaller notes.**
- The test portal (`--test-entrance`, Mido's House to the Clock Tower) skips Closed
  Forest. On the shipped defaults, the Happy Mask Shop opens only after the Deku Tree.
- No `integration` or `integration-soak` row loaded a `.redsave` at `d928d67d`. Every MM
  arrival in them was vanilla (`skipped-because-no-paired-oot-world`). `IntPairedFirstCrossing`
  (PR [#790](https://github.com/penelopeduncan/redshipblueship/pull/790)) now writes and loads one. The `redship` tier's save rows do
  load `.redsave` files and refuse some of them on purpose: a verbose re-run of the tier
  printed 66 `[RsbsSave] slot N REFUSED` lines (20 rows, plus `AllTests` repeating them),
  and every one of those rows passed.

**DirectX 11.** The Windows CI run on `d6c8f270` (run 36369933984) renders the menu
pages, windows and toasts through DirectX 11 without the ROM archives. It lists 98
captures, and 90 of them were rendered. The other 8 are the original SoH pages, which are
skipped without `oot.o2r` (in both runs), so no original SoH page was compared under
DirectX 11. The 90 were compared with an OpenGL render made at `d928d67d` on the same
832x600 profile, also without the ROM archives.
- **Text:** the text drawn is identical in all 90 rendered captures. Each capture's size,
  content rectangle, scroll range and expected text match too.
- **Pixels:** no capture is pixel-identical. Between 0.11% and 1.55% of a capture's pixels
  differ, and all but 282 of those pixels (over all 90 captures) differ by exactly one
  level. Pixels differ by more than 32 levels in only 27 captures: 11 frame-corner pixels
  in each of the 22 MM Tricks captures, one table-line end pixel in each of the 3 MM Mods
  captures, and 2 table-corner pixels each on the Cross-Game Spoiler (crossings) and the
  Combo Tracker (progress, scroll 1). None of them is visible.
- **Pages checked by eye,** side by side and in a difference image: Cross-Game Rules
  (including the Goal tooltip), Majora's Mask, Windows, MM Mods, MM Randomizer (live and
  frozen), MM Tricks (live and frozen), the creation overlay, the Combo Tracker and the
  Cross-Game Spoiler. No difference is visible.
- ~~**Both backends, not a DX11 issue:** at 832x600, the Combo > Majora's Mask page cuts
  off its checkbox labels next to the Autosave note.~~ **Resolved.** SoH's own pages do the
  same at that size, and still do: rule 0 keeps them as shipped. Measured since then (the
  `ui` tier's runtime lint R9, which compares every row's rectangle with its column's clip
  rectangle, OpenGL, ROM archives mounted): SoH's pages overrun 68 rows at 832x600 (23 on
  Enhancements > Quality of Life, among them "Remember Save Location" and "Nighttime GS
  Always Spawn"), 36 at 960x704 and 2 at 1280x800. Of those, 5 at 832x600 and 3 at 960x704
  carry this project's ruled "[Both Games] " marker, which SoH shipped without (Dev Tools >
  General, Settings > General); the other 63 and 33 are SoH's rows as shipped. Ours overran 25 at 832x600 and 10 at
  960x704, not only on the Majora's Mask page: Cross-Game Rules (the pool-size sliders, the
  class headers, Shared Ocarina, all three rows since retired), MM Randomizer (two access comboboxes and seven long
  labels) and the harness's MM Row States probe. The Majora's Mask page now has two columns,
  SoH's count for a mixed page, and the long labels are shorter (the renames are listed in
  the lane's pull request), so no row of ours overruns at any of the three profiles. R9 fails
  the `ui` tier if one does again at the run's profile, and the `UiSnapshotMin` row pins that
  profile to 832x600 in CI (docs/ui-style-guide.md section 1, the width rule).

**Findings.** None apart from #544, so no new issue was filed.

---

## Smoke, 2026-10-03, `82d05d0d`

The closing run of the work merged between `200adaea` and `82d05d0d`: 49 pull requests,
[#802](https://github.com/penelopeduncan/redshipblueship/pull/802) to
[#876](https://github.com/penelopeduncan/redshipblueship/pull/876). Like the run above, it
covers the checks hosted CI cannot run, because they need the ROMs.

**How it was run.** The workstation's main Windows checkout. Its object directories were
cleared and the tree was rebuilt (Release, MSVC, through sccache) at the merge of the last
pull request's branch ([#872](https://github.com/penelopeduncan/redshipblueship/pull/872))
onto `c36ca898`. That merge has the same tree as `82d05d0d` (`d23e4b5c`), the squash commit
that landed right after the run. `soh.o2r`, `2ship.o2r` and the two curated archives
`redship-oot.o2r` and `redship-mm.o2r` were regenerated at that tree. `oot.o2r` and `mm.o2r`
were not re-extracted: they are the ROM extractions earlier builds made. The config was
OpenGL only, and windows opened without taking focus.

| Check | Result |
|---|---|
| `redship` tier | 163 of 163 passed, none skipped (138 in the run above) |
| `rando` tier | 42 of 42 passed, none skipped (40 in the run above). All four golden-world rows passed (`GoldenSeedDigestDefault`, `GoldenSeedDigestProfileV1`, `GoldenPairedAttemptDigest`, `GoldenSeedDigestArmedCaps`) with `tests/golden/` as it stands at this commit |
| `ui` tier (OpenGL, ROM archives mounted) | 2 of 2 rows passed. The parity lint passed plain (0 gated hits) and no-grow (neither baseline gained an entry) |
| `integration` | 13 of 13 passed (6 rows in the run above, one of them red): `IntBootOoT`, `IntBootMM`, `IntSwitchOoTHmsToMm`, `IntSwitchMmClockTownSouthToOoT`, `IntArchiveHotswapCycle`, `IntCuratedArchiveBootRegen`, `IntWatchdogWedgeOoTFrame`, `IntWatchdogWedgeHandoff`, `IntWatchdogWedgeMMFrame`, `IntGameplayRoundtrip`, `IntGameplayRoundtripCapture`, `IntPairedFirstCrossing`, `IntPairedFirstCrossingDiverged` |
| `integration-soak` | `IntGameplayRoundtripSoak` passed (71.2 s) |

**Generated worlds moved during this work.** Two pull requests each re-pinned three of the
four golden files (`seed-digest-default`, `seed-digest-profile-v1`, `paired-attempt-digest`):
[#840](https://github.com/penelopeduncan/redshipblueship/pull/840) (MM shops host OoT items) and
[#874](https://github.com/penelopeduncan/redshipblueship/pull/874) (OoT's treasure chest game
hosts MM items). A seed string and settings that made one world at `200adaea` make a different
one now. Nothing stored in an existing save is recomputed.

**What this run did not cover.**
- Nobody has played this build. The rows drive door transitions and item gives directly; they
  do not see what a person sees.
- DirectX 11 was not compared with OpenGL.
- The creation-time sample (10 real paired creations against the 30 s budget) was not re-run.
- `ForeignItemGiveReverse` passed here. It ended once without a verdict in an earlier gate run
  and has not done so again ([#824](https://github.com/penelopeduncan/redshipblueship/issues/824)).

**Findings.** None in this run. Known and open at this commit, each with its own issue: get-item
text does not say which game a crossed item belongs to
([#865](https://github.com/penelopeduncan/redshipblueship/issues/865)); a chest that hosts the
other game's item always looks like a major chest
([#862](https://github.com/penelopeduncan/redshipblueship/issues/862)); MM's spoiler omits the
price of an OoT item in an MM shop or Tingle slot
([#863](https://github.com/penelopeduncan/redshipblueship/issues/863)); on a vanilla paired file,
OoT's Check Tracker can show checks from before a crossing commit after a reload
([#861](https://github.com/penelopeduncan/redshipblueship/issues/861)); OoT's threaded save worker
can land an older generation over a crossing commit
([#851](https://github.com/penelopeduncan/redshipblueship/issues/851)). All five were fixed
after this run, by PRs [#884](https://github.com/penelopeduncan/redshipblueship/pull/884),
[#892](https://github.com/penelopeduncan/redshipblueship/pull/892),
[#893](https://github.com/penelopeduncan/redshipblueship/pull/893),
[#898](https://github.com/penelopeduncan/redshipblueship/pull/898) and
[#896](https://github.com/penelopeduncan/redshipblueship/pull/896).

---

## Smoke, 2026-10-12, `851b1ee9`

The closing run of the 2026-09-30 wave: 92 pull requests merged between `200adaea` and
`851b1ee9`, [#802](https://github.com/penelopeduncan/redshipblueship/pull/802) to
[#940](https://github.com/penelopeduncan/redshipblueship/pull/940), 43 of them after the run
above. Like the runs above, it covers the checks hosted CI cannot run, because they need the
ROMs.

**How it was run.** The workstation's main Windows checkout at `851b1ee9` (the log opens
with `=== merge origin/main onto origin/main flags=uia` and `Already up to date.`), rebuilt
in 2496 build steps (Release). `soh.o2r`, `2ship.o2r` and the two curated archives were
regenerated at 07:16 (`redship-oot.o2r`: "332 curated resource(s)", `redship-mm.o2r`: "190
curated resource(s)"). `oot.o2r` and `mm.o2r` were not re-extracted. The config was OpenGL
only, and windows opened without taking focus. The run ended at 07:30:06 on 2026-10-12.

| Check | Result |
|---|---|
| `redship` tier | 179 of 179 passed ("100% tests passed out of 179"; 163 in the run above). `AllTests` and `ForeignItemGiveReverse` passed |
| `rando` tier | 43 of 43 passed (42 in the run above). All four golden-world rows passed (`GoldenSeedDigestDefault`, `GoldenSeedDigestProfileV1`, `GoldenPairedAttemptDigest`, `GoldenSeedDigestArmedCaps`) with `tests/golden/` as it stands at this commit |
| `ui` tier (OpenGL, ROM archives mounted) | 2 of 2 rows passed (`UiSnapshot`, `UiSnapshotMin`). The parity lint passed plain ("OK: 0 gated hit(s), all in the baseline") and no-grow ("neither baseline gained an entry relative to origin/main") |
| `integration` | 14 of 14 passed (13 in the run above): `IntBootOoT`, `IntBootMM`, `IntSwitchOoTHmsToMm`, `IntSwitchMmClockTownSouthToOoT`, `IntArchiveHotswapCycle`, `IntCuratedArchiveBootRegen`, `IntWatchdogWedgeOoTFrame`, `IntWatchdogWedgeHandoff`, `IntWatchdogWedgeMMFrame`, `IntGameplayRoundtrip`, `IntGameplayRoundtripCapture`, `IntPairedFirstCrossing`, `IntPairedFirstCrossingDiverged`, `IntPairedFirstCrossingTwoCycles` |
| `integration-soak` | `IntGameplayRoundtripSoak` passed (70.6 s) |

**Generated worlds moved during this work.** Three pull requests re-pinned golden files:
[#840](https://github.com/penelopeduncan/redshipblueship/pull/840) and
[#874](https://github.com/penelopeduncan/redshipblueship/pull/874) (named in the run above)
each re-pinned three of the four (`seed-digest-default`, `seed-digest-profile-v1`,
`paired-attempt-digest`), and [#895](https://github.com/penelopeduncan/redshipblueship/pull/895)
(MM's paired start) re-pinned all four, `seed-digest-armed-caps` included. A seed string and
settings that made one world at `200adaea` make a different one now, and paired files created
before #895 are refused (see "Back up your saves").

**What this run did not cover.**
- Nobody has played this build. The rows drive door transitions and item gives directly; they
  do not see what a person sees.
- DirectX 11 was not compared with OpenGL.
- The creation-time sample (10 real paired creations against the 30 s budget) was not re-run.
- Not tested by anyone, by a row or by hand: networking, and a
  crossing made after quitting and relaunching the game, or after a save made during play.
  (This line once also named "the cross-game warp songs"; at this commit none were built. They are
  since lane XW12, see [#1169](https://github.com/penelopeduncan/redshipblueship/issues/1169).)
  (A crossing after the file is created, saved and loaded back from disk in the same session
  is tested: `IntPairedFirstCrossing` does exactly that.)

**Findings.** None in this run. The open issues at this commit were listed under "Open issues
at `851b1ee9`" near the top of this document, since refreshed at `7e44b8a0` and `7671196a`.

---

## Gates, 2026-10-17, `7e44b8a0`

The three determinism pull requests merged on top of the version bump (`ece56551`, PR
[#946](https://github.com/penelopeduncan/redshipblueship/pull/946)): [#958](https://github.com/penelopeduncan/redshipblueship/pull/958) (the goldens pin hint text, prices, starting items and
the entrance table, plus an entrance-shuffle golden; static hints built in a fixed order;
byte-unsigned seed hashing), [#959](https://github.com/penelopeduncan/redshipblueship/pull/959) (the creation budget counts fill rounds) and
[#960](https://github.com/penelopeduncan/redshipblueship/pull/960) (the paired world's fingerprint and the build beside the seed icons). Like
the runs above, it covers the checks hosted CI cannot run, because they need the ROMs.

**How it was run.** The workstation's main Windows checkout, OpenGL only, windows opened
without taking focus, one tier at a time. Each pull request was gated as a merge of its branch
onto the `main` of that moment, and each merged tree equals its gated tree: #958 at tree
`1fa8507d` (squash commit `865ec10b`), #959 at `fb229270` (`aff54bf7`), #960 at `e524fdfc`,
which is `main`'s tree at `7e44b8a0`.

| Check | #958 (`1fa8507d`) | #959 (`fb229270`) | #960 (`e524fdfc` = `7e44b8a0`) |
|---|---|---|---|
| `redship` tier | 183 of 183 | 183 | 184 of 184 (179 at `851b1ee9`) |
| `rando` tier | 45 of 45 | 48 | 49 of 49 (43 at `851b1ee9`), the golden-world rows among them (five golden files now, with `seed-digest-entrances`) |
| `ui` tier, parity lint | 2 of 2; lint plain and no-grow OK | 2; lint OK | 2 of 2; lint plain and no-grow OK |
| `integration` | 16 of 16 | 16 | 17 of 17 (14 at `851b1ee9`); `IntPairedFileSelectIdentity` is new |
| `integration-soak` | 1 of 1 | 1 | 1 of 1 |

**Creation-time sample, re-run at `7e44b8a0`.** `RSBS_CSB_SAMPLE_EVENT=10 redship --test
combo-single-bag`, host scale 100%: 10 of 10 paired creations succeeded on their first
attempt, each after 258 fill rounds against the 1250-round per-attempt cap. End to end the mean
was 7058 ms, the best 6182 ms and the worst 8550 ms, with no per-attempt budget stop and no
dead heart pickup. This answers the "creation-time sample was not re-run" line of the two runs
above.

**Generated worlds.** No pinned world moved. #958 added its new fields to three golden files and
a fifth golden, `seed-digest-entrances`; no existing golden line changed. A typed seed with
non-ASCII characters makes a different world than before #958 on Windows and x86 Linux (ASCII
and generated seeds are unchanged).

**What this run did not cover.**
- Play by a person. The operator played `7e44b8a0` the same day: see the next entry.
- DirectX 11 was not compared with OpenGL, and F10 does not switch games under DirectX 11
  ([#955](https://github.com/penelopeduncan/redshipblueship/issues/955)).
- Networking is untested.

**Findings.** None in this run. The open issues at this commit were listed under "Open issues
at `7e44b8a0`" near the top of this document, since refreshed at `7671196a`.

---

## Playtest, 2026-10-17, `7e44b8a0`

The operator played the release candidate by hand: the `rsbs-windows` artifact of the
`generate-builds` run at `7e44b8a0`, on DirectX 11 (the default for a fresh folder), in a fresh
folder whose `oot.o2r` and `mm.o2r` the first-launch flow had extracted from both ROMs. `main`
was at `66dc8b93`, which adds only documentation to `7e44b8a0`.

**What was played.**
- The first-launch extraction flow, all four scenarios, checked on 2026-10-16 on PR
  [#945](https://github.com/penelopeduncan/redshipblueship/pull/945)'s build and recorded on that pull request.
- A new paired file.
- Shops in both games selling crossed items. An ice trap sold under a decoy name worked as
  intended.
- The Happy Mask Shop crossing, both ways.
- Play in Termina.
- The Combo Item Tracker overlay, in both games.

**Findings.** All are filed. None is a crash, and none blocks the tag (the operator's call).
- [#964](https://github.com/penelopeduncan/redshipblueship/issues/964): OoT's shop shelves and get-item cutscenes show the "?" stand-in for Majora's Mask
  items whose MM draw is custom (boss remains, stray fairies, the seahorse, the Moon's Tear, a
  bottled fairy). Seen on a Kokiri Shop shelf selling Twinmold's Remains; the purchase worked.
- [#965](https://github.com/penelopeduncan/redshipblueship/issues/965): frame interpolation does nothing in Majora's Mask. The shared graphics processor
  interpolates with OoT's recorder while MM records into its own.
- [#966](https://github.com/penelopeduncan/redshipblueship/issues/966): an audit of the settings Majora's Mask does not honour in the single executable,
  opened from the operator's "enhancements aren't applying to MM".
- [#962](https://github.com/penelopeduncan/redshipblueship/issues/962): the overlay's default position covers the HUD's hearts. Enable Dragging and Window
  Type on Combo > Tracker Settings move it.
- [#963](https://github.com/penelopeduncan/redshipblueship/issues/963): the overlay's Majora's Mask section is text until Majora's Mask has started.
- [#967](https://github.com/penelopeduncan/redshipblueship/issues/967): shared capacity tiers such as the Bow show per game instead of once in the Shared
  column.
- [#971](https://github.com/penelopeduncan/redshipblueship/issues/971): the Shared column reads the pool as of the last save or crossing, not the live game.
- [#968](https://github.com/penelopeduncan/redshipblueship/issues/968), [#969](https://github.com/penelopeduncan/redshipblueship/issues/969) and [#970](https://github.com/penelopeduncan/redshipblueship/issues/970): requests for Shared Magic Arrows, Shared Bunny Hood and Shared
  Bottles rules.

**What this playtest did not cover.** A crossing after quitting and relaunching the game, a
crossing after a save made during play, "[Both Games] Shared Kokiri
Sword" on, MM's game-over prompt ([#694](https://github.com/penelopeduncan/redshipblueship/issues/694)), OoT's icon in MM's get-item textbox ([#607](https://github.com/penelopeduncan/redshipblueship/issues/607)),
OpenGL compared with DirectX 11, and networking. On DirectX 11 F10 does not switch games
([#955](https://github.com/penelopeduncan/redshipblueship/issues/955)). (This list once also named "the cross-game
warp songs"; at this commit none were built. They are since lane XW12, see [#1169](https://github.com/penelopeduncan/redshipblueship/issues/1169).)

---

## Gates, 2026-10-20, `7671196a`

The 21 pull requests merged after PR [#974](https://github.com/penelopeduncan/redshipblueship/pull/974) (2026-10-18) up to `7671196a`. Like the runs
above, these gates cover the checks hosted CI cannot run, because they need the ROMs.

**How it was run.** The workstation's main Windows checkout, OpenGL only, the ROM archives
mounted, windows opened without taking focus and, since PR [#1041](https://github.com/penelopeduncan/redshipblueship/pull/1041), test processes silent.
Five pull requests were gated alone, each as a merge of its branch onto the `main` of that
moment (the last, [#1058](https://github.com/penelopeduncan/redshipblueship/pull/1058), after the batches): [#1000](https://github.com/penelopeduncan/redshipblueship/pull/1000), [#1041](https://github.com/penelopeduncan/redshipblueship/pull/1041), [#1001](https://github.com/penelopeduncan/redshipblueship/pull/1001) and [#1004](https://github.com/penelopeduncan/redshipblueship/pull/1004). The rest were gated in batches: each
batch verify-merged several branches onto `main` at once, the pull requests were then
squash-merged one by one, and the merged tree was checked equal to the gated tree. Batch b1
was a verification run only and was not merged as a set. PRs [#986](https://github.com/penelopeduncan/redshipblueship/pull/986) and [#989](https://github.com/penelopeduncan/redshipblueship/pull/989) (tooling
and documentation) are not in the table.

| Gate | Pull requests | `redship` | `rando` | `ui`, parity lint | `integration` | `integration-soak` |
|---|---|---|---|---|---|---|
| #1000 alone | [#1000](https://github.com/penelopeduncan/redshipblueship/pull/1000) (`253b8d49`) | 185 of 185 | 50 of 50 | 2 of 2 | 18 of 18; `IntResourceCacheOwner` is new | 1 of 1 |
| #1041 alone | [#1041](https://github.com/penelopeduncan/redshipblueship/pull/1041) (`4453be07`) | 187 | 50 | 2 | 18 | 1 |
| #1001 alone | [#1001](https://github.com/penelopeduncan/redshipblueship/pull/1001) (`b9748b5d`) | 187 | 55 (four new goldens and the player-path row) | 2 | 18 | 1 |
| #1004 alone | [#1004](https://github.com/penelopeduncan/redshipblueship/pull/1004) (`29e8dba5`) | 188 | 55 | 2 | 18, after a rerun (below) | 1 |
| b2 | [#1049](https://github.com/penelopeduncan/redshipblueship/pull/1049), [#1045](https://github.com/penelopeduncan/redshipblueship/pull/1045), [#985](https://github.com/penelopeduncan/redshipblueship/pull/985), [#1052](https://github.com/penelopeduncan/redshipblueship/pull/1052), [#1046](https://github.com/penelopeduncan/redshipblueship/pull/1046), [#1042](https://github.com/penelopeduncan/redshipblueship/pull/1042), [#1043](https://github.com/penelopeduncan/redshipblueship/pull/1043) (up to `0bea298d`) | 195 of 195 | 55 of 55 | 2 of 2; lint plain and no-grow OK | 18 of 18 | 1 of 1 |
| b3 | [#988](https://github.com/penelopeduncan/redshipblueship/pull/988), [#1053](https://github.com/penelopeduncan/redshipblueship/pull/1053), [#1054](https://github.com/penelopeduncan/redshipblueship/pull/1054) (up to `e496f625`) | 196 | 55 | 2 | 19; `IntCrossingRefusedNoOoTArchive` is new | 1 |
| b4 | [#1002](https://github.com/penelopeduncan/redshipblueship/pull/1002), [#1048](https://github.com/penelopeduncan/redshipblueship/pull/1048), [#1051](https://github.com/penelopeduncan/redshipblueship/pull/1051), [#1056](https://github.com/penelopeduncan/redshipblueship/pull/1056) (up to `ee35d2c6`) | 200 | 55 | 2 | 19 | 1 |
| #1058 alone | [#1058](https://github.com/penelopeduncan/redshipblueship/pull/1058) (`7671196a`) | 201 | 55 | 2 | 19 | 1 |

At `7e44b8a0` the tiers stood at 184, 49, 2, 17 and 1.

**The rerun at #1004.** The first `integration` run failed `IntPairedFirstCrossing`,
`IntPairedFirstCrossingDiverged` and `IntPairedFirstCrossingTwoCycles`: each logged
"[MM] spoiler: REFUSED to write this world's OoT spoiler over './Randomizer/...json': it is
another paired world's spoiler". PR #1004 changed every world's fingerprint, and the build
folder still held spoilers of the same seeds from older runs. With those spoilers deleted the
same built tree passed 18 of 18. The defect is filed as [#1055](https://github.com/penelopeduncan/redshipblueship/issues/1055) (see "Randomizer
generation" below): a test-isolation gap, and a trap a player can meet too.

**Generated worlds.** No placement moved. PR #1001 added four golden files (nine in all),
PR #1004 re-pinned only the fingerprint field (`comboSettingsHash`) of the goldens that carry
it, and PR #1049 added two lines (`mmFreeStoneCount`, `mmFreeStoneDigest`) to each
complete-world golden.

**What these gates did not cover.**
- Play by a person: nobody has played `7671196a`. Section N of the playtest guide lists what
  to check by hand.
- DirectX 11: F10 on DirectX 11 ([#955](https://github.com/penelopeduncan/redshipblueship/issues/955)) is fixed in the libultraship fork, and no gate runs
  that backend.
- Networking is untested.

**Findings.** [#1055](https://github.com/penelopeduncan/redshipblueship/issues/1055), above. The open issues at this commit are listed under "Open issues
at `7671196a`" near the top of this document.

---

## Gates, 2026-10-22, `30b841c1` (`v0.3.0-prealpha`)

The 33 commits after `v0.2.0-prealpha`, pull requests #1073 to #1108, listed in
[`release-notes-next.md`](release-notes-next.md). Like the runs above, these cover the checks hosted CI cannot run,
because they need the ROMs.

**How it was run.** The workstation's main Windows checkout, OpenGL only, the ROM archives mounted, test windows that
take no focus and test processes that play no sound, driven by the pyramid driver. #1073 to #1100 were gated in its
batches: each pull request's `[lock]` commits built alone on `main` with their rows seen red, then one build of the
batch and the `redship`, `rando` and `ui` tiers with the parity lint, then `integration` and `integration-soak`; a
failure was bisected to its branch. #1081 to #1100 reached `main` through the landing PRs #1103 (cycle 13) and #1110
(cycle 16), whose hosted CI rounds were green, and the merged trees were checked equal to the gated ones. #1102 and
#1104 to #1108 were merged once their hosted CI was green. The driver then ran the ROM tiers on `main` itself:

| Cycle | `main` at | `integration` | `integration-soak` |
|---|---|---|---|
| 17 | `db8d38e8` (#1073 to #1100) | 20 of 20 | 1 of 1 |
| 18 | `2a57d1c6` (with #1102, #1104, #1105, #1107, #1108) | 21 of 21; `IntMoonCrashCycle` is new | 1 of 1 |
| 19 | `30b841c1` (with #1106: the code of `v0.3.0-prealpha`) | 21 of 21 | 1 of 1 |

**Generated worlds.** No golden moved: `git diff 1ace3c7b 30b841c1 -- tests/golden/` is empty.

**What these gates did not cover.** Play by a person: nobody has played `30b841c1`; section P of the playtest guide
lists what to try first. The New Cycle moon crash has run only in `IntMoonCrashCycle`. DirectX 11 is not gated, and
networking is untested.

**Findings.** None in these runs. The open issues at this commit were listed under "Open issues at `30b841c1`" near the
top of this document; that list has been refreshed since ("Open issues at `bc09231c`").

## Gates, 2026-10-23, `main` after `v0.3.0-prealpha`

The pull requests merged after the tag, #1111 to #1144, listed under "Next release (draft)" in
[`release-notes-next.md`](release-notes-next.md). Since the replan of 2026-10-22 each was merged once its hosted CI
was green, and the pyramid driver runs the ROM tiers on `main` itself (same workstation, OpenGL, ROM archives
mounted, no focus, no sound):

| Cycle | `main` at | `integration` | `integration-soak` |
|---|---|---|---|
| 20 | `bc93e65e` (with #1111 to #1119) | 16 of 21: five paired-creation rows waited for a log line #1116 renamed (issue #1135) | 1 of 1 |
| 21 | `f416ecee` (with #1122, #1124 and the fix, #1136) | 21 of 21 | 1 of 1 |
| 22 | `fabe7d33` (with #1137, #1138, #1125) | 16 of 21 | 0 of 1 |

**Cycle 22 was red because of a game controller, not the code.** The failing rows were `IntGameplayRoundtrip`,
`IntPairedFirstCrossing`, `IntPairedFirstCrossingDiverged`, `IntPairedFirstCrossingTwoCycles`,
`IntPairedFirstCrossingForeignSpoiler` and `IntGameplayRoundtripSoak`. They stopped on two kinds of message: the
camera stayed put while Link walked ("camera is anchored"), or a warp landed at the wrong entrance (0x0033 instead
of 0x00B1, or 0x0171 instead of 0x01D1 on the return leg). The driver's re-run of those six rows failed five, with
the messages moving between rows. The triage ([#1152](https://github.com/penelopeduncan/redshipblueship/issues/1152)) found the operator's
gamepad driving the test windows: libultraship tells SDL to deliver joystick events to a window that has no focus,
and a test window never takes focus, so a moved stick walked Link (the camera assertion failed) and pressed buttons
took doors or loaded a file (warps and boots arrived at the wrong entrance), at a different point each run. The
driver's bisect assumed that the last green tip, `f416ecee`, was still green; it was not under the pad, so the bisect
named the first commit after it, #1137, and opened a revert, PR [#1150](https://github.com/penelopeduncan/redshipblueship/pull/1150). The revert
was closed unmerged and #1137 cleared: with the pad idle the same rows passed at `e3de3ff1` (cycle 23, in the next
section: this code plus #1144's curated icons), and since PR [#1153](https://github.com/penelopeduncan/redshipblueship/pull/1153) a test process ignores game controllers altogether.
The advice this section first gave, to read an unexpected crossing landing or a camera that stops following Link as
this finding, is withdrawn: on a build of your own, either is a finding to report.

**Generated worlds.** With every new option at its default no world moved: of the golden digests only
`mmPlacementHash` moved, in eight files, because it counts Majora's Mask's 53 new tree checks whether or not they
are shuffled (#1125); `git diff f2f3d64f 4fc4fd41 -- tests/golden/` is that field plus the two `mmReachable*`
fields #1114 retired.

## Gates, 2026-10-23, `af4d941f`

The pull requests merged after cycle 22: the curated tracker icons of PR [#1144](https://github.com/penelopeduncan/redshipblueship/pull/1144) and the docs of PR
[#1148](https://github.com/penelopeduncan/redshipblueship/pull/1148), then the Combo Tracker's
reachability (PR [#1147](https://github.com/penelopeduncan/redshipblueship/pull/1147)), Majora's Mask's rows in the Combo Item Tracker (PR
[#1149](https://github.com/penelopeduncan/redshipblueship/pull/1149)) and test processes that ignore game controllers (PR
[#1153](https://github.com/penelopeduncan/redshipblueship/pull/1153)). Same workstation and driver as above (OpenGL, ROM archives mounted, no focus,
no sound), on `main` itself:

| Cycle | `main` at | `integration` | `integration-soak` |
|---|---|---|---|
| 23 | `e3de3ff1` (with #1144, #1148; the pad idle) | 21 of 21 | 1 of 1 |
| 24 | `af4d941f` (with #1147, #1149, #1153) | 21 of 21 | 1 of 1 |

Cycle 23 ran the code that was red in cycle 22, plus #1144's curated icons and #1148's docs, and passed every row,
which cleared #1137 (see the section above).
From cycle 24 on, a controller connected to the workstation cannot reach a test process (#1153).

**Generated worlds.** No golden moved: `git diff 4fc4fd41 af4d941f -- tests/golden/` is empty.

**What these gates did not cover.** Play by a person: nobody has played `af4d941f`; section R of the playtest guide
lists what to try. The reachability capture at a real departure and the Combo Item Tracker's new rows in a running
Majora's Mask were exercised by test rows only. DirectX 11 is not gated, and networking is untested.

**Findings.** None in these runs. The open issues at this commit were listed under "Open issues at `af4d941f`" near
the top of this document; that list has been refreshed since ("Open issues at `bc09231c`").

## Gates, 2026-10-26/27

The pull requests merged after the `v0.3.1-prealpha` tag (`038ae33d`), listed under "Release notes: next release
(draft)" in [`release-notes-next.md`](release-notes-next.md). Each was merged once its hosted CI was green, and the
pyramid driver ran the ROM tiers on `main` itself (same workstation, OpenGL, ROM archives mounted, no focus, no
sound, controllers ignored since #1153); from cycle 29 it runs the unit tiers on `main` as well:

| Cycle | `main` at | `redship` | `rando` | `integration` | `integration-soak` |
|---|---|---|---|---|---|
| 25 | `7c1aecf9` (`v0.3.1-prealpha` plus the docs of #1174) | not run | not run | 21 of 21 | 1 of 1 |
| 27 | `095c8b83` (with #1177, #1179, #1180, #1181 and #1183) | not run | not run | 21 of 21 | 1 of 1 |
| 28 | `51bcfa4c` (with #1185, #1186, #1188, #1178, #1184 and the docs of #1193) | not run | not run | 21 of 21 | 1 of 1 |
| 29 | `0be75d87` (with #1201, #1192, #1197 and #1199) | 244 of 244 | 77 of 77 | 21 of 21 | 1 of 1 |
| 30 | `38a5138c` (with #1194, #1198, #1206, #1207, #1209 and the docs of #1208) | 248 of 248 | 78 of 78 | 21 of 21 | 1 of 1 |
| 31 | `722da4f8` (with #1211, #1212, #1213, #1223 and the docs of #1214) | 252 of 252 | 79 of 79 | 21 of 21 | 1 of 1 |
| 32 | `3772e70d` (with #1205, #1224, #1225, #1227 and #1230) | 260 of 260 | 79 of 79 | 21 of 21 | 1 of 1 |
| 33 | `c9fd36de` (with #1232, #1226, #1202, #1234 and the docs of #1241) | 262 of 262 | 80 of 80 | 21 of 21 | 1 of 1 |
| 34 | `7ec19332` (with #1243, #1242 and #1244) | 263 of 263 | 80 of 80 | 21 of 21 | 1 of 1 |
| 35 | `9ceea4bc` (with #1245, #1247, #1248, #1246, #1249, #1251, #1253 and #1250) | 265 of 265 | 80 of 80 | 21 of 21 | 1 of 1 |
| 36 | `11b2212b` (with #1257, #1252, #1258, #1255, #1265, #1261, #1254 and #1263) | 267 of 267 | 80 of 80 | 21 of 21 | 1 of 1 |
| 37 | `791f26d3` (with #1256, #1262, #1264 and #1266) | 271 of 271 | 81 of 81 | 21 of 21 | 1 of 1 |
| 38 | `347ba755` (with #1268, #1259, #1260, #1267, #1270, #1269 and #1271) | 273 of 273 | 81 of 81 | 21 of 21 | 1 of 1 |

**`v0.4.0-alpha`.** The release commit changes only the version string (`src/common/rsbs_version.h`), the release workflow's title and docs, so cycle 38 at `347ba755` is the code record for `v0.4.0-alpha`.

Cycle 25's first run was killed by a user interrupt on the workstation, not by a test; the relaunch is the run
recorded above.

**Why the gate on `main` runs the unit tiers now.** Issue [#1200](https://github.com/penelopeduncan/redshipblueship/issues/1200):
`ComboCrossingViewsWorld` (`rando`) was red in `main`'s hosted CI from `d94ab061` until PR
[#1201](https://github.com/penelopeduncan/redshipblueship/pull/1201). Neither #1178 nor #1184 was red alone: #1184
named Majora's Mask's chu packs by count in the tracker, and #1178's logic gates moved the pinned seed so that a
single Bombchu crossed into Hyrule, where the tracker's "1 Bombchu" no longer matched the spoiler's "Bombchu". Cycle
28 ran the ROM tiers only and did not see it. Since #1200 the driver runs `redship` and `rando` on `main` too.

**Generated worlds.** Cycle 25 moved no golden. Cycle 27 covers #1181, which re-pinned `ootHintDigest` in eight
golden digests (a junk stone now draws a gag line). Cycle 28 covers #1178, which re-pinned the same eight files'
placement, crossing and hint fields (the Romani Ranch and Circus Leader gates). Cycle 29 moved no golden (#1201 adds
a spoiler field no golden holds; #1192, #1197 and #1199 do not touch generation), nor did cycles 30 and 31: #1194
(Keep Items on Cycle Reset is off by default and adds no identity term when off), #1198, #1206, #1207, #1209, #1211,
#1212, #1213 and #1223 move no golden. None of the merges in cycle 32 moves one either (#1205 changes runtime data that
generation never reads; #1230 changes no logic), nor do those of cycles 33 to 36 (#1226's theater gate is runtime, the
debug seed of #1234 and #1265 is made only when its rows are set, and the Majora's Mask settings rows touch no
generation). Cycle 37 covers #1264, which lifted the 64-crossing limit per side and moved the seed generator to
version 2: `comboSettingsHash` moved in all eight golden digests (the generator label only), and
`seed-digest-shopsanity`'s Majora's Mask placement, coordinator, hint and free-stone digests moved, because that world
reached exactly 64 crossings into Hyrule (its crossings did not move; the branch with the bound set back to 64
reproduced the old golden byte for byte). `seed-digest-armed-caps`' placements did not move.

**What these gates did not cover.** Play by a person: beyond the operator's Woodfall run of 2026-10-27 on the
`347ba755` build (U1, [#1070](https://github.com/penelopeduncan/redshipblueship/issues/1070) resolved), nobody has played these builds; sections S, T, U, V and W of the
playtest guide list what to try. The Majora's Mask settings rows are locked for their page, key and linked code;
what each does in play, the Cosmetics Editor's colours and HUD placement in Termina, the Item Tracker's new groups
and an outbox past 64 pickups were seen by no gate. The `ui` tier is not part of the gate on `main`. The Termina hookshot reticle, the mask, fairy and heart chests, the key models, the
Master Sword respawn and the presets skip were exercised by test rows only, and nobody has seen the reticle, the
chests or the keys on screen. Alt Assets toggling in Termina, the Combo Tracker's tooltip and scroll, the
load-time reconcile, the retired SoH tracker windows, the Song of Time with Keep Items on Cycle Reset and the `.otr`
handle release were exercised by test rows only; the Fast64 skeleton fix was also run with the operator's mod folder
on the title demo and a crossing, before merging. DirectX 11 is not gated, and networking is untested.

**Findings.** None in cycles 25 and 27 to 38; #1200 was found by hosted CI, not by a gate. The open issues at the tip
were listed under "Open issues at `347ba755`" near the top of this document; that list has been refreshed since ("Open issues at `bc09231c`").

---

## Save loss and corruption

### Files from `v0.5.0-alpha` and earlier are refused (save format version 6, PR [#1535](https://github.com/penelopeduncan/redshipblueship/pull/1535))

**What you see.** When you pick a file saved by `v0.5.0-alpha` or any earlier build, a plain (non-randomizer) file included, the game shows "Refused: File made by an older version" and does not open it. The file stays on disk. Files from `v0.5.1-alpha` open normally: the save format did not change in `v0.5.2-alpha`. A file that this version has saved does not open in `v0.5.1-alpha` or any older build ("File made by another build" there). Seed packages are a separate matter: this version uses seed generator 6, so it refuses packages from `v0.5.1-alpha` and every earlier build (see "Seed packages from `v0.5.1-alpha` and earlier builds are refused" under "Randomizer generation").

**Why.** `v0.5.1-alpha` stores more checks and settings for both games, with room for the ones upstream keeps adding, so the save format went from version 5 to version 6. Old files are not converted.

**What to do.** To finish an old file, keep the build that wrote it in its own folder. To use the slot on this build, erase the file and create a new one. Saves made with development builds between releases are not supported.

Older save breaks and save-loss bugs (the `v0.3.0-prealpha` break, `efe573ec`, `5ea06a9c`, the load-time loss of money and ammo on `v0.3.1-prealpha`, the carpenters' cells of PR #1205, and the shifted randomizer data of files from before `v0.4.3-alpha`) no longer matter: this version refuses every file from `v0.5.0-alpha` and earlier. Their entries were removed from this document on 2026-11-17 and are in its history.

### ~~A changed MM option or Cross-Game Rule breaks the pair for the session~~ — RESOLVED ([#781](https://github.com/penelopeduncan/redshipblueship/issues/781); [#564](https://github.com/penelopeduncan/redshipblueship/issues/564))

Fixed by the #781 change: **the file's own rules win at load.** The pages are still
editable between sessions, and what they hold there is staging for the next file.
Loading a paired file now compares both its Cross-Game Rules and its Majora's Mask
profile (the same digest the crossing checks) and puts the file's own values back into
the pages:
- a changed Cross-Game Rule (Goal, Crossing Direction, Shared Kokiri Sword; Shared Ocarina until #879 retired it) is
  restored, and a toast reads "Restored from file:" with the rows it reset
  (for example "Restored from file: Goal, Crossing Direction");
- a changed MM option or trick is restored from the file's MM half, and a toast reads
  "Restored for Majora's Mask:" with the rows it reset (for example "Restored for
  Majora's Mask: Starting Hearts +1"; a long trick name is cut with "...", and rows
  that do not fit are counted).

The file loads paired, the slot stays writable, and the next crossing into Majora's Mask
agrees with the file. The cases the file cannot answer stay visible instead of silent:
- ~~an MM identity input the file does not record (the excluded-check list or the
  starting-item block, which no page in this build edits) loads the file paired but
  posts "Not restored: Majora's Mask options differ", and the crossing is refused until
  they match;~~ **RESOLVED (#836):** such a file is not opened ("Refused: Majora's
  Mask options differ" at the file select);
- a Cross-Game record field no page authors (only a file from another build can differ
  there) is refused with "Refused: File made by another build" (a damaged record:
  "Refused: Cross-game record is damaged"). The field names are on stderr. ~~The OoT
  file still opens and plays without its Majora's Mask half, because the load runs after
  OoT has opened the file; the next crossing into Majora's Mask says so ("Not paired:
  Termina stays un-randomized") instead of skipping pairing silently. Nothing is saved
  to the pair that session.~~ **RESOLVED (#836, operator ruling 2026-10-01: "unpaired is
  not an acceptable scenario"):** a refused file is not opened. Pressing A on it in the
  Ocarina of Time file select plays the error sound, posts the refusal toast, and keeps
  you on the file list; Dev Tools > Save Files (Combo > Save Files before PR [#1248](https://github.com/penelopeduncan/redshipblueship/pull/1248)) shows the same words. The same applies to a
  randomizer file whose cross-game record is missing ("Cross-game record is missing":
  a deleted record, or a file made with the file select's Copy, which copies Ocarina of
  Time's file only) and to every damaged record. Nothing is written, renamed or deleted.
  Two ways out: put a good copy of `Save/redship_slot<N>.redsave` back (N = file
  number - 1) and open the file again, or erase the file in the file select. A
  `.refused-*.bak` left by an earlier build is worth renaming back only when its
  refusal was not damage; "File made by another build" opens in that build.

The original report, kept only for matching old logs (historical; it describes the
behaviour before the fix):

> Open, and a trap between sessions. Combo → MM Randomizer and Combo → MM Tricks lock
> only while a creation stamp is resident (`Combo_MMProfileFrozen()` is
> `mmProfileDigest != 0`), and that stamp is zero on a fresh launch and after a return to the
> title screen, so both pages are editable exactly between sessions. Loading a paired file
> compares its Cross-Game Rules field by field against the live ones, but it does **not**
> recompute the MM profile: an edited MM option, trick, excluded
> check or starting item is accepted at load. The next crossing into Majora's Mask recomputes
> the profile, sees the difference and refuses: a 15-second "Cross-game pairing REFUSED"
> toast, an un-randomized Termina, and the slot latched against writes for the session
> (PR [#570](https://github.com/penelopeduncan/redshipblueship/pull/570)). The file on disk is untouched.
>
> Cross-Game Rules (including the Goal) unlock between sessions the same way
> (`Combo_ComboSettingsFrozen()` is `comboSettings.formatVersion != 0`). A changed rule *is*
> caught at load, but quietly: `LoadSlot` prints the diverged fields to stderr only and
> latches the slot, and OoT's caller ignores the result, so the OoT file opens and plays
> with nothing saved to the pair. By code reading (not run), the pairing identity is not
> restored either, so the next MM arrival skips pairing and plays an un-randomized Termina
> without a toast. No in-game surface shows the refusal (the `.redsave` file panel lives in
> the never-instantiated `ComboMenuBar`). (2026-09-28: `ComboMenuBar` is deleted; Combo >
> Save Files shows a refused file as "Not paired:" with its reason.)
>
> **Workaround:** once a paired file exists, leave both MM pages and Cross-Game Rules alone;
> if you changed one, set it back exactly or recreate the file. **The fix to come** is the
> same MM-profile compare at load time, plus a load refusal the player can see; neither
> exists yet.

### ~~A flag set in the scene you leave through the portal can be lost~~ — RESOLVED ([#635](https://github.com/penelopeduncan/redshipblueship/issues/635), community report; tracked in [#638](https://github.com/penelopeduncan/redshipblueship/issues/638), PR [#650](https://github.com/penelopeduncan/redshipblueship/pull/650))

Fixed by PR #650 (2026-09-11). Live scene flags are now flushed before every departure freeze, in
both games and on both switch paths. The original report, kept for matching old saves:

Collect the Heart Piece on the Clock Tower, walk straight out through the portal to
Ocarina of Time, come back: the Heart Piece is there again, and can be collected
again. The cross-game departure freezes MM's save **before** the scene-flag flush
that a normal scene transition performs, and that flush never runs — so anything
you picked up or triggered in the scene you left through the portal (collectibles,
chests, switches, in that scene only) is not in the frozen save. Progress in every
*other* scene is safe.

On builds older than PR #650, the workaround was to leave and re-enter the scene through any door
or loading zone before crossing.

### ~~F10 during MM's game-over screen hands over an empty health bar~~ — RESOLVED ([#626](https://github.com/penelopeduncan/redshipblueship/issues/626), PR [#650](https://github.com/penelopeduncan/redshipblueship/pull/650))

Fixed by PR #650 (2026-09-11). A dead MM health bar is now revived before every departure freeze.
The original report:

Health is a shared resource between the two games, and it is applied as-is on
arrival. Pressing the F10 debug hot-swap while MM's game-over prompt is up freezes
the save with zero health and switches; you arrive in OoT with an empty bar. The
normal game-over exit (choosing not to continue) already revives you on the way out
(PR [#625](https://github.com/penelopeduncan/redshipblueship/pull/625)); the F10
route bypasses it.

On builds older than PR #650, the workaround was not to press F10 on the game-over screen.

### ~~F10 during OoT's game-over screen hands MM a one-heart bar~~ — RESOLVED ([#664](https://github.com/penelopeduncan/redshipblueship/issues/664), PR [#753](https://github.com/penelopeduncan/redshipblueship/pull/753))

Fixed by PR #753 (2026-09-27), the OoT twin of #650's revive. A dead OoT bar is revived before
the freeze to OoT's own continue value: three hearts, or full capacity with "Spawn with Full
Health". If a bottled fairy was spent at the killing blow, both games give the fairy's refill
instead (OoT 20 hearts, MM 10, clamped to capacity). Before the fix MM arrived with one heart,
its arrival floor. A pre-release OoT half already frozen dead stays dead in its blob, and OoT's
arrival floors it at one heart.

### ~~MM's paired half could be wiped by the moon crash or an owl save~~ — RESOLVED ([#765](https://github.com/penelopeduncan/redshipblueship/issues/765), PR [#772](https://github.com/penelopeduncan/redshipblueship/pull/772))

Fixed by PR #772 (2026-09-28). The creation event left MM's half with slot number 0, a real
flash slot, so the moon-crash reset and the owl save's readback copied an empty save buffer
over the live one (observed in a headless row: day, rupees and inventory wiped). The half is
now authored as MM's own new-file path authors it, and every arrival pins the cross-game slot
number, which also covers older files (see "Back up your saves"). The fix is locked
headlessly and has not been played; the playtest guide's scenario I4 checks it in game.
It fixed the wipe only; the half then stayed on day 4 after a crash, fixed separately
(next entry). To see whether a file's MM half exists, open the Combo Tracker's Majora's
Mask panel: MM's world under its header, versus nothing under the header (its notes were
cut by PR [#929](https://github.com/penelopeduncan/redshipblueship/pull/929)).

### ~~A moon crash in a paired MM half leaves the cycle on day 4~~ — RESOLVED ([#785](https://github.com/penelopeduncan/redshipblueship/issues/785), PR [#789](https://github.com/penelopeduncan/redshipblueship/pull/789))

`Interface_StartMoonCrash` sets day 4 and 06:00 before the crash cutscene, and vanilla
rolls the cycle back when `Sram_ResetSaveFromMoonCrash` reloads the file from flash. A
cross-game session has no flash slot, so that reload was skipped and the half stayed on
day 4 (Final Hours clock, no Dawn of the First Day, no second crash). Now the reload is the
file's **last save**: the last whole `.redsave` commit (an owl save, the autosave, the Song
of Time, a save in OoT, your last crossing between the games, or the file's creation), with
both halves and the cross-game records
restored together. This is what vanilla's reload does and what OoTMM's default
("Last Save") moon crash does. Since [#1671](https://github.com/penelopeduncan/redshipblueship/issues/1671)
Moon Crash Behavior defaults to New Cycle (a new cycle that keeps your progress, as the Song of Time does), so this
entry is about files that choose Last Save, and about files created before that change, which keep Last Save. Loading
such a file sets the Moon Crash Behavior row to Last Save (the load puts the file's own value back), and a file you
create after that inherits it until you change the row. What to expect under Last Save:

- MM progress since that save is lost, as in vanilla. After an owl save or autosave the
  clock resumes at that save's day and time; the Dawn of the First Day plays only when the
  last save was a Song of Time or the file's creation (a file never saved restarts as
  created). A save taken within about ten in-game minutes of the crash is pulled back to
  05:49 on the final day, OoTMM's grace period, so the moon cannot fall again at once.
- **A crash no longer rolls OoT back** ([#837](https://github.com/penelopeduncan/redshipblueship/issues/837)).
  Every crossing into Termina is itself a save of the whole file, so OoT's half in the
  commit the crash restores is OoT as you left it. Without a save in Termina since you
  entered, MM comes back as you last left Termina (or as created, if you never left it).
- **A chest you opened in Termina before you left stays open** ([#837](https://github.com/penelopeduncan/redshipblueship/issues/837)).
  Leaving Termina keeps the scene flags (chests, switches, collected items, cleared rooms)
  the way an owl save does, so a crash that restores that crossing's save does not close
  a chest whose item you kept. Before this fix such a chest came back closed and opening it
  again gave its item a second time.
- Shared rupees, health, magic and ammo come back at the shared pool's value as of that
  save, applied to MM as an arrival applies them. They do not come back at MM's own
  balance from its last departure. After a save in OoT these two differ, and without the
  apply MM would have refunded what OoT spent (fixed in the same PR; row
  `mm-moon-crash-pool-applied`).
- A cross-game item that reached MM after the save is not lost: it is delivered again at
  your next arrival in MM.
- Where you wake (traced in source, not played): the crash cutscene ends in the Clock
  Tower interior with the Happy Mask Salesman's scene (the same destination the Skip Moon
  Crash enhancement sets, `ENTRANCE(CLOCK_TOWER_INTERIOR, 3)`), then you walk out into
  South Clock Town. The Dawn of the First Day card needs the restored clock at day 0
  before 06:01, so it does not appear after an owl save or autosave.
- A session with nothing saved at all (a refused slot, a debug boot) has nothing to reload;
  there the clock alone restarts at dawn. Nothing else about the cycle is reset on that
  path: cycle events (`weekEventReg`) and the rest of the half keep their pre-crash
  values, apart from what vanilla's own tail clears (event flags, cycle scene flags from
  the permanent ones, timers).
- **Decision 16, ruled 2026-10-01 (option (b)):** every cross-game crossing is a save
  commit, as OoTMM's `comboGameSwitch` saves at every game switch. This build commits the
  whole file at every crossing, door or F10, in both directions (#837), so the last save
  never predates the crossing and a crash cannot take back unsaved OoT play. A crash
  therefore usually resumes mid-cycle, from the crossing into Termina; OoTMM does the same.
  Resuming mid-cycle after an owl save or autosave (vanilla deletes the owl save and
  reloads the last Song of Time save) follows OoTMM's default too.

Locked headlessly (`mm-creation-new-file`, `mm-creation-new-file-world`,
`mm-moon-crash-never-saved`, `mm-moon-crash-pool-applied`); not played. The playtest guide's scenario I4 checks it in game.

### ~~F10 hot-swap silently rolls back your progress~~ — RESOLVED ([#364](https://github.com/penelopeduncan/redshipblueship/issues/364), PR [#400](https://github.com/penelopeduncan/redshipblueship/pull/400))

F10 now freezes the departing game's state and sets a return entrance, the same
as the portal switch, and the frozen state is cleared when consumed. The historical
symptom — resuming from the *other* game's save bytes after an F10 switch — is
gone. The game-over case above is fixed too.

### ~~A malformed save permanently deadlocks all saving~~ — RESOLVED ([#370](https://github.com/penelopeduncan/redshipblueship/issues/370), PR [#391](https://github.com/penelopeduncan/redshipblueship/pull/391))

`saveMtx` is released on the exception path and the file is no longer truncated
before the write that could throw.

### ~~Cross-game save format will change~~ — it did; see "Back up your saves"

The `reserved` padding, the version window and the refused state that entry asked
for all exist now. The advice stands: the format may change again.

---

## Crashes and hangs

### ~~Custom skeleton mods crash at Link's first draw~~ — RESOLVED ([#1190](https://github.com/penelopeduncan/redshipblueship/issues/1190), PRs [#1197](https://github.com/penelopeduncan/redshipblueship/pull/1197), [#1198](https://github.com/penelopeduncan/redshipblueship/pull/1198), [#1199](https://github.com/penelopeduncan/redshipblueship/pull/1199))

From the January symbol namespacing until PR #1197, a custom skeleton mod whose limbs use Fast64's `gEmptyDL`
marker for a limb with no mesh (the Heroine of Time Malon pack, for example) crashed where Link is first drawn: on
the title screen with Alt Assets on, or when child Link first appeared. The namespacing pass had renamed the marker
inside the code's string, so the limb asked for a display list that does not exist. On an older build, disable the
pack or turn Alt Assets off. The same pass broke the capital P's width in Ship of Harkinian's text auto-wrap, so
lines with names such as "Pocket Egg" ran past the text box's edge (fixed by PR #1198). Issue
[#1195](https://github.com/penelopeduncan/redshipblueship/issues/1195) is resolved by PR
[#1206](https://github.com/penelopeduncan/redshipblueship/pull/1206): the namespacer now skips string literals and
comments, and its sweep found one more damaged lookup key, Ocarina of Time's debug-print font path, now restored (a
texture pack's replacement debug font is picked up again; nothing changes without such a pack). That closes the
class; a Python invariant fails on any key-shaped literal that names a game-prefixed symbol. The report's second finding, every mod archive
mounted again after the curated archives, is the intended re-apply; until PR #1199 each re-added `.otr` kept the
replaced copy's file open, at boot and on every game switch. Since PR #1199 a release build's crash trace prints
each frame's module offset, which resolves against `redship.map`. Issue
[#1063](https://github.com/penelopeduncan/redshipblueship/issues/1063), a texture pack that worked for a while and then crashed, was a different report; the
operator closed it on 2026-10-31, with no reproduction recorded on it. Its strongest candidate cause, found on
2026-11-02 and not proven, is the render-thread texture cache freeing entries a later draw still uses
([#1352](https://github.com/penelopeduncan/redshipblueship/issues/1352), fixed after `v0.4.4-alpha` by PR
[#1356](https://github.com/penelopeduncan/redshipblueship/pull/1356)).

### ~~Majora's Mask's shooting gallery, the Southern Swamp route and the staff roll use Ocarina of Time's resources~~ — RESOLVED ([#981](https://github.com/penelopeduncan/redshipblueship/issues/981), PR [#1000](https://github.com/penelopeduncan/redshipblueship/pull/1000))

Fixed by PR #1000 (2026-10-19). Both games' archives carry some of the same resource paths
(the Octorok's skeleton `objects/object_okuta/gOctorokSkel`, the treasure chest, the get-item
shelf models, the staff roll's text), and the resource cache was keyed by path alone, so the
game that loaded such a path first supplied it to the other for the rest of the session. After
an Octorok in Ocarina of Time (Lake Hylia, for example), Majora's Mask was handed OoT's 37-limb
skeleton for its 15-limb Octoroks, and every frame wrote past the end of their joint tables:
the Clock Town shooting gallery crashed, and so could the Southern Swamp route's Octoroks. MM's
staff roll read OoT's credits table. Now each game loads under its own cache owner, set at every
switch before the arriving game's archives are re-added: a shared path is parsed once per game,
and a game coming back gets its own earlier copies, so nothing is freed or reloaded at a switch.
The libultraship fork gained the owner setter (its PR #9). Locked by `ResourceCacheOwner`
(redship tier) and `IntResourceCacheOwner` (integration tier); not played. To check it by hand
(the playtest guide's N1): in a session started in Ocarina of Time, fight an Octorok at Lake
Hylia, cross through the Happy Mask Shop door, play a round at the Clock Town shooting gallery
(the Octoroks show, and leaving does not crash), visit the Southern Swamp's Octoroks, and if you
can, watch Majora's Mask's staff roll (its text reads correctly). Back in Hyrule the Lake Hylia
Octoroks, chests and shop shelves look as before. The follow-ups, one of which a player can
meet (the Cosmetics Editor used while Majora's Mask runs), are [#1040](https://github.com/penelopeduncan/redshipblueship/issues/1040). A sibling, every
switch opening one more copy of every re-added archive ([#987](https://github.com/penelopeduncan/redshipblueship/issues/987)), is fixed by PR [#1045](https://github.com/penelopeduncan/redshipblueship/pull/1045).

### ~~Opening Network → Anchor in a wide window displaces the game view~~ — RESOLVED ([#634](https://github.com/penelopeduncan/redshipblueship/issues/634), community report; tracked in [#640](https://github.com/penelopeduncan/redshipblueship/issues/640), PR [#651](https://github.com/penelopeduncan/redshipblueship/pull/651))

Fixed by PR #651 (2026-09-11). `soh_port` now links whole-archive, so the page's registrar is kept,
and a menu page with no widgets consumes its own window position. The original report:

On Windows, open the menu, go to `Network → Anchor` while the window is wider than
roughly 800 px (maximized, for example): the page's content column is empty, and
when you close the menu the game view is pushed off to a black rectangle. The page
has no widgets because its menu registrar is dropped by the linker from the
`soh_port` archive, and the empty page still positions the main game window. Not
specific to fullscreen or DirectX 11.

On builds older than PR #651, the workaround was to narrow the window first or avoid the page.

### ~~Every normal exit heap-corrupts on Windows (Fault A)~~ — RESOLVED ([#396](https://github.com/penelopeduncan/redshipblueship/issues/396))

**Fixed and operator-confirmed 2026-07-21.** `redship --version` now exits cleanly
with status `0`; the heap corruption on normal exit is gone.

Historical detail, for anyone reading old crash reports: Windows builds used to die
with `STATUS_HEAP_CORRUPTION` (`0xC0000374`, detected inside `ntdll`) during **normal
process exit**, the minimal repro being `redship --version`. The root cause was not
the `/FORCE:MULTIPLE` CRT duplication originally suspected: `OTRExporter/Main.cpp` and
`VersionInfo.cpp` were compiled into **both** the `OTRExporter_OoT` and `OTRExporter_MM`
static libs, so seven-plus global objects were constructed twice and destroyed twice —
the second destructor walking freed heap. The fix (PR
[#413](https://github.com/penelopeduncan/redshipblueship/pull/413), submodule pointer
bump) namespaces the exporter globals per variant. A permanent strong-DATA-symbol CI
gate over the two exporter archives (PR
[#430](https://github.com/penelopeduncan/redshipblueship/pull/430)) prevents the class
from recurring.

If you ever see `0xC0000374` on exit again, it is a **new** regression, not this one —
start from the exporter-archive symbol gate.

### ~~Crashes in CI/headless mode present as a 180-second hang~~ — RESOLVED ([#388](https://github.com/penelopeduncan/redshipblueship/issues/388), PR [#394](https://github.com/penelopeduncan/redshipblueship/pull/394))

The crash handler no longer blocks in `SDL_ShowSimpleMessageBox` without a display;
headless crashes fail fast with an exit code.

### ~~Audio subsystem can wedge across a switch~~ — RESOLVED ([#365](https://github.com/penelopeduncan/redshipblueship/issues/365) PR [#416](https://github.com/penelopeduncan/redshipblueship/pull/416); [#371](https://github.com/penelopeduncan/redshipblueship/issues/371) / [#378](https://github.com/penelopeduncan/redshipblueship/issues/378) PR [#398](https://github.com/penelopeduncan/redshipblueship/pull/398); [#377](https://github.com/penelopeduncan/redshipblueship/issues/377) PR [#412](https://github.com/penelopeduncan/redshipblueship/pull/412))

OoT's audio buffer fill is guarded on its initialized flag, the sequence maps are
zero-initialized and bounds-checked before registration, and the audio probe observes
the reset handshake instead of bypassing it. If you still get silence or a hang after
a switch with custom music installed, it is a new report.

---

## Majora's Mask specific

MM is the newer half of the combo and still carries more debt than OoT.

### ~~With Enemy Drops shuffled, the trackers show a drop as available in places you cannot get it~~ — RESOLVED ([#1498](https://github.com/penelopeduncan/redshipblueship/issues/1498), PR [#1506](https://github.com/penelopeduncan/redshipblueship/pull/1506))

Fixed by PR #1506 and changed again by PR #1705: the Check Tracker's Majora's Mask pane lists a drop under every
area where the enemy lives, and with Enable Available Checks on each copy's lock shows whether the kill there is in
logic. Majora's Mask's own Check Tracker greys each area's copy on its own and names the areas in the drop's
tooltip (when Out of Logic Checks is not Normal). Nobody has played it yet. The original report:

An enemy drop is one check per kind of enemy: killing any one of them anywhere collects it. The
trackers have only one answer for the whole check, so once one of those enemies can be reached,
the drop reads as available under every area where that enemy lives, reachable or not, and the
Check Tracker's Majora's Mask pane files each drop under one area that may not be the one you can
reach. Majora's Mask's own tracker shows no logic tooltip for drops, and with Only Show Current
Scene on it never lists them. The seed itself is fine: one reachable enemy of the kind is enough.
Look for that enemy elsewhere before you give up on a green drop.

### ~~By night, the Peahat's drop reads as in logic, but the Peahat cannot be hurt~~ — RESOLVED ([#1398](https://github.com/penelopeduncan/redshipblueship/issues/1398), PR [#1516](https://github.com/penelopeduncan/redshipblueship/pull/1516))

Fixed by PR #1516: the Peahat's drop and its grotto chest are in logic only by day. The original report:

With Enemy Drops shuffled, the Peahat grotto's drop counts as reachable at night, but a grown
Peahat buries itself at night and cannot be damaged, and the small ones it sends out do not give
the drop. Kill it by day. With Shuffle Time on as well and only night half-days owned, the
randomizer can put an item you need on that drop; you then need a day half-day first. The fix
is part of the Majora's Mask logic packet, #1398.

### ~~A second copy of a Tingle map sells as junk while the shop shows the map~~ — RESOLVED ([#1499](https://github.com/penelopeduncan/redshipblueship/issues/1499), PR [#1507](https://github.com/penelopeduncan/redshipblueship/pull/1507))

Fixed by PR #1507: with Shuffle Tingle Maps on, a new file's pool holds one copy of each Tingle map (none with Starting
Maps and Compasses on), and a shop or Tingle slot whose map you already own shows and sells junk, once. A file created
before the fix keeps its world. Still open: after such a junk sale the trackers name the map again, and nobody has
checked the Bomb Shop keeper's hand or Tingle's text in play. The original report:

With Shuffle Tingle Maps on (off by default), the item pool holds two copies of every Tingle map.
Once you own a map, every other copy you find is junk: the shelf text reads "Junk", Tingle gives
nothing, and the sale still takes the rupees and can be bought again. The Bomb Shop keeper still
holds the map in his hand, and the trackers and the Bomb Shop hint still name it. With Starting
Maps and Compasses on, the copies left in the world are junk from the start, and Plentiful adds
more. Nothing you need is lost: the first copy of each map works.

### ~~A borrowed Ocarina of Time mask is shown as the Majora's Mask mask whose spot it uses~~ — RESOLVED ([#1500](https://github.com/penelopeduncan/redshipblueship/issues/1500), PR [#1505](https://github.com/penelopeduncan/redshipblueship/pull/1505))

Fixed by PR #1505: the C button, the D-pad and the pause name show the borrowed mask, and so does the A-button
description when it is the spot's only mask. Nobody has played it yet. The original report:

With `[Both Games] Gerudo Mask (MM)`, `Skull Mask (MM)` or `Spooky Mask (MM)` on (all off by
default), those masks borrow a spot of Majora's Mask's own mask screen (the Stone Mask's for the
Gerudo and Skull Masks, the Gibdo Mask's for the Spooky Mask). Getting one and the mask screen's
icon say the right mask, but its C-button icon, its name and description on the mask screen, and
its D-pad icon show the mask whose spot it borrows (a Gerudo Mask reads as the Stone Mask).
Wearing it still puts on the right mask and does what that mask does.

### Some shared settings do less, or nothing, in Majora's Mask

Since the merges after `v0.2.0-prealpha` every row the #966 audit filed works in Majora's Mask except these:

- **Ocarina of Time only:** D-pad in text boxes, D-pad hold change, D-pad on
  the pause screen and Customize Ocarina Controls (Majora's Mask has no code for them), Skip Song Cutscenes, Quick
  Boss Deaths and Exclude Glitch-Aiding Cutscenes (no Majora's Mask twin), and Skip To File Select (the title
  screen is Ocarina of Time's). ~~Damage Multiplier~~ has a Majora's Mask row of its own since PR [#1119](https://github.com/penelopeduncan/redshipblueship/pull/1119)
  ("MM Damage Multiplier", Enhancements > Difficulty, under Ocarina of Time's; the lists differ: Majora's Mask's
  goes to 16x or 1 Hit KO, Ocarina of Time's to 256x), and ~~Skip Boss Introductions~~ (Majora's Mask's Skip Enemy
  Cutscenes) works in Majora's Mask since PR [#1124](https://github.com/penelopeduncan/redshipblueship/pull/1124).
- **Cutscene skips that follow another row in Majora's Mask** (PR [#1124](https://github.com/penelopeduncan/redshipblueship/pull/1124)): its song-teacher cutscenes follow
  Skip Story Cutscenes, its boss blue-warp cutscenes follow Skip Boss Introductions, and Tatl's long flashback at
  the tree in southern Termina Field, on the road to the swamp, follows Skip Misc Interactions, as in
  2Ship2Harkinian. Skip First Cycle has no row: the crossing already starts Majora's Mask past its first cycle.
- **Cutscene-skip defaults differ** (PR [#1124](https://github.com/penelopeduncan/redshipblueship/pull/1124)): in a randomizer file whose config never touched the
  cutscene-skip checkboxes, Ocarina of Time skips story, entrance and boss-intro cutscenes by default but Majora's
  Mask plays them. Tick the boxes once to apply them to both.
- **Fast Chests** does nothing in a Majora's Mask randomizer file: the chest contents belong to the randomizer, as in
  2Ship2Harkinian (PR [#1086](https://github.com/penelopeduncan/redshipblueship/pull/1086)).
- **Permanent Heart Loss** turned off restores lost heart containers in Ocarina of Time only (PR [#1099](https://github.com/penelopeduncan/redshipblueship/pull/1099)).
- **Free Look** in Majora's Mask ignores the remote bombchu's camera focus (PR [#1089](https://github.com/penelopeduncan/redshipblueship/pull/1089)).
- **Skip Get Item Animations** (Randomizer > General) now decides Majora's Mask's get-item scenes too; Majora's Mask's
  own Skip Item Get Cutscene key is no longer read, and a value set by hand in `shipofharkinian.json` is ignored
  (PR [#1100](https://github.com/penelopeduncan/redshipblueship/pull/1100)).
- **Kept apart on purpose** (the Majora's Mask settings programme, PRs [#1243](https://github.com/penelopeduncan/redshipblueship/pull/1243) to [#1271](https://github.com/penelopeduncan/redshipblueship/pull/1271)): where the two
  games' rules differ, Majora's Mask has its own "MM" row beside SoH's, and SoH's row changes Ocarina of Time only.
  Text Speed (MM Fast Text), the aiming and camera inversion and sensitivity rows, the climb speed, Enable Bombchu
  Drops (MM Enable Bombchu Drops converts drops only once you hold Bombchus), the speed modifiers and Power Crouch
  Stab are such pairs. SoH's Mirrored World mirrors Termina, but its Dungeons (MQ) mode leaves Termina unmirrored,
  since Majora's Mask has no Master Quest. In the Cosmetics Editor a rainbow colour holds still in Majora's Mask, and
  Randomize All, Reset All and Lock All skip the Majora's Mask rows.

### Shuffle Ocarina: what is not yet seen in a running game, and what stays as it is ([#1671](https://github.com/penelopeduncan/redshipblueship/issues/1671))

Shuffle Ocarina (Randomizer > MM Randomizer, Additional Items, above Shuffle Ocarina Buttons) puts Majora's Mask's
Ocarina of Time in the item pool, so Termina starts with the Song of Time and no ocarina, and it makes the moon crash
New Cycle whatever Moon Crash Behavior says. It is frozen when the file is created. The integration row
`IntMMShuffleOcarina` drives the start, Day 3 midnight in North Clock Town and the moon crash; nobody has played
such a file yet. Until someone has:

- **Song checks without the ocarina.** Romani's Epona's Song check, Flat's tomb (the Song of Storms check), the Song
  of Soaring engraving, the Goron Elder's Lullaby intro, the Elegy of Emptiness and the Oath to Order give their
  item even if you have no ocarina yet. The imprisoned monkey (Sonata of Awakening), the Goron baby (Goron Lullaby)
  and the Zora eggs (New Wave Bossa Nova) need the ocarina in your hands, and logic expects you to have it there.
  This comes from reading the code on 2026-11-17: nobody has played these in a Shuffle Ocarina file yet.
- **The Clock Tower roof without the ocarina.** By the same reading, the roof's checks can be collected, you can
  leave, and letting the roof's timer run out starts a new cycle like any other moon crash. No automated run
  drives the roof with no ocarina.
- **Under the No Logic rung** the ocarina may land anywhere, somewhere you cannot reach without it included, as with
  every item under OoTMM's No Logic.
- **The Clock Tower roof save.** OoTMM's New Cycle lets you save on the Clock Tower roof; this build does not yet
  (the "Song of Time" item of "What the Shared Items rules do not do yet", below). With Shuffle Ocarina that applies
  to every crash.
- **Leftovers of the first cycle, accepted as they are.** Some of Majora's Mask's actors still look at whether you
  hold the ocarina, and have no hook to tell them a paired file is past its first cycle. None of them changes what
  you can reach:
  - From midnight on the Final Day, the Clock Tower seen from outside South Clock Town (Termina Field, the other
    districts) keeps its closed look, without the fireworks, until you hold the ocarina. The stairs to the roof are
    there from midnight whether you hold it or not, and in South Clock Town the tower opens at midnight as usual.
  - The Happy Mask Salesman chooses the cutscene he plays by whether you hold the ocarina (and the Deku Mask), so
    without it he may play the one meant for the first cycle.
  - In a Skull Kid cutscene he is drawn holding the ocarina while you have none.
  - Tatl's first-cycle hints (what she says when you call her, and one of her prompts in Clock Town), which vanilla
    stops once you hold the ocarina, can still play.

### The Song of Time takes your Powder Keg and Magic Beans unless Keep Items on Cycle Reset is on — this is intentional ([#1175](https://github.com/penelopeduncan/redshipblueship/issues/1175), PR [#1194](https://github.com/penelopeduncan/redshipblueship/pull/1194))

A Powder Keg found in Hyrule arrives in Termina as one keg. Majora's Mask's three-day reset then empties it, and any
Magic Beans, as in vanilla Majora's Mask and in OoTMM; rupees and the shared ammo are kept in every world, and the
pictograph is always lost. To keep the keg and the beans, turn on Keep Items on Cycle Reset (Randomizer > MM
Randomizer, after Moon Crash Behavior) before you create the file: it is frozen at creation, covers the Song of
Time, the new day after the credits and a New Cycle moon crash, and, when on, changes the placements a seed makes.

### Dying in MM respawns you at the entrance instead of showing a game-over prompt — this is intentional

MM's vanilla death behaviour (reload at the area entrance with three hearts, no
"Continue?" prompt) is the combo's shipped default; it is not a regression. The
2ship game-over prompt enhancement (`gEnhancements.Kaleido.GameOver`) is off by default.
Since PR [#695](https://github.com/penelopeduncan/redshipblueship/pull/695) ([#682](https://github.com/penelopeduncan/redshipblueship/issues/682)) you can turn it on: Game Over Prompt, in the "Majora's Mask" block of Enhancements → Quality of Life (Combo → Majora's Mask before PR [#1248](https://github.com/penelopeduncan/redshipblueship/pull/1248)).
ADR 0009's death-decline autosave machinery and #626's F10-during-game-over case
only apply once that enhancement is enabled — operator ruling, 2026-09-16
([#653](https://github.com/penelopeduncan/redshipblueship/issues/653)).

### A reload after a Termina save opens in Hyrule — this is intentional, as in OoTMM (PR [#1317](https://github.com/penelopeduncan/redshipblueship/pull/1317), [#1298](https://github.com/penelopeduncan/redshipblueship/issues/1298), [#1302](https://github.com/penelopeduncan/redshipblueship/issues/1302))

A reload after a Termina save opens in Hyrule, as in OoTMM; the first crossing into Termina after the reload arrives
at the entrance the save recorded: a temple save returns to the temple, and with Remember Save Location on any pause
save or autosave returns to its place (otherwise to South Clock Town). The paired file is one game, and it always
opens on Ocarina of Time's file select and in Hyrule, whichever game you saved in. PR #1317 backported
2Ship2Harkinian's pause-save entrance and respawn persistence (2Ship `16ef59bae`, `96611c3b3`), so the save keeps
where you pause-saved; the release notes of `v0.4.1-alpha` first said the reload itself returned there, which is not
so for a paired file. The return on the first crossing comes with PR [#1353](https://github.com/penelopeduncan/redshipblueship/pull/1353)
([#1348](https://github.com/penelopeduncan/redshipblueship/issues/1348)), merged after `v0.4.4-alpha`.

### ~~The Reset button does nothing while Majora's Mask runs~~ — RESOLVED ([#1274](https://github.com/penelopeduncan/redshipblueship/issues/1274), PR [#1315](https://github.com/penelopeduncan/redshipblueship/pull/1315))

Fixed by PR #1315 (2026-10-29). The `reset` console command and the menu header Reset button / Ctrl+R now dispatch to Majora's Mask's reset handler when Majora's Mask is the active running game. The original report:

**What you saw.** With Majora's Mask running, the red Reset button in the menu header (the undo icon beside Quit, tooltip "Reset (Ctrl+R)") and the Ctrl+R shortcut did nothing: no reset, no message. In Ocarina of Time both reset as before.

**Why.** Both send the console command `reset` (`games/oot/soh/SohGui/Menu.cpp`, `libultraship/src/ship/window/gui/Gui.cpp`). In the single executable that command was Ship of Harkinian's, registered through the gate of [#798](https://github.com/penelopeduncan/redshipblueship/issues/798), which refused every Ship of Harkinian command unless Ocarina of Time was the running game and wrote the refusal only into an output the button discarded. Majora's Mask's own `reset` lived in 2Ship2Harkinian's developer console, which the single executable did not link. PR #1315 routes console reset to `MM_gGameState` when Majora's Mask is running.

### ~~Woodfall Temple does not rise~~ — RESOLVED ([#1070](https://github.com/penelopeduncan/redshipblueship/issues/1070), retest passed 2026-10-27; no code changed for it)

**Retest.** On 2026-10-27 the operator played the Mido's House debug seed (`docs/playtest-fixtures/midos-house-sonata-deku`, generator 2, seed name `20-36-63-29-61`) on the playtest build of `347ba755`: opened the Mido's House chests, crossed at the Happy Mask Shop, received the Sonata of Awakening and the Deku Mask at the Clock Tower, stood on the Deku Scrub symbol at Woodfall as Deku Link and played the Sonata: the temple rose at the first attempt. No code changed for this issue: the two causes below were the position box of Majora's Mask's own trigger and the D-pad ocarina of `v0.2.0-prealpha` ([#1062](https://github.com/penelopeduncan/redshipblueship/issues/1062), fixed by PR [#1099](https://github.com/penelopeduncan/redshipblueship/pull/1099) in `v0.3.0-prealpha`). The history, as it stood before the retest:

Woodfall Temple did not rise (playing the Sonata of Awakening at the Woodfall platform had no effect). Two reports, with different causes. **The operator's 2026-10-25 laptop run** is explained by Majora's Mask's own trigger, which checks Link's position (vanilla `DmChar01`; with Dungeon Access at its default, Transformation & Song, which that seed used): as Deku Link you must stand on the Deku Scrub symbol on the floor between the pink flower and the owl statue (x within 40, z between 1000 and 1078). The flower and the Song of Soaring's arrival point are both outside it: played there, the Sonata gives its full announcement and nothing happens. A short chime the moment Deku Link takes out the pipes confirms the spot (playtest guide, section U). The forensic pass over that save, log and seed found no defect on the song's side: the Sonata, bought in a Hyrule shop, crossed and was redeemed in Termina before the first try, and a song redeemed at a crossing is identical to one learned. **The 2026-10-21 report** (the chime heard on the platform, but no song with C-up recognized, on more than one visit and after a reload) came from a playtester using the D-pad ocarina on `v0.2.0-prealpha`, where the D-pad ocarina played no note in Termina in any of the four directions ([#1062](https://github.com/penelopeduncan/redshipblueship/issues/1062), fixed by PR [#1099](https://github.com/penelopeduncan/redshipblueship/pull/1099) in `v0.3.0-prealpha`: Majora's Mask's gate read a key that only 2Ship2Harkinian's menu, which the single executable leaves out, could write). No code path on `main` treats C-up apart from the other notes; PR [#1232](https://github.com/penelopeduncan/redshipblueship/pull/1232) added a row, `MMOcarinaDpadNotes`, that drives the D-pad through Majora's Mask's real note path. The notes, from Majora's Mask's song table: the Sonata of Awakening is C-up C-left C-up C-left A C-right A, the Song of Soaring C-down C-left C-up C-down C-left C-up, and the Elegy of Emptiness C-right C-left C-right C-down C-right C-up C-left. Majora's Mask's free-play rule still applies: L held while a note is played cancels the song (`code_8019AF00.c`), so do not hold L while playing in Termina. The monkey's vanilla Sonata as you leave the Deku King's chamber is fixed (PR [#1225](https://github.com/penelopeduncan/redshipblueship/pull/1225), [#1203](https://github.com/penelopeduncan/redshipblueship/issues/1203) closed). **What closed it:** a retest on the latest build, with the debug seed `docs/playtest-fixtures/midos-house-sonata-deku` (the Sonata, the Deku Mask and an Empty Bottle in Mido's House with Closed Forest off; PRs [#1234](https://github.com/penelopeduncan/redshipblueship/pull/1234), [#1265](https://github.com/penelopeduncan/redshipblueship/pull/1265) and, for generator 2, [#1268](https://github.com/penelopeduncan/redshipblueship/pull/1268)), which reaches the Woodfall platform without a seed playthrough, and the ten-minute D-pad note check: turn on Dpad Ocarina Playback (Settings > Controls, Ocarina Controls) and the Input Viewer (Settings > Input Viewer > Toggle Input Viewer), take out the ocarina in Termina, and press each D-pad direction: each must sound its C button's note and add its arrow to the staff while the Input Viewer shows the press. For a C-up match without owning any song, teach the Scarecrow a song with C-up in it and play it back (playtest guide, V9). The ocarina-button view of [#1072](https://github.com/penelopeduncan/redshipblueship/issues/1072) is in since PR #1075, so a missing C-up shows in the trackers.

### ~~First arrival in Clock Town reads 08:00 instead of 06:00~~ — RESOLVED ([#636](https://github.com/penelopeduncan/redshipblueship/issues/636), community report; tracked in [#639](https://github.com/penelopeduncan/redshipblueship/issues/639), PR [#648](https://github.com/penelopeduncan/redshipblueship/pull/648))

Fixed by PR #648 (2026-09-11). A first MM arrival now re-authors the new-file clock, so the dawn
sequence runs. The original report:

The first crossing from OoT into a new MM file lands in South Clock Town at Day 1,
8:00 AM, with no dawn sequence, instead of a new file's 6:00 AM. No time value
leaks from OoT: the 8:00 is MM's own title-screen attract-demo clock, which the
first-entry path never re-authors. You lose two hours of the first day and the
dawn telop; nothing else is wrong with the clock.

### ~~MM enhancements do not initialize in single-executable builds~~ — RESOLVED ([#384](https://github.com/penelopeduncan/redshipblueship/issues/384) PR [#408](https://github.com/penelopeduncan/redshipblueship/pull/408); [#516](https://github.com/penelopeduncan/redshipblueship/issues/516) PRs [#518](https://github.com/penelopeduncan/redshipblueship/pull/518), [#520](https://github.com/penelopeduncan/redshipblueship/pull/520), [#616](https://github.com/penelopeduncan/redshipblueship/pull/616))

The colliding registrar names were prefixed `MM_`, and the registrars that MM's
excluded boot sequence used to run are now called explicitly from the single-exe
boot path, with a link-time audit (`.github/scripts/check-registrar-elision.sh`)
and a runtime lock over the list. MM's enhancement archive still links as a plain
archive by design, so an MM toggle that appears enabled and does nothing is still a
plausible bug — report it.

### ~~MM code binds to OoT implementations in several places~~ — RESOLVED

Actor culling ([#382](https://github.com/penelopeduncan/redshipblueship/issues/382),
PR [#402](https://github.com/penelopeduncan/redshipblueship/pull/402)), framebuffer
effects ([#386](https://github.com/penelopeduncan/redshipblueship/issues/386), PR
[#436](https://github.com/penelopeduncan/redshipblueship/pull/436)), the C++ layout
mismatches ([#383](https://github.com/penelopeduncan/redshipblueship/issues/383), PR
[#434](https://github.com/penelopeduncan/redshipblueship/pull/434)) and the stub
signature drift ([#372](https://github.com/penelopeduncan/redshipblueship/issues/372)
PR [#415](https://github.com/penelopeduncan/redshipblueship/pull/415);
[#385](https://github.com/penelopeduncan/redshipblueship/issues/385) PR
[#401](https://github.com/penelopeduncan/redshipblueship/pull/401)) each got MM its own
definition. A symbol-collision baseline and a strong-symbol gate in CI keep the class
from returning.

### ~~Timers are not neutralized on the MM side of a switch~~ — RESOLVED ([#373](https://github.com/penelopeduncan/redshipblueship/issues/373), PR [#419](https://github.com/penelopeduncan/redshipblueship/pull/419))

### ~~MM text that says the player's name shows blanks in a paired world~~ — RESOLVED ([#773](https://github.com/penelopeduncan/redshipblueship/issues/773), PR [#782](https://github.com/penelopeduncan/redshipblueship/pull/782))

The paired creation now translates the name typed on OoT's file select into Majora's Mask's
character set (OoTMM's mapping; characters MM cannot draw become a space) and stamps it on MM's
half, so an MM textbox that names Link prints the OoT name. Files created before the fix keep
the all-space name. Not yet looked at in play.

### ~~MM hook dispatch is still partial~~ — RESOLVED ([#438](https://github.com/penelopeduncan/redshipblueship/issues/438), PR [#673](https://github.com/penelopeduncan/redshipblueship/pull/673))

Fixed by PR #673 (2026-09-17), which wired the last fourteen dormant MM hook types and
the guards their registrants needed. Before it, those hook types had no dispatch point
in the single-exe build, and options that depended on them were shown disabled-with-reason.
The actor-init, actor-draw and open-text hooks (PR
[#512](https://github.com/penelopeduncan/redshipblueship/pull/512)), the pause-menu
and file-select hooks (PR [#547](https://github.com/penelopeduncan/redshipblueship/pull/547))
and the item/progression trio (PR
[#630](https://github.com/penelopeduncan/redshipblueship/pull/630)) now dispatch.

---

## Switching and entrances

### ~~On DirectX 11, F10 does not switch games~~ — RESOLVED, pending a hand test ([#955](https://github.com/penelopeduncan/redshipblueship/issues/955))

Windows delivers F10 as a system key (`WM_SYSKEYDOWN`). The DirectX 11 window handled only
ordinary key messages, so F10 went to Windows, which opened its menu loop: the game seemed to
pause until another key was pressed, and no switch happened. DirectX 11 is the default backend
on a fresh Windows install. The libultraship fork's PR #11 (merge commit `2f612109`) now hands
F10 to the game and returns without Windows' default handling; other system keys keep it, so
Alt+F4 still closes the game. The OpenGL window always delivered F10. The fix reached `main` on
2026-10-20 through PR [#1045](https://github.com/penelopeduncan/redshipblueship/pull/1045)'s pin (`21b8fa4f`). No gate runs DirectX 11 and nobody has
pressed F10 on it yet: the playtest guide's N2 checks it. Until then, the doors and the OpenGL
backend are the sure ways across.

### ~~MM's clock-shuffle randomizer option was unreachable~~ — RESOLVED ([#678](https://github.com/penelopeduncan/redshipblueship/issues/678), PR [#679](https://github.com/penelopeduncan/redshipblueship/pull/679))

`RO_CLOCK_SHUFFLE` is now live and reachable on Randomizer → MM Randomizer. The two
`2ship_enh` translation units it depends on (`BetterSongOfDoubleTime.cpp`,
`SkipSoTCutscenes.cpp`) were dropped by plain archive linking because they
register only through a file-scope `RegisterShipInitFunc`; a targeted
`WHOLE_ARCHIVE` carve-out links and guards both, and `check-registrar-elision.sh`
now locks the registrars in. This is not a general `2ship_enh` link change
([#427](https://github.com/penelopeduncan/redshipblueship/issues/427) item 3
stays a separate decision).

### ~~Test and default entrance links collide~~ — RESOLVED ([#374](https://github.com/penelopeduncan/redshipblueship/issues/374), PR [#397](https://github.com/penelopeduncan/redshipblueship/pull/397))

Duplicate entrance-link registrations are rejected instead of silently shadowing.

### ~~An entrance bound is an unchecked literal~~ — RESOLVED ([#380](https://github.com/penelopeduncan/redshipblueship/issues/380), PR [#417](https://github.com/penelopeduncan/redshipblueship/pull/417))

### ~~Leaving a game mid-session leaves its actors' state behind~~ — RESOLVED ([#666](https://github.com/penelopeduncan/redshipblueship/issues/666) PR [#751](https://github.com/penelopeduncan/redshipblueship/pull/751); [#750](https://github.com/penelopeduncan/redshipblueship/issues/750) PR [#767](https://github.com/penelopeduncan/redshipblueship/pull/767))

A departure through the portal or with F10 retires the game's play state without running each
live actor's teardown. Both games now retire that session at departure: every overlay that
still had actors is reset once, and per-actor extension data (such as a rando pot's check
identity) is dropped. Symptoms this fixes, from reading the code: MM's three-day clock actor
killing itself on the next arrival (no night, dawn or moon crash until a scene change), a
skipped "Dawn of the First Day" on a second file, and Romani Ranch's alien defense pointing at
actors from the discarded session; in OoT, a missing Zora diving-game Zora, Spirit and Forest Temple lifts and water, and the Treasure
Chest Shop keeper for the rest of the process. Each departure logs `[OoT] Abandoned session
retired: ...` or `[MM] ...`.

### ~~OoT actor teardown writes to the save are skipped at a departure~~ — RESOLVED ([#770](https://github.com/penelopeduncan/redshipblueship/issues/770), PR [#792](https://github.com/penelopeduncan/redshipblueship/pull/792); [#807](https://github.com/penelopeduncan/redshipblueship/issues/807))

The teardowns PR #767 does not run also write the save. Fixed by PR #792 (2026-09-28): right
before a departure freezes OoT's save, the windmill's Song of Storms flag is cleared and Lake
Hylia's raised water is set again after the Water Temple (rando), as those actors' teardowns do
on any exit, and the lake's river water box is put back. A running room or minigame timer's
`timerState`, Sun's Song state and a magic effect's `magicState` are reset on the next OoT
arrival or file load (read in code, #770). ~~A sun switch lit by a Light Arrow under SoH's
Sunlight Arrows enhancement stays on after a departure, and an F10 between an age change and
its scene reload freezes the old `linkAge`~~ — RESOLVED (#807): the departure now turns that
sun switch off and applies the pending age with its equipment swap, as those teardowns do. State cleared only by destroy
hooks is not retired either: a remote bombchu's camera focus and the entrance-randomizer Epona
state (PRs [#751](https://github.com/penelopeduncan/redshipblueship/pull/751), [#767](https://github.com/penelopeduncan/redshipblueship/pull/767)).

---

## Randomizer generation

### Seed packages from `v0.5.1-alpha` and earlier builds are refused (seed generator version 6)

**What you see.** On a build with seed generator version 6, Randomizer > General > Seed Package > Load seed package on a package exported by `v0.5.1-alpha` or any earlier build is refused ("Seed package from another generator") and stages nothing. The same seed and settings also show a different fingerprint at file select than on `v0.5.1-alpha`, and they can make a different world, with the default settings too.

**Why.** A package records the generator version that made it, and a build loads only its own. Generator 6 changes the world a seed makes, with the default settings too. Death Mountain Crater logic follows Ship of Harkinian's rework. The Fire Trial rooms in Ganon's Castle are divided by door, and the Master Quest Fire Trial uses the heat timer instead of always asking for the Goron Tunic. With the new Voidout Collection trick off (the default), the Gold Skulltula outside Ganon's Castle and the Master Quest Shadow Temple Gold Skulltula near the boss need more than before (PR [#1664](https://github.com/penelopeduncan/redshipblueship/pull/1664), issue [#1647](https://github.com/penelopeduncan/redshipblueship/issues/1647)). Majora's Mask's Moon Crash Behavior now defaults to New Cycle ([#1671](https://github.com/penelopeduncan/redshipblueship/issues/1671)), which also moves default worlds. With their settings on, the Deku Stick Bag and Deku Nut Bag shuffles now gate both games ([#1652](https://github.com/penelopeduncan/redshipblueship/issues/1652)), and Link's Pocket no longer takes a shared song under Shuffle Songs: Anywhere ([#1552](https://github.com/penelopeduncan/redshipblueship/issues/1552)). Every package from an earlier build is refused, whatever its version.

**What to do.** Generate the seed again from the same seed and settings on the new build. The generator version does not affect saved files: a paired file keeps the world it was created with.

### ~~A spoiler from an older build with the same seed name leaves a new world without a spoiler~~ — RESOLVED ([#1055](https://github.com/penelopeduncan/redshipblueship/issues/1055), PR [#1076](https://github.com/penelopeduncan/redshipblueship/pull/1076))

**Fixed (PR #1076, 2026-10-21):** a paired seed whose spoiler name is already taken by another identity's spoiler (an older build's, or one made with other MM options) is now written beside it as `Randomizer/<icons>-<fingerprint>.json`, and the Randomizer menu's "Spoiler File:" line names it. The integration rows clear their seed's spoilers before creating, so a stale spoiler in the build folder no longer fails them. What it was:

A paired world's spoiler is named from the seed (`Randomizer/<seed icons>.json`), and a creation
never writes over another paired world's spoiler (PR [#937](https://github.com/penelopeduncan/redshipblueship/pull/937)): the old file is left as it is,
the new world gets no spoiler, and only stderr says so (`[MM] spoiler: REFUSED to ... it is
another paired world's spoiler`). The world itself is created and plays normally. Since PR
[#1004](https://github.com/penelopeduncan/redshipblueship/pull/1004) put the generator's version into the fingerprint, every spoiler a build before
`29e8dba5` wrote counts as another world's, so generating a seed you have played before on an
older build meets this. **Workaround:** before you create the file, delete or move the older
spoiler with the same seed name out of `Randomizer/` (or empty the folder, as the playtest
guide's setup says). The integration rows met the same trap in the #1004 gate (see "Gates,
2026-10-20"). The seed package ([#1044](https://github.com/penelopeduncan/redshipblueship/issues/1044), PR [#1116](https://github.com/penelopeduncan/redshipblueship/pull/1116)) names its file by seed and fingerprint, which
removes the collision.

### What the Shared Items rules do not do yet ([#1085](https://github.com/penelopeduncan/redshipblueship/issues/1085), PRs #1080 to #1095)

Each Shared rule on Randomizer > Cross-Game (under Shared Items) is off by default; these apply only with one on.

- ~~**Two copies of a shared song.**~~ RESOLVED by PR [#1111](https://github.com/penelopeduncan/redshipblueship/pull/1111) ([#1085](https://github.com/penelopeduncan/redshipblueship/issues/1085)): a shared song is placed
  once whatever Shuffle Songs says. `v0.3.0-prealpha` still places two copies unless Shuffle Songs is Anywhere.
  With Shuffle Songs Off the song now stays at its usual check in both games (Malon and Romani Ranch, the Windmill
  and Beneath the Graveyard, the Ocarina of Time and the Clock Tower roof), and the second of the two you reach gives
  junk (PR [#1616](https://github.com/penelopeduncan/redshipblueship/pull/1616), issue
  [#1561](https://github.com/penelopeduncan/redshipblueship/issues/1561)).
- **Not every far-game upgrade steps the shared pool at the pickup** (PR [#1122](https://github.com/penelopeduncan/redshipblueship/pull/1122)). Since #1122 a capacity
  upgrade found in the other game (bomb bag, quiver, wallet, magic, Double Defense, heart pieces and containers,
  hookshot, and bottles under Shared Bottles) raises your tier at once, and since
  [#1145](https://github.com/penelopeduncan/redshipblueship/issues/1145) so does the Kokiri Sword under Shared Kokiri
  Sword. Majora's Mask's sword does this only while the shared sword is still empty (neither game has handed you a
  sword yet); a later copy still crosses, because Majora's Mask turns it into the Razor or Gilded Sword. Under
  Shuffle Deku Stick/Nut Bag, Ocarina of Time's Progressive Deku Stick or Nut upgrade found in Termina while neither
  game has the bag is the bag, and it opens both games at the pickup
  ([#1652](https://github.com/penelopeduncan/redshipblueship/issues/1652)); a later one, and every one with the
  shuffle off, still crosses and arrives at Ocarina of Time's next arrival. The ocarina is never shared. An upgrade raised at the pickup does not bring the other
  game's give-time refill with it: a bomb bag that Majora's Mask gives fills your bombs, but one raised at the
  pickup leaves the shared bomb count where it was.
- **A shared item found in the other game is yours where you pick it up** (issue #1551): the Lens of Truth, Epona's
  Song, the Song of Storms, the Song of Time, the magic arrows, the shared masks, the shields, the ocarina buttons
  and swimming, when their Shared option is on. The Kokiri Sword and bottles already did this. A later Majora's
  Mask sword copy still crosses.
- **Starting with a shared item.** If you start with a shared item, the other game's logic does not count it yet,
  so logic may expect a little less than you have (issue #1552). Play is not affected.
- **A surplus copy under the plentiful pool.** With Plentiful, a shared item is placed once, as one copy for both
  games. OoTMM places one more shared copy of some of them (issue #1653).
- **Shields.** With Shared Shields on, a Mirror Shield found in Termina before any Hero's Shield also gives Ocarina of
  Time the Hylian Shield, Ocarina of Time's own Hylian Shield stays in the pool beside the shared one, and the Item
  Tracker lists each game's shield in its own section rather than once under Shared.
- **Bottles.** Shared Bottles shares the bottle count up to Ocarina of Time's four, with each bottle's contents kept
  per game, so Majora's Mask's fifth and sixth bottle slots stay empty. OoTMM's sharedBottles shares six bottles with
  their contents; the difference is deliberate and open to the operator (PR [#1094](https://github.com/penelopeduncan/redshipblueship/pull/1094)).
- **Song of Time** (PR [#1102](https://github.com/penelopeduncan/redshipblueship/pull/1102)). Moon Crash Behavior's New Cycle (the default since [#1671](https://github.com/penelopeduncan/redshipblueship/issues/1671),
  and forced by Shared Song of Time and by Majora's Mask's Shuffle Ocarina) does not yet let you save on the Clock
  Tower roof, as OoTMM's does. The New
  Cycle default itself differs from OoTMM, whose default is Last Save (the operator's 2026-11-16 ruling). Forcing New Cycle under the rule is itself a divergence from OoTMM,
  which credits time resets in its logic instead.

### Majora's Mask's trap types are fixed when the file is created ([#1047](https://github.com/penelopeduncan/redshipblueship/issues/1047), PR [#1049](https://github.com/penelopeduncan/redshipblueship/pull/1049))

The set of trap types you choose is saved with the world, and each trap check always springs the same kind of
trap. If you change the types before your first trip to Termina, loading the file puts them back, and a message
names them (for example "Restored for Majora's Mask: Blast Traps"). The types are the seven rows after Trap Count
on Randomizer → MM Randomizer, Freeze Traps to Time Traps. They are greyed once the world is created. With no type
chosen, every trap is a Freeze Trap. Checked by automated tests only; not played.

### The Voidout Collection trick names a Gold Skulltula that logic does not use it for

The trick's description lists a Water Temple Gold Skulltula (near the boss key chest), but logic does not use the
trick there yet (issue #1647). With the trick on, logic never expects you to get that Skulltula by voiding out.

### New Cycle cannot save on the Clock Tower roof yet ([#1671](https://github.com/penelopeduncan/redshipblueship/issues/1671))

Moon Crash Behavior defaults to New Cycle: when the Moon crashes, a new cycle starts and you keep your items. Unlike
OoTMM's New Cycle, you cannot save on the Clock Tower roof first. You can still choose Last Save on Randomizer →
MM Randomizer, unless Shuffle Ocarina or Shared Song of Time is on. A file made before this version keeps the
behaviour it was made with.

### ~~OoT's plentiful item pool can wrap the wallet inside the fill's own logic~~ — RESOLVED ([#726](https://github.com/penelopeduncan/redshipblueship/issues/726))

OoT's own fill now stops every progressive grant at the item's top tier, as the combo coordinator's
rounds already did. Before the fix, a **Plentiful** pool (one more wallet, bow, slingshot, bomb bag,
strength, scale and magic than balanced) walked the fill's simulated inventory past the top: on a
plentiful + tycoon-wallet seed the wallet read 0 after the fourth copy (observed), and every other
of those items read one tier past its top. `OoTPlentifulProgressive` (rando tier) locks it.

Plentiful worlds can differ from before the fix when the wrap happened mid-fill; balanced, scarce
and minimal worlds, and the shipped default, do not move (the golden rows are unchanged).

---

## Platform and packaging

### macOS is untested ([#1501](https://github.com/penelopeduncan/redshipblueship/issues/1501))

Releases include a macOS app (`redshipblueship-<tag>-macos.dmg`, Universal, macOS 15 or later), built by CI's
macOS job. Nobody has run it on a Mac yet. The dmg can be attached to a release some hours after the Windows and
Linux downloads, because the macOS build takes longer. Do not use the `v0.5.0-alpha` dmg: it lacks the two ROM
extraction tools and stops at "ZAPD extractor executable not found". From `v0.5.1-alpha` on the app carries both,
and CI refuses a dmg without them.

### ~~MM mods and texture packs never load~~ — RESOLVED ([#670](https://github.com/penelopeduncan/redshipblueship/issues/670), PRs [#704](https://github.com/penelopeduncan/redshipblueship/pull/704), [#716](https://github.com/penelopeduncan/redshipblueship/pull/716))

MM now mounts its asset mods in the single executable. Both games share one
`mods/` folder: the root is OoT's and `mods/mm/` is MM's, each searched
recursively, and a mod under one partition is never registered for the other
game. Layout and precedence are in `docs/MODDING.md`.

**Loose (unpacked) asset folders now mount for both games** (PR [#732](https://github.com/penelopeduncan/redshipblueship/pull/732), [#705](https://github.com/penelopeduncan/redshipblueship/issues/705)).
Put OoT's loose files in `<mods>/loose` and MM's in `<mods>/mm/loose`. Each game mounts its loose
folder after its packed mods, so a loose file wins over a packed one.

**MM's mods have their own order and enabled set** (PR [#764](https://github.com/penelopeduncan/redshipblueship/pull/764), [#706](https://github.com/penelopeduncan/redshipblueship/issues/706)): Settings → MM Mods
(Combo → MM Mods before PR [#1248](https://github.com/penelopeduncan/redshipblueship/pull/1248), beside Settings → OoT Mods, Ship of Harkinian's Mod Menu renamed) lists the archives under `mods/mm`, highest priority on top, with arrows to reorder, disable
and enable, and a Rescan button. Changes are saved at once and apply when MM next mounts its
mods: its first start in the session, or after a restart once it has loaded them (the page's
note says which). A scan that cannot see the whole folder leaves the list read-only.

Limits, all described in `docs/MODDING.md`:

- Symlinked subfolders inside `loose/` are not followed.
- Removing a file needs a restart.
- A top-level `version` file must be left out.
- While you are in MM, an OoT loose file is shadowed wherever MM's base archives ship the same path.

### You must supply your own ROMs

Asset extraction requires original Ocarina of Time and Majora's Mask ROMs. No ROMs
or extracted archives are distributed. See `docs/BUILDING.md` and
`docs/mm-archive-setup.md`.

### Config and settings are still Ship-of-Harkinian-shaped

The config file is still named `shipofharkinian.json` and lives in the `soh`
directory (`rsbs/src/main.cpp`). This is deliberate — it keeps existing Ship configs
working. The settings-migration issue
([#34](https://github.com/penelopeduncan/redshipblueship/issues/34)) closed with PR
[#461](https://github.com/penelopeduncan/redshipblueship/pull/461), which converged
MM's volume and tunic keys onto OoT's and added a config updater; the namespace
decision is ADR 0003 (`docs/adr/0003-settings-namespace.md`). A standalone 2Ship
install's `2ship2harkinian.json` is read once, on an install's first boot, by the 2Ship
settings import of PR #461; since PR [#1270](https://github.com/penelopeduncan/redshipblueship/pull/1270) it carries every Majora's Mask
setting that has a row, not only the shared ones. An install that has already booted
keeps `"TwoShipImport": {"Completed": true}` in `shipofharkinian.json` and is not
imported again until that entry is removed; custom colours under 2Ship's
`gCosmetic.Hearts`, `gCosmetic.Magic` and `gCosmetic.Player.Human` are not carried
([#1272](https://github.com/penelopeduncan/redshipblueship/issues/1272)).

In narrow windows (the ui tier's 832x600 profile, for one),
Settings → General's **Boot Sequence** dropdown is drawn over its own label and
**ImGui Menu Scaling**'s dropdown covers the end of its label. Ship of Harkinian
draws these rows the same way (right-aligned dropdowns as wide as their longest
option, in a column that narrows with the window), so they are kept as shipped
([#899](https://github.com/penelopeduncan/redshipblueship/issues/899)).

### Majora's Mask's randomizer pages are under Randomizer ([#1101](https://github.com/penelopeduncan/redshipblueship/issues/1101))

Since PRs [#1104](https://github.com/penelopeduncan/redshipblueship/pull/1104) to [#1107](https://github.com/penelopeduncan/redshipblueship/pull/1107) (2026-10-22, in `v0.3.0-prealpha`), Majora's Mask's
randomizer pages and the pair's rules are pages of the Randomizer header, after Tricks/Glitches: **Combo → MM Randomizer** is **Randomizer → MM Randomizer**, **Combo → MM
Tricks** is **Randomizer → MM Tricks**, **Combo → Cross-Game Rules** is **Randomizer → Cross-Game**, and **Randomizer →
MM Locations** is new. Older reports and the records below use the old names. A menu last left on a moved page opens
on it in its new place. The MM Randomizer page's sections carry Ship of Harkinian's names: "Logic & Conditions",
"Shuffle Options" and "Items" are gone, and an option is under Logic, Win Condition, Area Access, Shuffle Items,
Shuffle Shops & Merchants, Additional Items, Traps, Hints or Starting Items.

**The Randomizer sidebar scrolls in a small window.** It has 16 entries (Ship of Harkinian's 12 and our four), and Ship
of Harkinian's own 12 already scrolled at 832x600. The pull requests estimated from the menu code, without measuring,
that about 10 entries fit at 832x600 and about 14 at 1280x800, so at 1280x800 the last entries scroll too (#1104,
#1106). Nobody has looked at it on screen.

### Some menu entries are stubs

Settings entries backed by unimplemented functionality are grayed out or labeled
where they were caught. Randomizer → MM Randomizer shows no per-row liveness label; its disabled
rows are the retired "Majora Access: Remains" and "Final Hours Start Time", and seven that read "Not Yet Supported
in Paired Worlds" (Skeleton Key, Shuffled Gold Skulltulas, Stray Fairies in Pool, Song of Double Time, Inverted Song
of Time, Sun's Song and Saria's Song). Ocarina of Time's Shuffle Silver Rupees, Shuffle Chest Minigame and its Chest
Minigame Keyring are no longer greyed out (#1530). This pass was not exhaustive —
an enabled-looking toggle that does nothing is a plausible bug, and worth reporting.

### The "Fipps" overlay font choice is gone

`Fipps-Regular.otf` shipped with the inherited port trees and asserted "All rights
reserved" with no license grant, so it was deleted (2026-09-21; see
`THIRD_PARTY_NOTICES.md`). It is no longer offered in the overlay font picker. It
was never the default, and a saved overlay-font selection that still names it — or
names anything else that is not loaded — falls back to **Press Start 2P** rather
than leaving a dead entry in the picker.

**Your saved selection is rewritten, once.** The setting is
`gSettings.OverlayFont` in `shipofharkinian.json` (older configs spell it
`gOverlayFont`; the game's own config migration renames it for you). Because the
fallback is a font that IS loaded, the first launch after this change stores
`Press Start 2P` over whatever the key used to say and flushes the config, so a
selection of the removed font is not kept for later. That is deliberate: the
alternative left the overlay drawn in ImGui's built-in face instead of a font this
project ships. No save file is touched — this is a settings key, not save data.

---

## CI and quality gates (for contributors)

The two gates that once **could not fail** — the more dangerous state, because
they read as coverage — are armed:
[#375](https://github.com/penelopeduncan/redshipblueship/issues/375)
(`check-symbol-collisions.sh` had no committed baseline) and
[#376](https://github.com/penelopeduncan/redshipblueship/issues/376) (orphaned
CTest labels, missing `--no-tests=error`, an unfailable `check-archives`, a
frame-budgeted watchdog; PRs [#409](https://github.com/penelopeduncan/redshipblueship/pull/409),
[#437](https://github.com/penelopeduncan/redshipblueship/pull/437)). Both are closed.

Duplicate-symbol bugs are only observable on Linux (Windows links with
`/FORCE:MULTIPLE`); `.github/workflows/link-check.yml` gives every push to a
`claude/**` branch a fast Linux link plus the three nm gates
([#387](https://github.com/penelopeduncan/redshipblueship/issues/387), PR
[#548](https://github.com/penelopeduncan/redshipblueship/pull/548)).

Two rows to know about before you read a red or green as a signal:

- **`build-windows` runs the `redship`, `rando` and `ui` tiers** ([#623](https://github.com/penelopeduncan/redshipblueship/issues/623), PR [#642](https://github.com/penelopeduncan/redshipblueship/pull/642);
  [#709](https://github.com/penelopeduncan/redshipblueship/issues/709), PR [#723](https://github.com/penelopeduncan/redshipblueship/pull/723)). The `rando` tier now takes 30 to 45 minutes on the Windows
  runner and is most of that job's time ([#1692](https://github.com/penelopeduncan/redshipblueship/issues/1692) is the plan to run it in parallel).
  The first Windows run after a runner-image or compiler change rebuilds the sccache from zero and
  takes far longer. That is a cold cache, not a regression.
- **Required checks are `build-linux`, `build-windows` and `clang-format`.** The macOS build runs on pull requests
  and on `main` and reports as `build-macos`, but it is not required: a red or slow macOS build does not block a
  merge. Still read a failed one.
- **`IntSwitchOoTHmsToMm` is a real signal now.** Until
  [#544](https://github.com/penelopeduncan/redshipblueship/issues/544) it waited on
  the file-select hook, which needs a Start press the harness never sends, so it
  timed out at 120 s every time without running one assertion. It now injects a
  debug save from the title screen (as `IntGameplayRoundtrip` does), fires the
  Happy Mask Shop entrance after 20 live gameplay frames outside the shop, and
  asserts the switch routes to MM `0xD800`. When an OoT stage before the trigger
  stalls while OoT keeps running frames (the #544 state), the row fails after
  30 s in that stage with a line naming it ("title screen / file select never
  presented", "gameplay never reached", ...) instead of hitting the CTest wall.
  ~~A wedge inside a single frame, or in the OoT-to-MM hand-off and MM half after
  the trigger, still ends at the 120 s CTest timeout: the budget is checked once
  per OoT frame, not from a watchdog thread.~~ RESOLVED
  ([#793](https://github.com/penelopeduncan/redshipblueship/issues/793)): a
  wall-clock watchdog thread now ends every `int-*` run that goes 60 s with no
  frame completing, with an `[INT-WATCHDOG] FAIL` line naming the last stage
  (an OoT frame, an MM frame, the hand-off) and each game's state, and exit
  code 3 (`RSBS_INT_WATCHDOG_SECS` changes the budget; 0 disables it, e.g. under a
  debugger). Either way a red run of this row is a regression to read, not
  known noise.
- **A game controller cannot disturb the rows** (PR [#1153](https://github.com/penelopeduncan/redshipblueship/pull/1153), [#1152](https://github.com/penelopeduncan/redshipblueship/issues/1152)). libultraship lets SDL deliver joystick events to a window without focus, and test windows never take focus, so until #1153 a pad in use while a tier ran walked Link and pressed buttons in the test windows: the integration rows failed with a camera that would not follow, or a warp or boot at the wrong entrance, at a different point each run (the red of cycle 22 under "Gates, 2026-10-23, `main` after `v0.3.0-prealpha`"). Every `--test` and `--integration-test` process now turns those background events off and opens no controller, every CTest row also carries `SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS=0`, and each `[FOCUS]` line ends `controllerBackground=0 openControllers=0`; any other value fails an integration run. `TestControllersIgnored` locks it. You no longer need to leave the pad alone, or unplug it, while the tiers run.
- **Three rows cross with a paired file:** `IntPairedFirstCrossing` (PR
  [#790](https://github.com/penelopeduncan/redshipblueship/pull/790)),
  `IntPairedFirstCrossingDiverged` (changed settings, the load's restore, then a real MM
  boot; PR [#833](https://github.com/penelopeduncan/redshipblueship/pull/833)) and
  `IntPairedFirstCrossingTwoCycles` (a second arrival in Termina; PR
  [#891](https://github.com/penelopeduncan/redshipblueship/pull/891)). Like every
  `integration` row they need the ROM archives, so hosted CI never runs them; run them locally
  before merging anything that touches creation, the `.redsave` load, or the MM arrival.

Also: ~~clang-format is enforced against an incremental allowlist
(`.github/clang-format-paths.txt`), not the full tree~~ RESOLVED
([#369](https://github.com/penelopeduncan/redshipblueship/issues/369)): `src/common/` and
`rsbs/` are enforced whole. The vendored game trees stay on the file-by-file allowlist by
design: a vendored file is never reformatted just to enrol it.

---

## Tracking

- [#492](https://github.com/penelopeduncan/redshipblueship/issues/492) — Phase 3.1
  tracker (two-way combo randomizer; closed 2026-11-02, every child closed)
- [#500](https://github.com/penelopeduncan/redshipblueship/issues/500) — Phase 3.2
  tracker (cross-game logic and beatability), closed on 2026-11-03, with its increment epics
  [#644](https://github.com/penelopeduncan/redshipblueship/issues/644) (closed 2026-09-21) and
  [#645](https://github.com/penelopeduncan/redshipblueship/issues/645) (closed 2026-11-03)
- [#564](https://github.com/penelopeduncan/redshipblueship/issues/564) — the one-game
  semantics audit (closed 2026-11-02; its last row, V15, moved to
  [#574](https://github.com/penelopeduncan/redshipblueship/issues/574))
- [#577](https://github.com/penelopeduncan/redshipblueship/issues/577) — cross-game
  models (every child landed; closed 2026-11-02)
- Epics closed by the 2026-09-30 wave: [#458](https://github.com/penelopeduncan/redshipblueship/issues/458)
  (trackers), [#575](https://github.com/penelopeduncan/redshipblueship/issues/575) (hints),
  [#800](https://github.com/penelopeduncan/redshipblueship/issues/800) (shops),
  [#836](https://github.com/penelopeduncan/redshipblueship/issues/836) (a refused file is never
  opened unpaired), [#837](https://github.com/penelopeduncan/redshipblueship/issues/837) (every
  crossing is a save), [#879](https://github.com/penelopeduncan/redshipblueship/issues/879)
  (MM's paired start)
- [#310](https://github.com/penelopeduncan/redshipblueship/issues/310) — manual
  runtime QA pass (closed 2026-11-02 as not planned: the integration tier, the playtests and the release
  AppImages cover it; a sanitizer pass and an AppImage smoke test on real hardware were not carried forward);
  findings are appended to this document rather than tracked separately
- Closed trackers, for history: [#321](https://github.com/penelopeduncan/redshipblueship/issues/321)
  (pre-alpha v0.1.0 readiness), [#381](https://github.com/penelopeduncan/redshipblueship/issues/381)
  (Phase 2 review follow-ups), [#392](https://github.com/penelopeduncan/redshipblueship/issues/392)
  (Phase 3.0)
- Full open list: `gh issue list -R penelopeduncan/redshipblueship`
