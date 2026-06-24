# 报警中心页面增强设计（AlarmPage Enhancement）

- **日期**：2026-06-19
- **范围**：`qml/pages/AlarmPage.qml` 前端重构 + `src/alarm/` 后端扩展
- **目标**：把当前只有「共 N 条 + 全部确认 + 列表」的简陋报警中心，升级为指挥控制中心级的报警管理页面。

## 1. 背景与现状

当前 `AlarmPage.qml` 仅有：
- 顶部工具栏：`共 N 条` 文字 + `全部确认` 按钮
- 一个 `ListView` 列表（等级色条 / 等级文字 / 标题 / 消息 / 时间 / 未确认徽标 / 确认按钮）

后端 `EventLog` 已有的字段**未被页面利用**：`source`（来源，6 类）、`recovered`（已恢复）。`AlarmListModel` 有全量数据但**没有统计概览**，也**没有筛选/搜索**能力。

## 2. 目标

四个增强方向（均已与用户确认）：

1. **统计概览卡**：顶部一排紧凑指标，显示总数 / 未确认 / 各等级（信息/警告/错误/急停）数量。
2. **筛选过滤栏**：按等级、按来源筛选，「只看未确认」开关，关键词搜索。
3. **列表信息补全**：每行补显示来源标签、已恢复徽标；列表为空时占位提示。
4. **清空 / 导出**：清空当前内存列表（**保留数据库历史**，用户决策）；导出 **CSV**（全部记录，用户决策）。

## 3. 方案对比（筛选的架构实现）

| 方案 | 做法 | 结论 |
|---|---|---|
| **A（采用）** | 后端新建 `AlarmFilterProxyModel : QSortFilterProxyModel`；统计/clear/export 放进 `AlarmListModel`；`AlarmManager` 对外暴露 | Qt 标准做法、千条不卡、符合项目「QML 不直接处理数据逻辑」的分层规范 |
| B | 纯 QML 用数组手动过滤 | 违反分层约束、千条卡顿，否决 |
| C | filter 属性塞进 `AlarmListModel` 自身 | 数据模型与过滤职责混乱，否决 |

**采用 A。**

## 4. 后端设计

### 4.1 `AlarmListModel` 扩展（不新增文件）

新增 Q_PROPERTY 统计（统计**全量**，不受筛选影响），在 `append` / `acknowledge` / `acknowledgeAll` / `clear` 后通过 `recomputeStats()` 重算并 `emit` 对应 `NOTIFY`：

- `int unackedCount`
- `int infoCount` / `int warnCount` / `int errorCount` / `int emergencyCount`

新增方法：

- `Q_INVOKABLE void clear()` —— `beginResetModel` + 清空内存 `m_events` + 重置统计 + `endResetModel`。**不触碰数据库**（数据库写入在 `AlarmManager::raise` 完成，`clear` 只清内存列表）。
- `bool exportToCsv(const QUrl &fileUrl)` —— 写 UTF-8 BOM（Excel 友好）+ 中文表头 + 遍历 `m_events` 全部记录。CSV 字段统一双引号包裹、内部双引号转义。

### 4.2 新建 `AlarmFilterProxyModel`（`src/alarm/AlarmFilterProxyModel.h/.cpp`）

```cpp
class AlarmFilterProxyModel : public QSortFilterProxyModel {
    Q_OBJECT
    Q_PROPERTY(int activeLevel  ...)   // -1=全部, 0..3
    Q_PROPERTY(int activeSource ...)   // -1=全部, 0..5
    Q_PROPERTY(bool onlyUnacked ...)
    Q_PROPERTY(QString keyword ...)
protected:
    bool filterAcceptsRow(int row, const QModelIndex&) const override;
};
```

- 每个属性的 setter：赋值 → `emit changed` → `invalidateFilter()`，实时刷新可见行。
- `filterAcceptsRow`：从 source `data()` 读取 `LevelRole/SourceRole/AckedRole/TitleRole/MessageRole`，按 4 个条件组合判断；`keyword` 匹配 `title` + `message`，大小写不敏感。
- 代理自动继承 source 的 `roleNames`，QML delegate 用原名（`alarmLevel` 等）无感。

### 4.3 `AlarmManager` 扩展

- `Q_PROPERTY AlarmFilterProxyModel* filterModel` —— 构造时创建，`setSourceModel(m_model)`。
- `Q_INVOKABLE void clear()` → 转发 `m_model->clear()`。
- `Q_INVOKABLE void exportCsv(const QUrl&)` → 转发 `m_model->exportToCsv(url)`。

> 现有接口（`raise`/`info`/`warning`/`error`/`emergency`/`acknowledge`/`acknowledgeAll`/`latestEvent`）保持不变，`test_alarm_persistence` 不受影响。

## 5. 前端设计（`AlarmPage.qml` 重构）

`PanelFrame` 内容槽改为三段：

1. **统计行** —— `GridLayout`(1×6) 包 6 个 `MetricCell`（总数 / 未确认 / 信息 / 警告 / 错误 / 急停），外层 1px border 网格形成分隔线（沿用 `MetricCell` 设计意图）。数值绑定 `alarmListModel` 的统计属性，**不受筛选影响**。
2. **筛选栏** —— 等级用一组可单选 `Button`（全部/信息/警告/错误/急停）；来源用 `ComboBox`（全部 + 系统/安全/设备/RTK/工况/通信）；「只看未确认」可切换 `Button`；搜索 `TextField`（占位「搜索标题/内容」）。右侧三个操作按钮：`全部确认`、`清空`（弹确认 `Dialog`，不可逆提示）、`导出CSV`（弹 `FileDialog`）。中间显示「显示 X / 共 Y 条」。
3. **列表** —— `ListView.model` 从 `alarmListModel` 改为 `filterModel`。delegate 增强：
   - 来源 pill（`[安全]` 等文字标签，不强行配图标）
   - 确认状态三态徽标：未确认=黄、已确认=灰、已恢复=绿
   - `filterModel.count === 0` 时叠一层空状态占位。

`PanelFrame.liveText`：未确认 > 0 时红点 +「N 条未确认」，否则绿点 +「全部正常」。

## 6. 数据流

```
各模块 raise() -> AlarmManager -> AlarmListModel.append()
                                      |
            +-------------------------+-------------------------+
            v (NOTIFY stats)                    v (QSortFilterProxyModel 自动)
      统计卡刷新                            ListView 可见行刷新

筛选控件 ==双向绑定==> filterModel.{activeLevel,activeSource,onlyUnacked,keyword}
                                      \-> invalidateFilter -> 列表刷新
[清空]   -> alarmManager.clear()      -> 内存列表清空(数据库保留)
[导出]   -> FileDialog -> alarmManager.exportCsv(url) -> 写 CSV
```

## 7. 构建约束（CLAUDE.md 硬要求）

- 新增 `AlarmFilterProxyModel.h/.cpp` → **必须登记到 `CMakeLists.txt` 的 `adasmaster_core` 库列表**（含 `Q_OBJECT`，需 AUTOMOC）。
- `AlarmPage.qml` 路径不变，已在 `QML_FILES`，重构无需重新登记。
- 无新增 `.json`/`.qml`/图标。

## 8. 验证

- 构建：`cmake --build build`
- 回归：`test_alarm_persistence`（确认未破坏 AlarmManager/AlarmListModel 既有接口）
- QML lint：`qml_lint_gate`
- 手动：运行 `./build/ADASMaster.exe`，进报警中心页，验证统计实时刷新、筛选/搜索、来源/已恢复显示、清空、CSV 导出。
