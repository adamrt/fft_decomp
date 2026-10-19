# Archaeology

What the North American PlayStation release of *Final Fantasy Tactics* (`SCUS-94221`) reveals about its original construction. These findings come from the retail binaries, their credits and development paths, and compilation that reproduces their bytes. Original source filenames and build scripts have not been recovered.

## Programs and memory

The game keeps a resident executable in memory and loads other programs into fixed, overlapping regions as needed.

| Program | Load address | Role |
|---|---|---|
| `SCUS_942.21` | `0x80010000` | Resident libraries, sound driver, file loading, memory management and game-data services |
| `BATTLE.BIN` | `0x80067000` | Battles, scripted scenes, map rendering, calculations, AI and the effect runtime |
| `WORLD/WLDCORE.BIN` | `0x80067000` | World map, travel, bar, propositions and Brave Story |
| `WORLD/WORLD.BIN` | `0x800e0000` | Formation, shops and other menus, plus shared event and menu code |
| `OPEN/OPEN.BIN` | `0x80067000` | Opening, title sequence and ending |
| `EVENT/*.OUT` | Three addresses from `0x801bf000` | Eleven menu and support overlays |
| Code-bearing `EFFECT/E###.BIN` | `0x801c2500` | Native animation callbacks and their effect data |

`SCUS_942.21` has a 2 KB PS-X EXE header; its payload loads at `0x80010000` and execution begins at `0x80010a30`. The other programs are raw images. BATTLE overlaps both world programs; WLDCORE and WORLD occupy separate regions and run together. Eight EVENT overlays load at `0x801bf000`, SMALL at `0x801d7000`, and HELPMENU and JOBSTTS at `0x801df000`, allowing selected overlays to coexist. The addresses and file identities are recorded in [target/](target/).

An overlay need not have one main function. OPEN exposes separate title and ending runners. EQUIP and JOBSTTS expose functions called once per frame, whereas BUNIT runs a complete screen loop. Effect code is selected through a resident callback catalog and script instructions, with initialization, update and destruction phases. The first function in a file is often a helper rather than its external entry.

Even the ending crosses this split: OPEN plays the movie and credits, then MAIN reloads BATTLE, whose scenario loader enters Delita and Ovelia's epilogue.

The EFFECT directory contains 512 numbered files: 111 are empty, 110 contain native code before their section tables, and 291 begin directly with data section tables. Many effects therefore use data alone; the code-bearing files reuse a small collection of animation routines. All 512 directory records carry the date October 17, 1997.

## Compiler fingerprints

Two toolchain families reproduce the original instruction sequences:

| Programs | Compiler | Assembler behavior | Usual optimization |
|---|---|---|---|
| Resident executable, BATTLE, WORLD, EVENT and EFFECT | GCC 2.6.3 | ASPSX 2.34 | `-O2` |
| OPEN and WLDCORE | GCC 2.7.2 | ASPSX 2.21 | `-O2` |

The exceptions help distinguish separately compiled objects. Parts of WORLD's formation and menu code use `-O1`; its tutorial object and most of CARD use `-O0`. A few resident routines require older assembler behavior. Four Suzuki heap functions use the small-data setting `-G8`, while the rest of the reconstruction uses `-G0`. These profiles are recorded per program, unit or function in the target files.

Division traps are another object-level fingerprint. Shared event/menu routines can contain checked division sequences in WORLD or an EVENT overlay and unchecked sequences in BATTLE. This is an assembler expansion setting, not necessarily a source change. Likewise, some apparently different menu implementations become identical when compiled at the other program's optimization level.

GCC can reproduce some apparent global tables as constant pools for local initializers. The six action-result digit zoom curves reproduce the original stack copies when written as ordinary local arrays. Compiling the surrounding unit-presentation routines together also reproduces the following switch table's alignment; the initializer version built with the earlier file split placed that table four bytes too early. This constrains the reconstructed grouping without identifying an original filename.

OPEN and WLDCORE share both their toolchain profile and related window/file code. That supports a common build lineage; it does not identify a particular development machine or recover the original makefiles.

## Shared components and the original split

BATTLE's layout separates large regions for the battle core, map loading, event system, calculations, AI and effects, with data between their code regions. WORLD combines shared components with its own formation and shop code. The executable split therefore does not describe the whole source organization.

Two shared collections are especially visible:

- **Event code:** BATTLE and WORLD contain corresponding script, cooperative-thread, dialog, text, window and sound-queue routines in matching order. Some bodies differ by program settings or behavior; others are stubs where a feature is unused.
- **Menu code:** WORLD and the EVENT overlays reuse scrolling lists, selection records, graphics, item helpers, text and memory-card routines. Related functions appear in different orders, interleaved with each program's own code. This rules out one unchanged source file compiled in the same order everywhere; separate objects and edited source copies remain possible.

Reuse extends beyond those two collections. OPEN and WLDCORE share window-record and file helpers; OPEN also shares memory-card routines with WORLD and CARD. Shared code is compiled into each program, while calls to resident services use a single implementation already in memory.

Program settings leave readable traces. BATTLE has sixteen 1 KB thread slots; WORLD has seventeen, with tutorial text in the extra slot. Reset routines clear only the first sixteen, allowing that tutorial thread to survive screen changes. The menu work buffer has 53 pages of 256 bytes in BATTLE and 112 in WORLD. Shared menu interpreters also use different opcode-table limits and selection capacities. See [thread.h](include/fft/thread.h) and the target `defines` maps.

The sound system has a separate division. Suzuki's 222 functions occupy a contiguous resident block immediately before PsyQ's SPU library. FFT's music requests and sound wrappers occur later, outside that block. The sequencing engine, game-facing requests and hardware library are distinct layers. WORLD additionally links PsyQ graphics objects, and OPEN links movie decompression objects; those libraries are not all resident in the main executable.

## Traces of development

Paths such as `c:\fftimg\event\attack.out` and `C:\FFTIMG\MAP\GNS\` survive in file-load records. They reveal a PC-side staging tree named `FFTIMG`. EVENT's `.OUT` naming was already present there; it was not merely assigned when the disc was mastered. These are staging paths, not recovered C source paths.

Some text resources also survive as complete embedded files: `SAMPLE.LZW` in BATTLE and WORLD, `HELP.LZW` in HELPMENU and WORLD, `WLDHELP.LZW` in WORLD, and `JOIN.LZW` in REQUIRE. Despite the extension, their decoder uses LZ-style distance/length backreferences rather than an LZW dictionary. HELP and WLDHELP use those commands; SAMPLE and JOIN contain none.

BATTLE and WORLD also embed byte-identical copies of `EVENT/FONT.BIN`: 77,000 bytes for 2,200 glyphs, stored as 10×14 grids of two-bit pixels.

The credits distinguish **Taku Murata — Main Program**, **Hiroshi Ono — World Map Program**, **Masaaki Kubo — Shop & Organization Program**, and **Hidenori Suzuki — Sound Programmer**. Those named responsibilities fit the broad program split. The binaries do not establish who wrote every renderer, interpreter or shared object, so finer ownership remains uncertain. The credit blocks begin at file offsets `0xae65` and `0xb3fc` in `OPEN/OPEN.BIN`.

Some low-level routines have strong assembly fingerprints: cooperative context switches, stack-changing trampolines and several small accessors and copy loops. Register conventions, delay slots and trapping arithmetic distinguish them from the surrounding compiler output. These clues concern how the code was built; GTE or scratchpad use alone does not establish Sony library ownership.

There are also traces of imperfect integration. WORLD retains calls to BATTLE-only addresses even though the world programs replace that memory. DEBUGCHR contains both ordinary mounted-unit Status and a developer editor; its filename does not mean all its code was hidden from players. Details and other preserved behavior are recorded in [QUIRKS.md](QUIRKS.md).

## How much can be recovered

Compiler-profile changes, intervening assembly or library objects, jump-table alignment and noncontiguous read-only data constrain original object boundaries. Different function orders are useful clues, but cannot alone distinguish separately linked objects from edited source copies.

Matching bytes alone cannot recover a source-file boundary. Adjacent objects can sometimes be rebuilt as one file without changing their output. Conversely, identical instructions can operate on different record layouts. The evidence supports shared event/menu components and program-specific implementations, but not exact original filenames, directory names or every translation unit. Our source organization is a reconstruction of those constraints.
