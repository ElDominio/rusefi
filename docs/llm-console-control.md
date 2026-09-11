# LLM Control Tab for the rusEFI Console (design proposal)

Status: design only, not implemented. No code changes have been made against this
document - it captures a design conversation for future implementation.

## Motivation

`java_console/mcp_ecu` (`EcuMcpServer`) already lets an LLM agent connect to an ECU,
read/write Lua scripts, send commands, and read messages/tune data, speaking MCP
(JSON-RPC 2.0) over stdio. It is useful, but it is a *separate standalone process* with
its own `LinkManager` - it cannot attach to a console session a human already has open,
and if both tried to hold the same serial port at once they would conflict.

This document proposes folding equivalent capability directly into the Swing console
(`ConsoleUI`) as a new tab, with an explicit, staged, human-controlled permission model,
so a human operator can hand control to an LLM (e.g. an agent running in Claude Code)
against a *live* console session, with a full audit trail and an instant way to take
control back.

## Current architecture (for reference)

- `ConsoleUI` holds one `LinkManager` per running console process
  (`uiContext.getLinkManager()`), shared by every GUI panel.
- `LinkManager.submit(Runnable)` (`java_console/io/.../LinkManager.java`) already
  serializes *all* wire access onto one thread/executor - this is the choke point every
  GUI action goes through today, and the same one an in-process LLM bridge would reuse.
  No new locking primitive is needed for wire safety; GUI clicks and LLM-issued commands
  would simply interleave safely through the existing executor.
- `EcuMcpServer` duplicates this: it creates its *own* `LinkManager` in a separate JVM,
  which is the reason it cannot coexist with a live console session against the same
  port.
- Existing per-user data folder convention: `RUSEFI_SETTINGS_FOLDER = ~/.rusEFI/`
  (`java_console/shared_io/.../FileUtil.java`). This is the durable, launch-method-
  independent location referenced below for the audit log folder - not
  `FileLogger.DIR = "logs/"`, which is a relative path from wherever the process happens
  to be launched from.

## Proposed design

### No MCP dependency required

MCP is a tool-discovery/RPC convention for generic MCP clients (Claude Desktop, etc.).
An agent running inside Claude Code already has a shell tool and does not need that
layer - the console can expose a plain local API (loopback-only TCP or a Unix domain
socket, line-delimited JSON) and the agent talks to it directly. This also removes the
two-process/two-`LinkManager` problem: the bridge lives inside the console process and
reuses `uiContext.getLinkManager()`, so there is only ever one connection to the ECU.
(A future MCP-compatible facade in front of the same bridge is possible later if wider
MCP-client interop is wanted, but it is not a prerequisite.)

Bind loopback-only (`127.0.0.1`), never the network - this is live engine control, not a
convenience feature to expose on the LAN.

The bridge must run on its own thread/executor, separate from the Swing EDT, feeding
into `LinkManager.submit()` like every other caller. A bug or hang in bridge code must
not be able to freeze the console UI.

### New "LLM" tab, three-stage gate

A new top-level tab next to the existing device tabs (structurally comparable to
`SlcanTab` - a peripheral-protocol tab with its own connect/status state), holding three
independent, escalating toggles:

1. **Enable LLM communication (read-only)** - `connect`, `ecu_info`,
   `read_output_channel`, `read_messages`, `wait_for_message`, `read_tune`. No write
   capability of any kind.
2. **Enable LLM write for Lua** - adds `set_lua`, `get_lua`, `lua_reset`. Scoped
   specifically to the Lua script field; no other write path.
3. **Enable LLM write for everything** - adds `send_command` (arbitrary text to the
   command queue - inherently ungateable any finer than "trust it"), `reboot`,
   `reboot_to_blt`, and any future write-capable tools (e.g. tune/calibration writes -
   note none exist yet; today only `read_tune` exists, so stage 3 as "everything" is
   somewhat aspirational until those tools are built).

Each stage requires the human to have explicitly enabled every stage below it first -
the three-stage climb *is* the informed-consent chain. Deliberately **no automatic
RPM/engine-running interlock** on top of stage 3: once a human has explicitly walked
through all three stages, that is accepted as sufficient authorization by design; an
automatic override would be redundant on top of an explicit chain of consent the
operator already climbed.

### Kill switch

A single, separate "disarm everything now" action - not "step back down through the
stages one at a time." Drops straight to fully off regardless of current stage. Exists
independently of the staged enable flow, for the moment something looks wrong mid-
session.

### Session lifecycle

All three stages reset to off on every console launch. Nothing persists across restarts
- an operator must re-arm explicitly each session. This is a deliberate choice: a stale
"stage 3 was armed last time" surviving a restart is exactly the failure mode this
design exists to prevent.

### Live status indicator

Whenever any stage is armed, the console shows a persistent, visible indicator (not just
a log entry) - e.g. in the tab itself and/or a status-bar element - so anyone glancing at
the screen can tell control could be non-local right now, independent of anyone actually
reading the audit log.

### Audit logging

- Folder: `~/.rusEFI/LLM_logs/`, created lazily the first time any stage toggle is first
  flipped (not created on console launch unconditionally).
- One file per session (a session = one arm-to-disarm/console-lifetime span, consistent
  with "every launch resets"), e.g. `session_2026-09-08_14-30-12.log`.
- Logged content: every command/tool call and its result, each with a timestamp and the
  stage active at the time; and, just as importantly, the stage transitions themselves
  (armed stage 2 at 14:31, dropped to stage 1 at 14:47, kill switch hit at 14:52) - the
  toggle history is part of the audit trail, not just the commands.
- No auto-retention/deletion policy - this is a safety log, not a cache; pruning is left
  to the human.

## Explicitly rejected

- **Automatic RPM/engine-running write interlock on top of stage 3.** Considered and
  rejected: the three-stage explicit-consent chain is treated as sufficient
  authorization once climbed; see "New LLM tab, three-stage gate" above. A lighter-
  weight alternative that was *not* rejected but also not committed to: flagging (not
  blocking) stage-3 commands sent while RPM > 0 more prominently in the log, for
  after-the-fact review - open for a future iteration if wanted.

## Open follow-ups (not yet decided / not yet built)

- Stage 3 "everything" currently only actually reaches `send_command`/`reboot`/
  `reboot_to_blt` from the existing tool surface - true tune/calibration write tools
  (e.g. writing individual calibration constants, burning a full tune) do not exist yet
  in `EcuMcpServer` and would need to be designed and gated the same way if/when built.
- Exact wire format of the local bridge protocol (request/response envelope, whether it
  mirrors `EcuMcpServer`'s existing `tools/call` JSON shape for reuse, or is simpler)
  is unspecified - implementation detail for the build step, not decided here.
- Whether `set_lua`'s current single atomic "write + burn + luareset" tool call should
  be split into a stage + human-confirmed apply step (console shows the diff, operator
  clicks burn) was raised as a possible extra safety layer but not decided either way.
