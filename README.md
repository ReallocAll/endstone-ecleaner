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
        "minecraft:phantom"
    ]
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

Automatic entity cleanup skips entities with a custom NameTag.

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

If the old config is no longer useful, remove `plugins/ecleaner/config.json` and restart to regenerate the 0.2.0 defaults.
