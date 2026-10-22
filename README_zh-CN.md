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
- `falling_block` 守卫默认只监测压力与超预算数量，不再通过删除生成实体干扰机器；破坏性拒绝必须显式启用。
- 0.3.3 将主配置改为 TOML。旧 `config.json` 不再读取，也不做迁移；没有 `config.toml` 时直接按当前默认值生成新配置。
- 0.3.4 避免无生成请求或请求低于限额时持续退避，并补充允许/拒绝生成的诊断统计。

## 安装

将对应平台的插件文件放入 Endstone 服务端的 `plugins` 目录。首次启动后会生成：

```text
plugins/ecleaner/config.toml
```

语言文件位于：

```text
plugins/ecleaner/language/
```

## 默认配置

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

默认 `mode = "monitor"`：继续统计按维度的生成需求、压力和令牌桶预算，但绝不取消生成。旧配置缺少 mode 时也迁移到监测模式。超预算用 `over_budget` 单独记录，不能把它称为已拒绝或已削减产量。监测模式不强制控制下落方块负载，过载时需通过机器自身控制停止或降速。

明确配置 `mode = "reject"` 才会启用旧版破坏性拒绝。Endstone 的取消事件会 despawn 已加入世界的实体，不是暂停或重试，可能删除机器循环方块。最低配额不能保证机器继续运转；启动和重载会输出风险警告。

两种模式均保留原有压力阈值、激活需求、令牌桶、最低预算、退避和恢复迟滞。高 MSPT 下仅在需求超过预算时继续退避；缺少 MSPT 时不新增限制并保持现有计算预算。定时清理仍依赖 PAPI/Spark；手动命令绕过 MSPT 门控。更多依据与限制见文末“配置事务与刷沙机保护更新”。

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

## 0.3.3 配置格式

ECleaner 主配置现在只使用 `plugins/ecleaner/config.toml`。旧的 `config.json` 会被忽略，不进行自动迁移。更新前删除旧配置即可；如果 `config.toml` 不存在，插件会直接按当前默认值生成一份全新的 TOML 配置。

语言文件仍然保持 JSON，位于 `plugins/ecleaner/language/`。


## 配置事务与刷沙机保护更新

`falling_block_guard.mode` 默认改为 `"monitor"`；旧配置缺少该字段时也迁移到监测模式。保留原有按维度的压力反馈、令牌桶和恢复迟滞，但不再取消下落方块生成。日志中的 `over_budget` 表示超出计算预算的数量，`rejected` 才是实际拒绝数量。监测模式不会强制降低下落方块负载；过载时需要通过机器自身控制停止或降速。

原因：Endstone v0.11.0 的 ActorSpawnEvent 取消实现会在实体加入世界后调用 despawn，并不是暂停生产、延迟重试或恢复源方块。删除循环中的重力方块可能使刷沙机失效；即使保留每秒 16 个的最低配额，也不能保证机器继续运行。此更新不能恢复此前已丢失的方块。监测模式下通用实体清理也保留下落方块（包括白名单模式）；区块守卫仍只处理生物。

只有明确接受破坏风险时，才设置 `mode = "reject"` 恢复旧版的超额拒绝行为；启动和重载会警告。它可能破坏刷沙机或普通重力方块结构。没有经过真实 BDS 机器复现，不能宣称实现了无损自动限速。reject 模式的最低预算不能作为机器正常运行的保证。

依据：[Endstone 生成事件实现](https://github.com/EndstoneMC/endstone/blob/v0.11.0/src/endstone/runtime/bedrock_hooks/script_level_gameplay_handler.cpp)、[事件接口](https://github.com/EndstoneMC/endstone/blob/v0.11.0/include/endstone/event/actor/actor_spawn_event.h)、[基岩版 MCPE-185621 报告](https://bugs.mojang.com/browse/MCPE/issues/MCPE-185621)。报告说明活塞、传送门与重力方块的耦合机制，不代表已复现本服务器的问题。

配置重载和表单保存先完整验证核心配置、删除集合、语言和两个守卫，再在主线程发布。失败不改变原有策略、守卫运行状态或任务周期；成功重载仍按原逻辑重新调度。文件先完整序列化并重新解析验证，再写同目录独占临时文件、刷新、关闭和原子替换。不支持的嵌套数组或表数组在打开输出前拒绝，保留旧文件；不承诺断电后的目录持久化。

清理选择每轮构建一次区块/类型索引，单区块触发只访问对应候选，3×3 触发最多查九个区块。保持同 tick 执行、保护谓词和 runtime ID 去重。预期复杂度为 O(N + T + 匹配访问数)，不是固定耗时预算；重叠触发仍可能重复访问匹配实体。
