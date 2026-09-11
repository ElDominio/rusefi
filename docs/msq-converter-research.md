# MSQ Tune Converter - Feasibility Research

Status: research / not started. No code written yet.

## Goal

A standalone Windows tool (single `.exe`) that takes a tune saved under an older
rusEFI firmware build and produces an equivalent tune for a newer firmware
build, using only files a user already has:

1. An `.msq` tune file (usually `CurrentTune.msq`) from inside a TunerStudio
   Projects folder.
2. The matching `projectCfg/mainController.ini` from that same project
   (the ini the old tune was built against). If the user picked the `.msq`
   directly out of a `TunerStudioProjects/<project>/` folder, this can be
   located automatically; otherwise prompt for it.
3. A folder containing the target `rusefi.ini` (a normal firmware build
   output) to convert *to*.

Output: a new `.msq` valid against the new `rusefi.ini`.

## Why this isn't just "open old msq in new TS project"

TunerStudio already merges an `.msq` against a differently-versioned `.ini`
by field name when you open a tune under a project with a newer ini - fields
present in both are kept, fields missing from the new ini are dropped,
fields new to the ini fall back to firmware defaults. That's most of what
this tool needs to do. Two things stock TS does *not* do, which are the
actual value-add here:

- No standalone/scriptable entry point - it's a manual step inside the TS
  UI, per project, per user. Useful for support tooling / batch conversion /
  handing something to a non-technical user with instructions ("run this
  exe").
- No rename detection - if a field was renamed between firmware versions,
  TS sees "field disappeared" + "field appeared", not "field renamed", and
  silently drops the old value. Since our tool has *both* inis side by
  side, it can surface these as an explicit mapping step.

## `.msq` format findings

Source: `java_tools/msq-file/src/main/java/com/rusefi/tune/xml/{Msq,Constant,Page}.java`.

- `.msq` is XML. Each tunable value is a flat `<constant>` element:
  `name`, `units`, `value`, `digits`, `rows`, `cols` (rows/cols only present
  for array fields).
- Critically, `value` is **already the scaled, human-readable number** (or
  enum label text for bits/enum fields), not a raw byte/offset value. TS
  computes this from the field's `scale`/`translate` at write time
  (`ConfigurationImageGetterSetter.getStringValue`).
- Consequence: converting a tune is a **name-keyed value transplant of
  strings**, not a binary re-layout. We never need to know a field's byte
  `offset` or reconstruct a `ConfigurationImage`. This removes most of the
  complexity that a "recompile the binary tune" approach would need.
- Existing merge logic to mirror conceptually (`Msq.asImage` /
  `Msq.applyOnto`): for each `<constant>` in the source, look up the same
  name in the target ini's field table; if absent, skip; if present, take
  the value. `applyOnto` explicitly documents this as "preferable ... it
  ... handles firmware-version differences gracefully" - i.e. this really
  is rusEFI's own sanctioned strategy for the general case, we're mainly
  productizing it standalone plus adding rename handling.

## `rusefi.ini` grammar subset actually needed

Verified against a real generated file:
`firmware/tunerstudio/generated/rusefi_protorico-econoline.ini`.

Two simplifications versus a full TS-ini parser:

- The **generated per-board `.ini` has no `@@if_flag@@`/macro-substitution
  layer left** - that's a build-time preprocessing step already resolved
  before this file is written. A converter only ever needs to read
  generated output files (`rusefi_<board>.ini`, or a project's
  `mainController.ini`, which is a copy of one), never the raw
  `rusefi_config.txt`/template sources. So no `@@...@@` handling needed.
- Only the `[Constants]` section matters. Everything else in the file
  (`[OutputChannels]`, `[CurveEditor]`, `[TableEditor]`,
  `[GaugeConfigurations]`, `[Menu]`, `[FrontPage]`, `[Datalog]`, ...) is
  UI/logging metadata, irrelevant to tune values.

Within `[Constants]`, three field-line shapes cover everything:

```
name = scalar, TYPE, offset, units, scale, translate, lo, hi, digits [, flags...]
name = array,  TYPE, offset, [dim] | [dimXdim], units, scale, translate, lo, hi, digits [, flags...]
name = bits,   TYPE, offset, [lowbit:hibit], $enum_list_or_literal_list
```

Plus macro definitions referenced by `$name`:

```
#define some_list = "Choice A", "Choice B", ...
```

and page separators (`page = N` ... next `page = N` or end of section) -
these are informational only; the `.msq`'s own `<page>` grouping is
authoritative for reconstructing output, so the parser doesn't need to
cross-check page membership.

What we need per field, and what we can ignore:

| Need | Ignore |
|---|---|
| field name (the key) | byte `offset` |
| category: scalar / array / bits | `scale` / `translate` (msq value is pre-scaled) |
| array shape (`rows`, `cols`) | `TYPE` (U08/U16/F32/etc - irrelevant once value is scaled text) |
| bits/enum resolved choice-string list (via `$macro` lookup) | `{...}` dynamic TS expressions (e.g. `{bitStringValue(...)}` for dynamic units/hi) - keep as opaque string, never evaluated |
| `lo`/`hi` range (sanity-check only) | `noMsqSave` fields entirely (never appear in a tune file - skip both sides) |

This is a small, self-contained tokenizer - no JVM, no dependency on
rusEFI's Java `configuration_definition` codegen tooling.

## Enum/bits value representation

`.msq` stores bits/enum values as the **label text** (e.g. `"PA5"`,
`"Ignition Coil 3"`), not an ordinal index. This is naturally more
version-tolerant than a binary copy would be: if the enum's underlying
ordinal changes between firmware versions (list reordered/extended) but
the label text is unchanged, name+label matching still works correctly
without any special-casing. Only a genuine label rename/removal needs
flagging.

## Field matching / rename strategy

No external, hand-maintained rename table needed - the tool always has
*both* ini files (old, from the project; new, from the user-supplied
folder) loaded simultaneously, so it can diff them directly:

1. Name present in both inis -> copy value from old msq straight through.
   Validate: for bits/enum, the label must still exist in the new choice
   list; for scalar/array, value should fall within the new `lo`/`hi` (not
   fatal, just a warning surfaced to the user).
2. Name only in old ini (and had a value in the old msq) -> orphaned.
3. Name only in new ini -> new field, left at firmware default (omit from
   output msq entirely, same as stock TS behavior - the firmware's own
   `applyDefaultsOrFixAfterBurn()` fills it in on first connect/burn).
4. Leftover orphaned-old / new-only sets -> present as a manual "did you
   mean" mapping step in the GUI, ranked by string similarity plus
   matching type/units/array-shape as a tie-breaker. User confirms or
   skips each one. This is the actual value-add over stock TunerStudio
   behavior.

## Packaging constraint -> implementation language decision

User requirement: ship as a single Windows `.exe` eventually (PyInstaller
or similar).

- A pure-Python implementation (stdlib `xml.etree`/`lxml` for `.msq`, a
  hand-rolled `[Constants]` tokenizer for `.ini`) packages cleanly with
  PyInstaller - no JVM dependency.
- Reusing rusEFI's existing Java tooling (`java_tools/msq-file`,
  `java_console`'s `com.opensr5.ini` model, `TsProjectCreator`/`TsHelper`)
  would avoid re-implementing the ini/msq parsing, but would require
  bundling a JRE inside (or alongside) the exe - heavier (~150MB+),
  redistribution/licensing overhead for the bundled OpenJDK build, and
  works against the "single exe" goal.
- **Decision: pure Python.** Trade-off accepted: the ini-grammar subset
  above has to be hand-maintained in Python and kept in sync by hand if
  rusEFI's generated-ini `[Constants]` line grammar ever changes shape
  (unlikely - it's stable/simple relative to the full template/dialog ini
  syntax).

## TunerStudio project folder conventions (for the "browse for .msq" step)

Source: `java_console/io/src/main/java/com/rusefi/ts/TsProjectCreator.java`
(`TsHelper.MAIN_CONTROLLER_PATH`, `TsHelper.CURRENT_TUNE_MSQ`).

- Project root: `~/TunerStudioProjects/<projectName>/`
- Ini: `<project>/projectCfg/mainController.ini`
- Tune: `<project>/CurrentTune.msq` (also per-tune saves under
  `<project>/TuneView/` and backups under `<project>/restorePoints/` - not
  needed for v1, but the same folder-walk logic could offer them later)

Given a user-picked `.msq` path, the matching ini is simply
`<msq's project root>/projectCfg/mainController.ini`; fall back to a file
prompt if it's missing (e.g. user copied just the `.msq` out of the
project).

## Open questions / not yet resolved

- Exact list of TS value-formatting edge cases in `digits`/rounding when
  re-emitting a value under the new field's `digits` setting (probably
  fine to just carry the string through unmodified and let TS reformat on
  next save - needs a quick empirical check by round-tripping a real msq
  through TunerStudio).
- Whether to attempt any array-shape reconciliation (e.g. VE table resized
  between firmware versions) beyond "shape changed -> treat as orphaned,
  offer manual remap" for v1.
- GUI toolkit choice (Tk ships with CPython -> simplest PyInstaller story;
  PySide/Qt nicer UI but bigger exe) - not decided.

## Next step

Scaffold: ini `[Constants]` parser, msq reader/writer, and a first-pass
GUI shell (pending toolkit decision above).
