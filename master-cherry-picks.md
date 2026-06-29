# Master Cherry-Pick Tracker

Commits from `master` worth bringing into `first-order-rpm`.
Grouped by category; ordered oldest-to-newest within each group (cherry-pick order).

---

## Done

| Hash | Description |
|------|-------------|
| `2b2a93bcbb` | sensors: add Bosch 3-bar TMAP MAP sensor preset |
| `17c99f2f87` | EnumGenerator: pad enum text with "INVALID" for unused bits on TS file |
| `ada6f7f8b4` | storage: internal: show debug for extra pages too |
| `12097e7b86` | internal flash: stm32f7: relax PVD settings a bit |
| `aad27a5522` | storage: flash: validate what was just written |
| `f01bf2a7a7` | storage flash: remove duplicated info from error messages |
| `f3b4c09f70` | simulator: implement intFlashCompare() |
| `a93f5fc81b` | storage: flash: critical error if flash failed due to undervoltage |
| `8b1d017d43` | storage: flash: always check if area is erased |
| `bcefacfe33` | cranking & priming: single flexCranking toggle drives 2D coolant x ethanol tables |
| `bd4d3281fe` | cranking flex: retire crankingFuelCoefE100, migrate legacy tunes via TuneMigrator |
| `afb6558708` | Move Lua to separate TS page (#9693) — Lua → page 5; AlphaX custom → page 6 |
| `84716fa9a4` | read lua script from correct page on fw/console combinations |
| `12346c64b9` | sd: default migration for conditional logging thresholds |
| `cd97c9fe3f` | fuel: flex fuel transient compensation (accel enrichment + wall wetting) |
| `cc8819408f` | fuel: fix flex transient TableEditor bins (add ethanol live channel) |
| `00e46bda7e` | fuel: migrate flex transient axis bins for older tunes |
| `8fe687b081` | fuel: fix STFT correction channel scaling (reads 99% at neutral) |
| `8b4f11ff2f` | IsoTp: allow custom padding byte |
| `90dc8fd5b4` | flex: add Flex sensor status indicator (Flex OK / Flex error) |
| `d5ee2d6ebf` | MSD: check LUN number |
| `f0ce74ad10` | MSD null device: do not fake media size if no media is inserted |
| `8ed8fc9285` | SD card access over USB is not reliable #9664 |
| `626e4c35a2` | USB: add IAD Descriptor for MSD interface |
| `3900feb9ac` | SD card access over USB is not reliable #9664 |
| `64b431ab56` | USB string descriptors: no tailing zero |
| `c635865b40` | ChibiOS: USB update |
| `067b9f787c` | USB more fixes |
| `25d47f7cd4` | LTFT: can be disabled at compile time |
| `6f5581ece5` | tle9201: do not overflow when generating name |
| `15efa7a85b` | GHA says update firmware/libfirmware submodule |
| `5e4285b88a` | CAN: send TS-over-CAN announce every 250mS |
| `cb2c80736c` | Ini generator needs to be smarter about missing pages |
| `1e37a289a8` | Fix logic to address missing fields in DefaultTuneMigrator (page 1 → page N moves) |
| `7658ae487b` | AlphaX-8chan: include elf file into bundle |
| `2c8f29e751` | EFI_HPFP |
| `99b6a8c781` | Renix 44-2-2 trigger #9713 |
| `33e374d288` | Trigger wheel definitions |
| `add2fe0915` | Renix 44-2-2 trigger #9713 |
| `206c05f030` | 9713: Renix 44-2-2 trigger ratios fix |
| `88e3cc074e` | 9713: Renix 44-2-2 trigger ratios tests |
| `919ccc6e61` | Trigger wheel definitions |

---

## Pending

*(none)*

---

## Skip

- `defd58463b` / `65ca4ef290` / `1044247c69` — misfire (branch has own page-6 impl)
- `cc13493aad` — starter clutch-down cranking (originated on this branch)
- All `only:uaefi`, `only:super-uaefi`, `only:magic` commits
- Console / UI cosmetic commits (`feat(ui):`, `refactor(ui):`, loading overlay, etc.)
- Auto-generated files commits
