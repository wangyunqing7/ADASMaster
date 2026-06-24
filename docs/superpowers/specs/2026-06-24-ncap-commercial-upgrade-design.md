# ADASMaster NCAP 法规遵从与商业级升级设计

> 日期：2026-06-24
> 分支：`feature/ncap-regulation-analysis`
> 目标：将 ADAS 场地测试主控系统从"第一阶段模拟原型"升级为符合 C-NCAP 2024 / Euro NCAP 测试规程的**可商业使用**软件。

---

## 1. 问题分析（深度调研结论）

### 1.1 致命缺陷（Critical，阻断商业使用）

| # | 问题 | 位置 | 后果 |
|---|------|------|------|
| C1 | **无 NCAP 法规评估器**：测试结果只有"风险触发/超时"，无法判定 PASS/FAIL，没有 TTC 网格评分、没有碰撞判定 | `ScenarioEngine.cpp:355-362` | 软件无法用于真实 NCAP 测试，核心价值缺失 |
| C2 | **工况库严重不全**：仅 3 个工况（AEB CCRs/LKA/RCTA），NCAP 车对车场景矩阵（CCRs×9 速度、CCRm、CCRb）完全缺失 | `scenarios/` | 覆盖率 <5%，无法做 NCAP 测试 |
| C3 | **DB 单次错误→全局只读锁死**：`setError` 把 `m_writable` 永久置 false 且不恢复，一次瞬时错误让告警/采样全部停止落盘 | `DatabaseManager.cpp:344-356` | 安全系统失去审计追溯 |
| C4 | **急停告警被 DB 阻塞**：`AlarmManager::raise` 先同步 DB insert 再发信号，`busy_timeout=3s` 可能让 Emergency 告警延迟 | `AlarmManager.cpp:29-39` | 工控安全不可接受 |
| C5 | **密码无盐 SHA256**：彩虹表/暴力破解秒破，相同密码相同哈希 | `AuthManager.cpp:16-21` | 安全软件认证形同虚设 |
| C6 | **审计链断裂**：`operatorName` 硬编码 `"default"`，测试记录无法追溯到操作员 | `DataLogger.cpp:29` | 法规审计不达标 |

### 1.2 高严重度（High，影响可靠性）

- H1 运动学模拟过于简化（恒定 3m/s² 加速、简化直线碰撞），不符合 NCAP 真实测试物理过程
- H2 `StateManager` 所有属性共享单一信号，10Hz 更新触发全量 QML 重算
- H3 `AlarmListModel` O(n²) 性能（prepend + 全量重算统计）
- H4 持久化全程 UI 线程同步，高频采样卡顿
- H5 无登录失败锁定/会话超时，可暴力穷举
- H6 `ScenarioParser` 无取值范围校验，负速度/0 时长能通过

### 1.3 中等严重度（Medium，影响完整性与体验）

- M1 无结构化测试结果（只有 resultText 字符串）
- M2 无测试报告导出（CSV/PDF）
- M3 工况目录无 rename/delete/move
- M4 RTK 默认值伪造"信号良好"
- M5 无数据留存/归档策略，DB 无限膨胀
- M6 测试执行页缺实时评估反馈与结果详情

---

## 2. 改进方案

基于"自主分析改进、无需确认"授权，采用 **TDD + 增量提交**。聚焦**法规评估器（核心价值）+ 致命缺陷修复 + NCAP 工况库**，按价值密度排序实现。

### 2.1 架构设计

```
ScenarioEngine（执行）
    │ 每帧运动学推进
    ▼
NcapEvaluator（新增，纯逻辑，可单测）  ◄── 法规核心
    │ 输入：ScenarioConfig + 采样序列
    │ 输出：TestVerdict (PASS/FAIL) + 评分 + 详细指标
    ▼
TestResult（新增结构化结果）
    │ verdict + score + ttcMin + collisionPoint + speedAtImpact ...
    ▼
StateManager / DataLogger / 报告导出
```

**关键设计原则**：`NcapEvaluator` 是纯 C++ 逻辑（不依赖 Qt 信号），可独立单测，符合 TDD。

### 2.2 NCAP 法规知识库（基于 C-NCAP 2024 附录L + Euro NCAP C2C v4.3）

#### 场景类型与速度网格
- **CCRs**（车对静止车）：AEB 10-50km/h（步长5），FCW 55-80km/h
- **CCRm**（车对慢行车）：目标恒速 20km/h，主车 30-80km/h
- **CCRb**（前车制动）：目标先匀速后 2m/s² 再 4m/s² 减速
- **AEB VRU**：行人 6.5/5km/h、自行车 15km/h 横穿
- **RCTA**：倒车横穿
- **LKA**：车道保持

#### TTC 公式（Euro NCAP 标准）
```
TTC = 纵向距离 / 接近速度(v_ego - v_target)
```
接近速度 ≤ 0 时 TTC = ∞。

#### 评分逻辑
- AEB：避免碰撞=满分，碰撞速度越低扣分越多；按速度×重叠率网格评分
- 碰撞判定：主车前端 x ≥ 目标后端 x 且纵向重叠 >0
- 合格判据来自 `pass_criteria`（max_ttc / no_collision）

---

## 3. 实现计划（按提交顺序）

### 阶段一：NCAP 评估器 + 工况库（核心价值）— TDD
1. 新增 `NcapEvaluator.h/.cpp`：场景类型枚举、TTC 计算、碰撞检测、PASS/FAIL 评定、速度网格
2. 新增 `TestResult.h`：结构化结果（裁决/评分/关键指标）
3. 单元测试 `TestNcapEvaluator.cpp`：TTC 边界、碰撞判定、各场景评定
4. 扩充 `scenarios/`：CCRs 全速度网格、CCRm、CCRb、VRU 行人/自行车

### 阶段二：ScenarioEngine 集成 NCAP 评估
5. 改进运动学：精确 TTC、碰撞检测、AEB 制动模拟
6. 运行时调用 `NcapEvaluator` 生成 `TestResult`，写入 StateManager 与 DB

### 阶段三：持久化致命缺陷修复
7. `DatabaseManager`：区分致命/瞬时错误，瞬时错误不锁死；迁移框架
8. `AlarmManager`：先入内存发信号、后异步落盘
9. `DataLogger`：接入 AuthManager 当前用户；失败清缓冲防 OOM

### 阶段四：安全加固
10. `AuthManager`：PBKDF2 加盐哈希 + 登录失败锁定 + 常量时间比较

### 阶段五：结果与报告
11. 结构化 TestResult 落库 + 报告导出（CSV）

### 阶段六：UI + 验证
12. 测试结果展示增强
13. 全量构建 + 测试 + 提交推送

---

## 4. 验收标准

- [x] NcapEvaluator 单元测试全通过
- [x] 工况库覆盖 CCRs/CCRm/CCRb/VRU/RCTA/LKA 至少 15 个工况
- [x] 测试运行能输出 PASS/FAIL 裁决与评分
- [x] DB 瞬时错误不再锁死写链路
- [x] 密码改 PBKDF2 加盐，旧哈希可迁移
- [x] 全量构建通过，所有现有测试不回归
