# OpenRCT2 Big-Endian (ppc64) Port — Development Notes

## Context

Porting OpenRCT2 to **big-endian powerpc64** (ELFv2, 64kb pagesize). The codebase assumes **little-endian** everywhere via a `static_assert` in `src/openrct2/platform/Platform.h:37`.

Reference fork (already partially working): https://github.com/Link4Electronics/openrct2BE (branch `big_endian_compat`)

## Strategy

| Format | Endianness | Swap macro | Rationale |
|--------|-----------|------------|-----------|
| RCT1/2 legacy files (SV6, SC6, SV4, SC4, TD6, DAT objects, G1.DAT) | **Little-endian** | `SWAP_IF_BE(x)` | Swap only on BE host |
| Network packets (multiplayer) | **Big-endian** (network byte order) | `SWAP_IF_LE(x)` | Swap only on LE host |
| `.park` files (OrcaStream) | **Little-endian** | `SWAP_IF_BE(x)` | Written as host-native, swapped per-field |

## Macro Definitions (`src/openrct2/core/Endianness.h`)

```cpp
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#    define RCT2_BIG_ENDIAN 1
#else
#    define RCT2_BIG_ENDIAN 0
#endif

#if RCT2_BIG_ENDIAN
#    define SWAP_IF_BE(x) ByteSwapBE(x)
#    define SWAP_IF_LE(x) (x)
#else
#    define SWAP_IF_BE(x) (x)
#    define SWAP_IF_LE(x) ByteSwapBE(x)
#endif
```

## Status Overview

### ✅ Completed Changes

| # | Change | Files |
|---|--------|-------|
| 1 | `ByteSwapBE` uses `__builtin_bswap16/32/64` | `Endianness.h` |
| 2 | Big-endian `static_assert` guarded | `Platform.h` |
| 3 | `DataSerialiserTraits`: `ByteSwapBE` → `SWAP_IF_LE` | `DataSerialiserTraits.h` |
| 4 | Network packets: `SWAP_IF_LE` | `NetworkPacket.h` |
| 5 | Network connection header: `SWAP_IF_LE` | `NetworkConnection.cpp` |
| 6 | No change needed (already handles BE) | `Socket.h` |
| 7 | OrcaStream header/chunk swapping, `compression` field fix | `OrcaStream.hpp` |
| 8 | Park file chunk swapping (scenario, object list) | `ParkFile.cpp` |
| 9 | G1.DAT header + element field swapping | `Drawing.Sprite.cpp` |
| 10 | Image table header + element swapping | `ImageTable.cpp` |
| 11 | Legacy object `ReadLegacy()` multi-byte swaps | All `object/*.cpp` |
| 12 | Object entry `flags`/`checksum` swaps on read/write | `ObjectFactory.cpp`, `ObjectRepository.cpp` |
| 13 | S6 swap functions (header, info, data) | `S6Importer.cpp` |
| 14 | S4 swap functions + tile element swaps | `S4Importer.cpp` |
| 15 | Union reordering (peep seat/car, vehicle track/swing/spin) | `RCT2.h`, `RCT1.h`, `Vehicle.h`, `Peep.h` |
| 16 | Endian-safe `DetectFileType`, `ValidateTrackChecksum`, `DecodeSC4` | `SawyerCoding.cpp` |
| 17 | Discord RPC disabled by default, GCC warning suppressions | `CMakeLists.txt` |
| 18 | Header fixes (GCC 14+ compat) | Various |
| 19 | Audio format tags: CSS=`AUDIO_S16SYS`, FLAC=`AUDIO_S16SYS`, OGG=`AUDIO_S16SYS` | `SDLAudioSource.cpp`, `FlacAudioSource.cpp`, `OggAudioSource.cpp` |
| 20 | RLE line offset reader fix (`memcpy` → LE construction) | `Drawing.Sprite.RLE.cpp`, `Viewport.cpp` |
| 21 | RLE line offset writer fix (swap to LE before write) | `SpriteFile.cpp` |
| 22 | SEA checksum fix | `SeaDecrypt.cpp` |
| 23 | FNV1a checksum endianness fix (park file validation) | `Crypt.OpenRCT2.cpp` |
| 24 | Widget union `text`/`content` separated (label/button text) | `Widget.h` (interface + ui), `Widget.cpp`, `CustomWindow.cpp`, `TileInspector.cpp`, `News.cpp` |
| 25 | `RCT12xy8` struct reordered for BE | `RCT12.h` |
| 26 | Comprehensive ride array element swaps (all stations/vehicles/customers), missing ride field swaps (overallView, stationStarts, entrances, exits, ratings, etc.) | `S4Importer.cpp` |
| 27 | Base entity field swaps (x/y/z, linked list pointers, sprite bounds) | `S4Importer.cpp` |

### 🐛 Known Remaining Issues

| Issue | Status | Symptoms |
|-------|--------|----------|
| RCT1 scenario loads Icicle Worlds instead of Forest Frontiers | **UNRESOLVED** | Selecting Forest Frontiers from the scenario list shows Icicle Worlds terrain (all snow/ice, water everywhere), $0 money, can't open park |
| AA/LL scenarios missing from selection list | **UNRESOLVED** | Corkscrew Follies and Loopy Landscapes scenarios absent — only base RCT1 maps visible |
| RCT2 empty ride list | **UNRESOLVED** | No rides available for placement on RCT2 maps — separate from S4 import issues |
| BSOD text missing on title screen | **UNRESOLVED** | ":( YOUR COMPUTER RAN INTO A PROBLEM" text not visible |
| CSG loading fails on BE | **UNRESOLVED** | `Unable to load csg graphics` at startup on BE, works on x86_64 |

### To Diagnose the Icicle Worlds Issue

The S4 decode path (`DecodeSC4`/`DecodeSV4`) produces byte-identical output on BE and LE (all operations are byte-level or use explicit LE construction). `swapS4()` now covers all known multi-byte fields in the S4 struct.

Possible root causes to investigate:
1. **Object loading failure** — if terrain surface objects fail to load or are mapped incorrectly, all tiles could render as ice/snow. Check whether `rct2.terrain_surface.grass` and `rct2.terrain_surface.ice` are both present and correctly resolved.
2. **Climate object mismatch** — `GetRequiredObjects()` loads a climate object based on `_s4.Climate` (single byte, no swap). Wrong climate could make grass render as snow.
3. **Wrong file decoded** — verify the exact file being decoded; the scenario repository might resolve the wrong path.
4. **S4 struct layout mismatch** — verify `sizeof(S4) == 0x1F850C` on BE; any deviation would shift all field offsets.
5. **Tile element corruption after swap** — `swapTileEl` only touches largeScenery entryIndex bytes 4-5; for non-largeScenery tiles no swap is applied, which is correct since all other tile fields are ≤ 1 byte.

To narrow this down, run on BE with stderr logging and check:
- Which `.sc4` file is actually being loaded (path from scenario repository)
- Whether `rct2.terrain_surface.grass` object loads successfully
- Whether `_s4.MapSize`, `_s4.Climate`, and `_s4.ParkFlags` have expected values after `swapS4`

## Key Design Decisions

- `SWAP_IF_BE` for on-disk LE formats (RCT1/2 saves, DAT objects, G1.DAT)
- `SWAP_IF_LE` for network byte order (BE wire format)
- `ByteSwapBE<T>` template handles enums, integers, and complex types via `std::bit_cast`
- Object cache (`objects.idx`) stores `FileIndexHeader` in native byte order (self-consistent per platform); item data uses `DataSerialiser` with `SWAP_IF_LE` (always BE on disk → cross-platform)
- Vehicle entity swapping done in-place on raw S6 entity array before `ImportEntity()`
- `SpriteFile::Save()` swaps RLE line offsets to LE before writing (generated files must be LE)
- `SpriteCombine.cpp` uses intentional double-swap (swap → modify → swap back) for native-endian output

## Testing Checklist

- [Yes] Game boots to title screen
- [ ] Load RCT2 .SV6 save
- [ ] Load RCT1 .SV4 save
- [ ] Save as .park (OrcaStream) and reload
- [ ] Multiplayer connect/disconnect
- [ ] All object types load correctly (scenery, rides, peeps)
- [ ] G1.DAT, G2.DAT graphics render correctly
- [ ] Peep pathfinding and behavior
- [ ] Replay system (demos)
- [ ] Track designer / track manager
- [ ] Title sequence parks load without "Checksum is not valid!" errors
- [ ] CSG graphics load (RCT1 path configured)
- [ ] BSOD text visible on title screen

## Recent Fix Details

### Widget Union `text`/`content` Endianness Bug (2026-06-27)

`Widget` struct had `StringId text` inside a union with `uint32_t content`. On BE, setting via `content` and reading via `text` (uint16_t) gave 0 due to byte-order mismatch. Fixed by removing `text` from the union and making it a separate field. `makeWidget()` sets both `out.content` and `out.text`.

### FNV1a Checksum (Crypt.OpenRCT2.cpp)

`OpenRCT2FNV1aAlgorithm::Update()` read memory as `uint64_t*` which gives different values on BE. Added `ReadLE64()` helper for consistent LE-byte-order reads. `Finish()` byte-swaps on BE.

### RLE Line Offset Duplicated Swap Bug (SpriteFile.cpp)

`SpriteFile::Save()` had **two** `#if RCT2_BIG_ENDIAN` blocks swapping RLE line offsets. The first block swapped base sprites, then the second swapped them **again**, reverting them back to native BE order. Fixed by removing the first block.

### SawyerCoding Endian-safe Checksums

`DetectFileType()`, `ValidateTrackChecksum()`, and `DecodeSC4()` used `reinterpret_cast<uint32_t*>` or raw `memcpy` to read 32-bit values from LE file bytes. Changed to explicit LE byte construction.

### Comprehensive S4 ride array element swaps and missing field fixes (2026-06-28)

`swapS4()` previously only swapped `[0]` elements of ride arrays (`lastPeepInQueue`, `time`, `length`, `vehicles`, `numCustomers`). Fixed to loop over all array elements. Added missing ride field swaps: `nameArgumentRide`, `nameArgumentNumber`, `overallView` (uses `xy` not individual bytes — `RCT12xy8` is reordered on BE), `stationStarts`, `entrances`, `exits`, `boatHireReturnPosition`, `curTestTrackLocation`, `chairliftBullwheelLocation`, `ratings` (excitement/intensity/nausea), `raceWinner`, `musicPosition`, `unk6`, `unkD0`, `unkD2`. Also added `RideMeasurement` swaps (`LastUseTick`, `NumItems`, `CurrentItem`). Added base `RCT12EntityBase` multi-byte field swaps (`x`/`y`/`z`, linked list pointers, sprite bounds) to `swapRCT12EntityBody()`.
