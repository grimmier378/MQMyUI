# MQMyUI

A native C++ MacroQuest plugin converting the **MyUI** Lua UI bundle into a single
self-contained DLL with a modular, class-per-module architecture and peer-to-peer
Actor support for sharing live character data between boxed characters.

## Getting Started

```txt
/plugin MQMyUI
```

### Commands

```txt
/myui                  Show command help
/myui list             List modules and their visibility
/myui <module>         Toggle a module window (e.g. /myui Player)
/myui show <module>    Show a module window
/myui hide <module>    Hide a module window
```

### Configuration

Settings are stored in a single shared SQLite database at
`config/MQMyUI/MQMyUI.db`, keyed by `(server, character, module, name)` with a
typed value (`string` / `number` / `boolean`). This lets one character's config be
copied to another in-game. Named ImGui themes are stored in the `themes` table.

## Architecture

```
MQMyUI.cpp        Plugin entry, callbacks, /myui command, owns the managers
Core/
  ModuleBase.h    Abstract module: OnInit/OnPulse/OnRenderGUI/OnShutdown/OnZone
  ModuleManager   Registry + lifecycle driver
  SettingsStore   SQLite-backed typed settings + theme rows + char->char copy
  ThemeManager    DB-backed ImGui themes (push/pop), replaces hardcoded themes
  CharData        Per-pulse local TLO snapshot (modules read this, never poll TLOs)
  ChatBridge      Routes output to MQMyChat (${MyChat.Send[...]}) or MQ console
  ColorUtil.h     Progress-bar + color-lerp helpers
  ActorManager    (Phase 3) peer-to-peer protobuf data sharing
Modules/          One focused .h/.cpp per UI window
Proto/            (Phase 3) myui.proto peer messages
```

### Design notes (vs the old MQGrimGUI attempt)

- Modular (no 4,000-line monolith); each module owns its own settings.
- SQLite is the single source of truth for settings (no duplicated metadata vectors).
- Themes are DB-backed and editable in-game (ThemeZ), not hardcoded.

## Status

- **Phase 1 (done):** Framework, ModuleManager, SQLite SettingsStore, ThemeManager
  (Default), CharData, ChatBridge, ColorUtil, `/myui` command, Player+Target window,
  status-effects HUD (ported from MQGrimGUI). Links `sqlite3.lib`.
- **Phase 2 (done):** IconHelper, SpellPicker, and the Buffs, Songs, Pet, Casting, and
  Spells (gem bar with click-to-cast and right-click memorize) modules. CharData now
  snapshots buffs/songs.
- **Phase 3:** Actors + protobuf, Group, AAParty.
- **Phase 4:** ThemeZ editor + settings copy.
- **Phase 5:** Inventory (BigBag/MyInventory/iTrack).
- **Phase 6:** MyAA browser.

Out of scope (separate plugins or dropped): MyChat, MyDPS, MyPaths, DialogDB,
XPTrack, AlertMaster, MyDots, RaidWatch, ChatRelay, SillySounds, Clock, MapButton.

## Build

Built in Visual Studio as part of the MacroQuest solution. Requires `sqlite3.lib`
(already in the MQ tree). Phase 3 adds protobuf compilation via the MQ
`ProtocolBuffer` MSBuild item.

## Authors

* **Grimmier** - *Initial work* (Lua MyUI bundle and conversion)
