![header](https://capsule-render.vercel.app/api?type=waving&height=300&color=gradient&text=ECleaner)

[简体中文](README_zh-CN.md)

## Introduction

ECleaner is a lightweight entity cleaner for Endstone. Starting with this fork's 0.2.0 line, the plugin uses a performance-first, high-frequency cleanup model:

- Dropped items and entities have independent schedules.
- Intervals are configured in seconds as pressure-check intervals. Defaults are 10 seconds for items and 60 seconds for entities; actual deletion only happens when the MSPT gate passes.
- Scheduled cleanup is silent by default; the old sound and 30-second warning are removed.
- Default blacklist entries target low-value terrain drops and common hostile mobs.
- Named entities are protected from automatic entity cleanup.
- Automatic cleanup is skipped while the server has no online players.
- Scheduled cleanup is gated by Spark MSPT pressure: by default it only runs when the 10-second p95 MSPT is at least 50 ms.
- MSPT is read from Spark through Endstone PAPI. If PAPI/Spark is unavailable or the MSPT value is unresolved, automatic cleanup fails closed and skips deletion.
- 0.3.1 adds an MSPT-aware Chunk Entity Guard with pressure limits plus unconditional hard safety caps for runaway mob reactors/farms.
- A shared high-value entity protection policy preserves villagers, pets, mounts, allays, shulkers, named mobs, and actors tagged `ecleaner_protect` by default. Protected mobs still count toward pressure.
- Falling-block protection defaults to non-destructive pressure monitoring; destructive spawn rejection requires explicit opt-in and can break gravity-block machines.
- 0.3.3 switches the main configuration to TOML. `config.json` is no longer read or migrated; a missing `config.toml` is generated directly from current defaults.
- 0.3.4 avoids repeated rate reductions when a dimension has no falling-block demand or is already within its allowance, and adds accepted/rejected spawn diagnostics.

## Installation

Place the plugin binary in the Endstone server's `plugins` directory. On first startup ECleaner creates:

```text
plugins/ecleaner/config.toml
```

Language files live under:

```text
plugins/ecleaner/language/
```

## Default configuration

```toml
language = "zh_CN"
auto_item_clean = true
auto_entity_clean = true
item_clean_interval_seconds = 10
entity_clean_interval_seconds = 60
broadcast_cleanup_results = false
mspt_threshold = 50.0
mspt_window = "10s"
mspt_statistic = "p95"

item_clean_whitelist = false
item_clean_ids = [
    "minecraft:netherrack",
    "minecraft:cobblestone",
    "minecraft:cobbled_deepslate",
    "minecraft:stone",
    "minecraft:deepslate",
    "minecraft:dirt",
    "minecraft:grass_block",
    "minecraft:gravel",
    "minecraft:tuff",
    "minecraft:granite",
    "minecraft:diorite",
    "minecraft:andesite",
    "minecraft:calcite",
    "minecraft:basalt",
    "minecraft:blackstone",
    "minecraft:end_stone",
    "minecraft:sandstone",
    "minecraft:red_sandstone",
]
item_clean_legacy_names = []

entity_clean_whitelist = false
entity_clean_list = [
    "minecraft:zombie",
    "minecraft:skeleton",
    "minecraft:creeper",
    "minecraft:spider",
    "minecraft:cave_spider",
    "minecraft:husk",
    "minecraft:drowned",
    "minecraft:stray",
    "minecraft:bogged",
    "minecraft:witch",
    "minecraft:slime",
    "minecraft:magma_cube",
    "minecraft:zombie_pigman",
    "minecraft:zombified_piglin",
    "minecraft:phantom",
]

[entity_protection]
enabled = true
protect_named = true
protect_tag = "ecleaner_protect"
types = [
    "minecraft:villager",
    "minecraft:villager_v2",
    "minecraft:zombie_villager",
    "minecraft:zombie_villager_v2",
    "minecraft:allay",
    "minecraft:horse",
    "minecraft:donkey",
    "minecraft:mule",
    "minecraft:camel",
    "minecraft:llama",
    "minecraft:trader_llama",
    "minecraft:wolf",
    "minecraft:cat",
    "minecraft:parrot",
    "minecraft:sniffer",
    "minecraft:iron_golem",
    "minecraft:snow_golem",
    "minecraft:shulker",
]

[chunk_entity_guard]
enabled = true
reconcile_interval_ticks = 20
pressure_mspt_threshold = 50.0
cleanable_types = [
    "minecraft:zombie",
    "minecraft:skeleton",
    "minecraft:creeper",
    "minecraft:spider",
    "minecraft:cave_spider",
    "minecraft:husk",
    "minecraft:drowned",
    "minecraft:stray",
    "minecraft:bogged",
    "minecraft:witch",
    "minecraft:slime",
    "minecraft:magma_cube",
    "minecraft:silverfish",
    "minecraft:endermite",
    "minecraft:zombie_pigman",
    "minecraft:zombified_piglin",
    "minecraft:phantom",
]
pressure_cleanable_mob_limit_per_chunk = 192
hard_cleanable_mob_limit_per_chunk = 384
pressure_total_mob_limit_per_chunk = 320
hard_total_mob_limit_per_chunk = 512
pressure_cleanable_mob_limit_3x3 = 512
hard_cleanable_mob_limit_3x3 = 768
emergency_delete_protected_mobs = false
log_triggers = true
log_cooldown_seconds = 10

[chunk_entity_guard.pressure_type_limits]
"minecraft:slime" = 96
"minecraft:silverfish" = 128
"minecraft:magma_cube" = 96

[chunk_entity_guard.hard_type_limits]
"minecraft:slime" = 256
"minecraft:silverfish" = 256
"minecraft:magma_cube" = 256

[falling_block_guard]
enabled = true
mode = "monitor"
control_interval_ticks = 20
pressure_mspt_threshold = 50.0
recovery_mspt_threshold = 45.0
backoff_factor = 0.5
recovery_factor = 2.0
recovery_stable_intervals = 5
min_rate_per_second = 16.0
activation_rate_per_second = 16.0
burst_capacity = 32.0
log_adjustments = true
```

### Options

`auto_item_clean`: Enables scheduled dropped-item cleanup.

`auto_entity_clean`: Enables scheduled entity cleanup.

`item_clean_interval_seconds`: Item cleanup pressure-check interval in seconds. By default ECleaner checks MSPT every 10 seconds and only deletes items when the threshold is met. Set to `0` to disable scheduled item cleanup.

`entity_clean_interval_seconds`: Entity cleanup pressure-check interval in seconds. By default ECleaner checks MSPT every 60 seconds and only deletes entities when the threshold is met. Set to `0` to disable scheduled entity cleanup.

`broadcast_cleanup_results`: Broadcast scheduled cleanup counts to all players. Defaults to `false`.

`mspt_threshold`: Automatic-cleanup pressure threshold in milliseconds. Defaults to `50.0`. Scheduled cleanup only runs when the selected Spark MSPT statistic is at or above this value. Set it to `0` to disable the MSPT gate and restore unconditional interval-based cleanup.

`mspt_window`: Spark MSPT window, either `10s` or `1m`. Defaults to `10s`.

`mspt_statistic`: Which value from Spark's `{spark:tickduration_*}` distribution to compare: `min`, `median`, `p95`, or `max`. Defaults to `p95`, so the default policy is “10-second p95 MSPT >= 50 ms”.

`item_clean_whitelist`: When `false`, only items in `item_clean_list` are removed. When `true`, listed items are preserved and other dropped items are removed.

`item_clean_ids`: Uses stable ItemType IDs such as `minecraft:netherrack`; matching no longer depends on an English display name or client language.

`item_clean_legacy_names`: Compatibility-only fallback for legacy custom English names that cannot be mapped to an ItemType ID. New configs normally keep this empty.

`entity_clean_whitelist`: When `false`, only entities in `entity_clean_list` are removed. When `true`, listed entities are preserved and other unnamed entities are removed.

`entity_clean_list`: Uses entity IDs such as `minecraft:zombie`.

### High-value entity protection

`entity_protection.enabled` enables the shared protection policy used by both MSPT entity cleanup and the Chunk Entity Guard. Protected types, named mobs (`protect_named`), and actors carrying the configured `protect_tag` (`ecleaner_protect` by default) are preserved. Protected mobs still count toward density/pressure.

### Chunk Entity Guard

The guard has two tiers. It reads the same Spark MSPT statistic selected by the root `mspt_window` / `mspt_statistic` settings, but uses its own configurable `pressure_mspt_threshold` (default `50.0` ms).

Pressure limits are enforced only while MSPT is at or above that threshold. The observation defaults are deliberately permissive: 96 slimes, 128 silverfish, or 96 magma cubes per chunk; 192 cleanable mobs per chunk; 320 total mobs per chunk; and 512 cleanable mobs in a 3x3 area.

Hard limits do not depend on Spark/PAPI and remain active even when MSPT is healthy or unavailable: 256 for each configured high-risk type, 384 cleanable mobs per chunk, 512 total mobs per chunk, and 768 cleanable mobs per 3x3 area. This keeps healthy technical farms unrestricted below the hard safety envelope while retaining a last-resort fuse.

`reconcile_interval_ticks` defaults to 20 ticks. ActorSpawnEvent/ActorRemoveEvent maintain a fast-path counter; hard-limit crossings, and pressure-limit crossings while pressure is active, request a next-tick reconciliation. MSPT is sampled once per reconciliation rather than once per spawn.

If Spark/PAPI data is unavailable, pressure-tier cleanup fails closed and does not delete entities, but hard limits continue to protect the server. Set `pressure_mspt_threshold` to `0` to make the pressure tier unconditional.

`emergency_delete_protected_mobs` defaults to `false`. Protected mobs are counted toward density but are preserved by default; setting this option to `true` allows chunk-total pressure/hard fuses to remove them. Players are never removed.

Trigger logs identify whether the event was `pressure` or `hard` and include the sampled MSPT when available. `log_triggers` and `log_cooldown_seconds` only rate-limit repeated logs; they never pause enforcement.

### Falling Block Guard

`falling_block_guard.mode` now defaults to `"monitor"`, including configs from older versions that omit this setting. Monitor mode measures pressure and computes the same token-bucket budget, but **never cancels falling-block spawns**. Logs distinguish actual accepted/rejected counts from `over_budget` counts (spawns that would have been rejected). It does not provide an enforced falling-block resource limit. Turn off or slow an overloaded machine through its own controls.

This is intentional: Endstone's spawn cancellation despawns an actor that has already been added to the level. It is not a pause/retry or source-block rollback. Discarding a gravity block can remove a sand duper's circulating block, so even a positive minimum rate cannot guarantee continued machine operation. Monitoring cannot restore blocks already lost. Generic entity cleanup also preserves falling blocks in monitor mode, even with entity whitelist mode; the chunk guard continues to apply only to mobs.

Administrators who explicitly prefer destructive load shedding can set `mode = "reject"`. A warning is emitted on start/reload. This retains the previous behavior and can damage machines or ordinary falling-block constructions. There is no supported generic producer-pause API or spawn-cause information in the pinned Endstone API; ECleaner does not guess source blocks, rewrite pistons, or re-create canceled actors.

Both modes retain the per-dimension feedback controller: activate at the configured pressure MSPT and attempt rate, back off only when demand exceeds the allowance, refill a bounded token bucket, and recover after the configured low-MSPT streak. Missing MSPT introduces no new limit and holds the current calculated budget. Existing settings (`activation_rate_per_second`, `min_rate_per_second`, `burst_capacity`, backoff/recovery factors and intervals) remain valid. In monitor mode their budget is diagnostic, not a production-rate guarantee. Mob hard limits and other cleanup protections are unchanged.

Research and limitations:
- [Endstone v0.11.0 spawn hook](https://github.com/EndstoneMC/endstone/blob/v0.11.0/src/endstone/runtime/bedrock_hooks/script_level_gameplay_handler.cpp): cancellation invokes `despawn()` after `LevelAddedActorEvent`.
- [Endstone spawn-event API](https://github.com/EndstoneMC/endstone/blob/v0.11.0/include/endstone/event/actor/actor_spawn_event.h): no spawn cause or resumable producer contract.
- [Bedrock report MCPE-185621](https://bugs.mojang.com/browse/MCPE/issues/MCPE-185621): reports gravity-block machines moving blocks through portals/gateways with sticky pistons. This corroborates the state-sensitive mechanism, not this plugin's exact incident or all current machine designs.

No live BDS machine reproduction is claimed. Monitor mode prevents this plugin's destructive spawn cancellation; it is a safety mitigation, not lossless automatic machine throttling.

### Configuration publication and selection

Reload/form saves stage the complete core policy, deletion lookups, language resources and both guards before publishing on the main thread. Failed parsing, language validation, serialization or file replacement keeps the previous policy and runtime guard state. Missing language files retain the existing key-as-text fallback. Numeric normalization/clamping and legacy schema migration remain in place. Successful reloads retain the existing rescheduling/reset behavior.

Writes serialize and reparse the entire TOML document before creating a same-directory exclusive temporary file, then flush, close and atomically replace the destination. Unsupported nested arrays/arrays of tables are rejected before touching the existing file. This preserves old bytes on ordinary save failures; it does not promise power-loss durability for the parent directory.

Trigger selection builds one chunk/type index per reconciliation. A chunk trigger visits only its matching bucket; a 3x3 trigger visits at most nine cleanable buckets. Selection remains in the same tick, preserves candidate/protection semantics and deduplicates runtime IDs before removal. Expected cost is O(N + T + matching visits), rather than rescanning all N actors for each trigger; overlapping triggers still revisit matching candidates and this is not a fixed wall-clock tick budget.


Scheduled cleanup depends on `papi` and `spark`. If either is missing, PAPI is inactive, the Spark expansion is not registered, the placeholder has no usable samples yet, or the returned value cannot be parsed, scheduled cleanup is skipped. Manual operator commands (`/ecl clean`, `/ecl clean item`, `/ecl clean entity`) bypass the MSPT gate.

## Commands

All commands are operator-only by default.

```text
/ecl
```

Open the configuration form.

```text
/ecl clean
```

Immediately run the currently enabled item/entity cleaners.

```text
/ecl clean item
```

Immediately run dropped-item cleanup.

```text
/ecl clean entity
```

Immediately run entity cleanup.

```text
/ecl reload
```

Reload configuration and safely cancel/recreate both scheduled tasks.

## Configuration format in 0.3.3

ECleaner now uses only `plugins/ecleaner/config.toml` for its main configuration. Legacy `config.json` is ignored and is not migrated. Remove the old configuration before updating, or simply leave it unused; if `config.toml` does not exist, ECleaner generates a fresh TOML file with the current defaults.

Language files remain JSON under `plugins/ecleaner/language/`.

