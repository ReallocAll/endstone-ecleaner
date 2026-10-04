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
        "minecraft:phantom"
    ]
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

自动实体清理会跳过带有自定义 NameTag 的实体。

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

如果配置已经比较混乱，删除旧的 `plugins/ecleaner/config.json` 后重启即可重新生成 0.2.0 默认配置。
