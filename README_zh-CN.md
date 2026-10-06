![header](https://capsule-render.vercel.app/api?type=waving&height=300&color=gradient&text=ECleaner)

[English](README.md)

## 介绍

ECleaner 是一个面向 Endstone 的轻量实体清理插件。本分支从 0.2.0 起改为以服务器性能为优先的高频清理策略：

- 掉落物与实体使用独立定时器。
- 检查间隔使用“秒”，默认掉落物每 10 秒检查一次、实体每 60 秒检查一次；只有 MSPT 门控通过时才执行实际清理。
- 默认静默清理，不再每轮播放声音或提前 30 秒广播。
- 默认只清理明确配置在黑名单中的低价值掉落物和常见敌对生物。
- 有自定义名称的实体不会被自动清理。
- 服务器无人在线时跳过自动清理。
- 自动清理由 Spark MSPT 压力阈值控制：默认仅当最近 10 秒的 p95 MSPT ≥ 50 ms 时才执行，避免服务器健康时无意义地删除掉落物或刷怪塔产物。
- 通过 Endstone PAPI 读取 Spark 占位符；PAPI/Spark 未就绪或 MSPT 数据不可用时自动清理会 fail-closed（跳过清理）。
- 0.3.1 将 Chunk Entity Guard 改为“MSPT 压力阈值 + 无条件硬上限”两级保护：服务器健康时不受普通压力阈值限制，只有达到更高硬上限才强制熔断。
- 高价值生物使用统一保护策略：默认保护村民、宠物、坐骑、悦灵、潜影贝等，也保护命名实体和带 `ecleaner_protect` scoreboard tag 的实体。受保护实体仍计入压力，但默认不会被自动删除。
- 0.3.2 新增 `falling_block` 自适应限产：MSPT 健康时完全不限速，达到压力阈值后按指数退避降低刷沙机/重力方块机器产率，但不会低于配置的最低产量。

## 安装

将对应平台的插件文件放入 Endstone 服务端的 `plugins` 目录。首次启动后会生成：

```text
plugins/ecleaner/config.json
```

语言文件位于：

```text
plugins/ecleaner/language/
```

## 默认配置

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
    },
    "falling_block_guard": {
        "enabled": true,
        "control_interval_ticks": 20,
        "pressure_mspt_threshold": 50.0,
        "recovery_mspt_threshold": 45.0,
        "backoff_factor": 0.5,
        "recovery_factor": 2.0,
        "recovery_stable_intervals": 5,
        "min_rate_per_second": 16.0,
        "activation_rate_per_second": 16.0,
        "burst_capacity": 32.0,
        "log_adjustments": true
    }
}
```

### 配置说明

`auto_item_clean`：是否启用定时掉落物清理。

`auto_entity_clean`：是否启用定时实体清理。

`item_clean_interval_seconds`：掉落物自动清理的检查间隔，单位秒。默认每 10 秒检查一次 MSPT；只有达到阈值才执行清理。设为 `0` 时关闭掉落物定时清理。

`entity_clean_interval_seconds`：实体自动清理的检查间隔，单位秒。默认每 60 秒检查一次 MSPT；只有达到阈值才执行清理。设为 `0` 时关闭实体定时清理。

`broadcast_cleanup_results`：是否把每轮自动清理结果广播给全服。默认 `false`，即静默清理。

`mspt_threshold`：自动清理启动阈值，单位 ms。默认 `50.0`。只有 Spark 返回的 MSPT 指标达到或超过该值时，定时清理才真正执行；设为 `0` 可关闭 MSPT 门控并恢复“到点就清”的行为。

`mspt_window`：Spark MSPT 统计窗口，可选 `10s` 或 `1m`，默认 `10s`。

`mspt_statistic`：从 Spark `{spark:tickduration_*}` 的 `min/median/p95/max` 四项中选择用于判定的指标，可选 `min`、`median`、`p95`、`max`，默认 `p95`。默认策略因此是“最近 10 秒 p95 MSPT ≥ 50 ms 才清理”。

`item_clean_whitelist`：掉落物名单模式。为 `false` 时只有名单内物品会被删除；为 `true` 时名单内物品被保留、其他掉落物会被删除。

`item_clean_ids`：掉落物名单，使用稳定的 ItemType ID，例如 `minecraft:netherrack`。不再依赖客户端语言或英文显示名。

`item_clean_legacy_names`：仅用于兼容旧版无法映射的英文显示名。新配置通常保持为空。

`entity_clean_whitelist`：实体名单模式。为 `false` 时只有名单内实体会被删除；为 `true` 时名单内实体被保留、其他实体会被删除。

`entity_clean_list`：实体名单，使用实体 ID，例如 `minecraft:zombie`。

### 高价值生物保护

`entity_protection.enabled`：统一高价值实体保护总开关。默认 `true`。该策略同时作用于 MSPT 实体清理与 Chunk Entity Guard。

`entity_protection.types`：受保护实体类型白名单。默认包括村民、僵尸村民、悦灵、马/驴/骡/骆驼/羊驼、狼、猫、鹦鹉、嗅探兽、铁傀儡、雪傀儡和潜影贝。

`entity_protection.protect_named`：默认 `true`，有自定义 NameTag 的实体不会被自动删除。

`entity_protection.protect_tag`：实例级保护 scoreboard tag，默认 `ecleaner_protect`。可用于保护不在类型白名单中的特殊 NPC、宠物或展示实体。

受保护实体**仍然计入区块/区域压力统计**。这可以准确反映真实实体负载，同时避免普通熔断误删高价值生物。

### Chunk Entity Guard

Chunk Entity Guard 现在分为两级：**性能压力阈值**和**无条件硬上限**。它读取根配置中的 `mspt_window` / `mspt_statistic` 所选 Spark 指标，但拥有独立的 `chunk_entity_guard.pressure_mspt_threshold`，默认 `50.0 ms`。

只有 MSPT 达到该阈值时才启用压力级限制。观察阶段默认放宽为：史莱姆 96/区块、蠹虫 128/区块、岩浆怪 96/区块；cleanable Mob 192/区块；全部 Mob 320/区块；cleanable Mob 512/3×3。

MSPT 低于 50 ms 时，上述压力限制**不介入**。但无条件硬上限始终有效：上述三个高风险类型均为 256/区块，cleanable Mob 384/区块，全部 Mob 512/区块，cleanable Mob 768/3×3。这样正常高产机器不会因为普通数量阈值被削产，同时失控机器仍有最终保险。

`chunk_entity_guard.reconcile_interval_ticks`：权威全量校准周期，默认 20 tick。ActorSpawnEvent/ActorRemoveEvent 维护快速计数；硬上限达到时，或压力状态下达到压力阈值时，会请求下一 tick 立即校准。MSPT 每次 reconcile 只查询一次，不会在每个 spawn callback 中查询 PAPI。

如果 PAPI/Spark 数据暂时不可用，压力级限制 fail-closed，不执行删除；无条件硬上限仍然有效。将 `pressure_mspt_threshold` 设为 `0` 可让压力级限制恢复为无条件生效。

`chunk_entity_guard.cleanable_types`：允许区块/3×3 cleanable 密度限制清理的实体类型集合。

`chunk_entity_guard.emergency_delete_protected_mobs`：默认 `false`。受保护实体仍计入密度，但默认不会被删除；显式设为 `true` 后，chunk-total 压力/硬熔断可以删除受保护 Mob。玩家永远不会被删除。

熔断日志会区分 `pressure` / `hard`，并在可用时记录触发瞬间的 MSPT。`log_triggers` / `log_cooldown_seconds` 只控制日志频率，不影响实际保护。

### Falling Block Guard

Falling Block Guard 不是固定实体数量上限，而是一个基于 MSPT 的反馈限产控制器。它按维度统计 `minecraft:falling_block` 的生成尝试，并且每个控制周期只读取一次 Spark MSPT。

当 MSPT 低于 `pressure_mspt_threshold`（默认 50 ms）时，刷沙机/重力方块机器**完全不限速**。当 MSPT 达到阈值，并且某维度的生成需求至少达到 `activation_rate_per_second`（默认 16/s）时，允许产率每个控制周期乘以 `backoff_factor`（默认 0.5），也就是逐级 50% 指数退避，直到 MSPT 恢复或降到最低产率。

默认最低产率 `min_rate_per_second = 16`，即 **57,600 个/小时**。即使服务器持续处于压力状态，也不会把机器彻底关停。默认 `burst_capacity = 32` 的令牌桶用于吸收短时间突发。

恢复速度刻意慢于退避：MSPT 必须连续 `recovery_stable_intervals = 5` 个控制周期低于 `recovery_mspt_threshold = 45 ms`，允许产率才乘以 `recovery_factor = 2` 放宽一级。当下一档已经足以满足机器实际生成需求时，直接解除限速。

控制器按**维度**独立工作，因此末地失控刷沙机不会占用主世界的产率额度。生成尝试会在被拒绝前计数，因此即使处于限速状态，也能估计机器原本的真实需求。PAPI/Spark 不会在每个 ActorSpawnEvent 中查询，而是每个控制周期查询一次；MSPT 数据短暂不可用时不会新建限速，已有的限速保持当前档位等待反馈恢复。

自适应限产不会删除已经存在的 falling_block，只会在限速期间拒绝超出允许产率的新生成。


自动定时清理依赖 `papi` 与 `spark`。两者缺失、PAPI 服务未激活、Spark expansion 未注册、占位符尚无可用样本或返回值无法解析时，自动清理会直接跳过，不会在性能数据未知时删除实体。`/ecl clean`、`/ecl clean item`、`/ecl clean entity` 属于管理员手动操作，不受 MSPT 门控限制。

## 命令

所有命令默认仅管理员可用。

```text
/ecl
```

打开配置菜单。可以调整自动清理开关、名单模式、掉落物/实体清理秒数以及是否广播自动清理结果。

```text
/ecl clean
```

立即按当前自动清理开关执行一次掉落物和实体清理。

```text
/ecl clean item
```

立即执行一次掉落物清理。

```text
/ecl clean entity
```

立即执行一次实体清理。

```text
/ecl reload
```

重新读取配置，并安全地取消、重建两个定时任务。

## 从 0.1.x 升级

0.2.0 会移除旧的 `clean_time` 与 `clean_tps` 配置项，并补充新的秒级独立清理间隔。

- 如果检测到**完全未修改的 0.1.x 默认配置**，会自动迁移到新的性能优先默认值：掉落物 10 秒、实体 60 秒，并把旧的“潜影盒白名单”改为低价值掉落物黑名单，避免 10 秒一次误删几乎所有掉落物。
- 如果旧的 `clean_time` 被手动修改过，则会按原分钟数换算成秒并同时用于两个新定时器；`clean_time = 0` 会继续保持关闭。
- 自定义过的黑/白名单与名单内容会尽量保留。旧 `item_clean_list` 会自动转换为稳定 ItemType ID；无法识别的自定义英文名会保存在 `item_clean_legacy_names` 中继续兼容。

0.3.0 配置升级时会把旧的 Chunk Entity Guard 数量阈值迁移到新的 pressure 字段，保留自定义值，并补充 hard safety tier。删除 `plugins/ecleaner/config.json` 后重启则会直接生成当前 0.3.2 默认配置，并包含新的 Falling Block Guard。
