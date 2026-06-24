// =============================================================================
// NcapEvaluator.cpp —— 见头文件注释。NCAP 法规评估核心逻辑实现。
// 参考：Euro NCAP AEB C2C Test Protocol v4.3 / C-NCAP 2024 管理规则附录L。
// =============================================================================

#include "NcapEvaluator.h"

#include <algorithm>
#include <cmath>

double NcapEvaluator::computeTtc(double longitudinalDistanceM,
                                 double egoSpeedMps,
                                 double targetSpeedMps)
{
    // 接近速度 = 主车速度 - 目标速度（沿同一方向）
    const double closingSpeed = egoSpeedMps - targetSpeedMps;
    // 未接近（目标更快或同速）→ TTC 无穷，表示不会追上
    if (closingSpeed <= 0.0)
        return std::numeric_limits<double>::infinity();
    // 距离已为 0 且仍在接近 → TTC=0（即将碰撞）；但这里距离应非负
    if (longitudinalDistanceM <= 0.0)
        return 0.0;
    return longitudinalDistanceM / closingSpeed;
}

double NcapEvaluator::lateralOverlap(double egoY, double targetY, double vehicleWidthM)
{
    if (vehicleWidthM <= 0.0)
        return 0.0;
    const double dy = std::abs(egoY - targetY);
    return std::clamp(1.0 - dy / vehicleWidthM, 0.0, 1.0);
}

bool NcapEvaluator::detectsCollision(const ActorPose &ego, double egoHalfLenM,
                                     const ActorPose &target,
                                     double targetWidthM, double egoWidthM)
{
    // 纵向：主车前端（沿 yaw 方向）触及目标中心点。
    // 简化模型：主车沿 X 轴正方向行驶（yaw≈0），前端 x = ego.x + egoHalfLen。
    // 目标用中心点近似，当主车前端越过目标中心即判定碰撞（NCAP 测试的可接受简化）。
    const double egoFrontX = ego.x + egoHalfLenM;
    const bool longitudinalOverlap = egoFrontX >= target.x;

    // 横向：用两车宽度的较小值计算重叠率（保守）
    const double effectiveWidth = std::min(egoWidthM, targetWidthM);
    const double overlap = lateralOverlap(ego.y, target.y, effectiveWidth);

    return longitudinalOverlap && overlap > 0.0;
}

NcapVerdict NcapEvaluator::evaluate(const ScenarioConfig &config, const TestResult &result)
{
    NcapVerdict verdict;
    verdict.detail.clear();

    // 未正常完成（急停/异常中断）直接 FAIL
    if (!result.completedNormally) {
        verdict.verdict = NcapOutcome::Fail;
        verdict.scorePercent = 0.0;
        verdict.detail = QStringLiteral("测试未正常完成");
        return verdict;
    }

    const QString type = config.type.toLower();
    const QString subgroup = config.subgroup.toLower();

    // 优先级 1：TTC 合格判据（明确设定 passTtcSec 的工况，无论是否碰撞）
    if (config.passTtcSec > 0.0) {
        if (result.minTtcSec >= config.passTtcSec) {
            verdict.verdict = NcapOutcome::Pass;
            verdict.scorePercent = 100.0;
            verdict.detail = QStringLiteral("最小 TTC %1 s 满足要求（≥%2 s）")
                                 .arg(result.minTtcSec, 0, 'f', 2).arg(config.passTtcSec);
        } else {
            verdict.verdict = NcapOutcome::Fail;
            verdict.scorePercent = 0.0;
            verdict.detail = QStringLiteral("最小 TTC %1 s 不满足要求（<%2 s）")
                                 .arg(result.minTtcSec, 0, 'f', 2).arg(config.passTtcSec);
        }
        return verdict;
    }

    // 优先级 2：AEB 类工况的碰撞判定 + 按碰撞速度评分
    if (type == QStringLiteral("aeb") || config.noCollisionRequired) {
        if (!result.collisionOccurred) {
            // 避免碰撞 = 满分通过
            verdict.verdict = NcapOutcome::Pass;
            verdict.scorePercent = 100.0;
            verdict.detail = QStringLiteral("成功避免碰撞");
            return verdict;
        }
        // 发生碰撞：按碰撞速度占目标速度的比例扣分。
        // 碰撞速度越低（制动越有效）得分越高；全速碰撞得 0 分。
        if (result.egoTargetSpeed > 0.0) {
            const double ratio = std::clamp(result.speedAtImpactMps / result.egoTargetSpeed, 0.0, 1.0);
            // 速度降低比例 → 得分：完全刹停 100%，全速碰撞 0%
            verdict.scorePercent = (1.0 - ratio) * 100.0;
        }
        verdict.verdict = NcapOutcome::Fail;
        verdict.detail = QStringLiteral("发生碰撞，碰撞速度 %1 m/s")
                             .arg(result.speedAtImpactMps, 0, 'f', 1);
        return verdict;
    }

    // 兜底：无法判定
    verdict.verdict = NcapOutcome::Pending;
    verdict.detail = QStringLiteral("缺少合格判据");
    return verdict;
}

QList<int> NcapEvaluator::speedGrid(const QString &scenarioSubgroup)
{
    QList<int> speeds;
    const QString key = scenarioSubgroup.toLower();

    if (key == QStringLiteral("ccrs")) {
        // CCRs AEB：10-50 km/h，步长 5 → 9 个测试点
        for (int v = 10; v <= 50; v += 5)
            speeds.append(v);
    } else if (key == QStringLiteral("ccrm")) {
        // CCRm AEB：30-80 km/h，步长 5
        for (int v = 30; v <= 80; v += 5)
            speeds.append(v);
    } else if (key == QStringLiteral("ccrb")) {
        // CCRb：主车 50-80 km/h
        for (int v = 50; v <= 80; v += 5)
            speeds.append(v);
    } else if (key == QStringLiteral("vru") || key.contains(QStringLiteral("pedestrian"))) {
        // AEB VRU 行人：20-60 km/h
        for (int v = 20; v <= 60; v += 5)
            speeds.append(v);
    }
    return speeds;
}
