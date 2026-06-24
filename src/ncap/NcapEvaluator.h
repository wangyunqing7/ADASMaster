#pragma once

// =============================================================================
// NcapEvaluator.h
// 功能：NCAP/C-NCAP/E-NCAP 法规评估核心逻辑（纯函数，无 Qt 信号依赖，可单测）。
//       实现 Euro NCAP AEB C2C v4.3 / C-NCAP 2024 附录L 的测试规程：
//         1. TTC 计算（纵向距离 / 接近速度）
//         2. 碰撞检测（几何重叠：纵向重叠 + 横向重叠率）
//         3. PASS/FAIL 评定（避免碰撞满分、部分制动按碰撞速度比例评分）
//         4. 速度网格生成（CCRs 10-50km/h 步长 5 等）
// 输入：ScenarioConfig（工况参数）+ TestResult（运行观测）+ ActorPose（位姿）。
// 输出：NcapVerdict（裁决/评分/详情），或 bool（碰撞）, double（TTC/重叠率）。
//
// 设计原则：所有方法静态、无状态，便于单元测试覆盖法规边界。
// =============================================================================

#include <QList>
#include <QString>
#include "../scenario/ScenarioConfig.h"
#include "TestResult.h"

// 运动体瞬时位姿（中心点），单位：m（坐标），deg（yaw，沿 X 轴为 0）
struct ActorPose
{
    double x = 0.0;
    double y = 0.0;
    double yawDeg = 0.0;
};

class NcapEvaluator
{
public:
    // ---- TTC 计算（Euro NCAP 标准）----
    // 纵向距离 / 接近速度；接近速度 <= 0 时返回 ∞
    static double computeTtc(double longitudinalDistanceM,
                             double egoSpeedMps,
                             double targetSpeedMps);

    // ---- 碰撞检测 ----
    // 判断主车前端是否与目标几何重叠（纵向重叠 + 横向重叠 > 0）。
    // egoHalfLen/egoWidth/targetWidth 默认按 NCAP GVT 尺寸。
    static bool detectsCollision(const ActorPose &ego,
                                 double egoHalfLenM,
                                 const ActorPose &target,
                                 double targetWidthM,
                                 double egoWidthM);

    // ---- 横向重叠率（0~1）----
    // 两车中心横向偏移 dy，车宽 egoWidth；重叠 = max(0, width - |dy|) / width
    static double lateralOverlap(double egoY, double targetY, double vehicleWidthM);

    // ---- 场景评定（核心）----
    // 根据工况类型与运行结果给出 PASS/FAIL + 评分。
    static NcapVerdict evaluate(const ScenarioConfig &config, const TestResult &result);

    // ---- 速度网格（按场景类型返回法定测试速度 km/h）----
    // CCRs AEB: 10-50 步长 5；CCRm AEB: 30-80 步长 5；等
    static QList<int> speedGrid(const QString &scenarioSubgroup);
};
