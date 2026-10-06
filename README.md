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
- 0.3.0 adds an MSPT-independent Chunk Entity Guard with per-type, per-chunk, and 3x3 density fuses for runaway mob reactors/farms.
- A shared high-value entity protection policy preserves villagers, pets, mounts, allays, shulkers, named mobs, and actors tagged `ecleaner_protect` by default. Protected mobs still count toward pressure.

## Installation

Place the plugin binary in the Endstone server's `plugins` directory. On first startup ECleaner creates:

```text
plugins/ecleaner/config.json
```

Language files live under:

```text
plugins/ecleaner/language/
```

## Default configuration

```json
{
    "language": "zh_CN",
    "auto_item_clean": true,
    "auto_entity_clean": true,
    "item_clean_interval_seconds": 10,
    "entity_clean_interval_seconds": 60,
    "broadcast_cleanup_results": false,
    "mspt_threshold": 50.0,
    "mspt_window": "10s",
    "mspt_statistic": "p95",
    "item_clean_whitelist": false,
    "item_clean_ids": [
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
        "minecraft:red_sandstone"
    ],
    "item_clean_legacy_names": [],
    "entity_clean_whitelist": false,
    "entity_clean_list": [
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
        "minecraft:phantom"
    ],
    "entity_protection": {
        "enabled": true,
        "protect_named": true,
        "protect_tag": "ecleaner_protect",
        "types": [
            "minecraft:villager",
            "minecraft:villager_v2",
            "minecraft:zombie_villager",
            "minecraft:zombie_villager_v2",
            "minecraft:allay"
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
            "minecraft:shulker"
        ]
    },
    "chunk_entity_guard": {
        "enabled": true,
        "reconcile_interval_ticks": 20,
        "pressure_mspt_threshold": 50.0,
        "pressure_type_limits": {
            "minecraft:slime": 96,
            "minecraft:silverfish": 128,
            "minecraft:magma_cube": 96
        },
        "hard_type_limits": {
            "minecraft:slime": 256,
            "minecraft:silverfish": 256,
            "minecraft:magma_cube": 256
        },
        "cleanable_types": [
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
            "minecraft:phantom"
        ],
        "pressure_cleanable_mob_limit_per_chunk": 192,
        "hard_cleanable_mob_limit_per_chunk": 384,
        "pressure_total_mob_limit_per_chunk": 320,
        "hard_total_mob_limit_per_chunk": 512,
        "pressure_cleanable_mob_limit_3x3": 512,
        "hard_cleanable_mob_limit_3x3": 768,
        "emergency_delete_protected_mobs": false,
        "log_triggers": true,
        "log_cooldown_seconds": 10
    }
}
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

## Upgrading from 0.1.x

0.2.0 removes the legacy `clean_time` and `clean_tps` keys and adds independent second-based intervals.

- An untouched 0.1.x default config is migrated to the new performance-first defaults: 10 seconds for items and 60 seconds for entities. The old shulker-box whitelist is also replaced with the low-value item blacklist so high-frequency cleanup does not delete almost every dropped item.
- A customized legacy `clean_time` is converted from minutes to seconds and applied to both new schedules; `clean_time = 0` remains disabled.
- Customized blacklist/whitelist modes and list contents are preserved where possible. Legacy `item_clean_list` entries are migrated to stable ItemType IDs; unknown custom English names are retained in `item_clean_legacy_names` as a compatibility fallback.

Upgrading a 0.3.0 config migrates the old chunk limits into the new pressure-limit fields without changing custom values, then adds the new hard-limit safety tier. Removing `plugins/ecleaner/config.json` and restarting regenerates the new 0.3.1 observation defaults.
