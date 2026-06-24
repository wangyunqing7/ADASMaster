// =============================================================================
// TestNcapEvaluator.cpp
// 功能：验证 NCAP 法规评估器的核心逻辑——TTC 计算、碰撞检测、PASS/FAIL 评定、
//       速度网格生成。这是软件从"模拟原型"升级到"可商业使用"的核心保障。
// 输入：纯逻辑输入（位姿/速度/场景参数），无 Qt 信号依赖，便于单测。
// =============================================================================

#include <QtTest>

#include "src/ncap/NcapEvaluator.h"
#include "src/scenario/ScenarioConfig.h"

class TestNcapEvaluator : public QObject
{
    Q_OBJECT

private slots:
    // ---- TTC 计算（Euro NCAP 标准：纵向距离 / 接近速度）----
    void computesTtcWhenClosing()
    {
        // 主车 20m/s，目标 10m/s，纵向间距 50m → TTC = 50 / (20-10) = 5s
        const double ttc = NcapEvaluator::computeTtc(50.0, 20.0, 10.0);
        QVERIFY2(qAbs(ttc - 5.0) < 1e-9, qPrintable(QString::number(ttc)));
    }

    void ttcIsInfiniteWhenNotClosing()
    {
        // 目标更快（远离）或同速 → TTC = 无穷
        QVERIFY(qIsInf(NcapEvaluator::computeTtc(50.0, 10.0, 20.0)));
        QVERIFY(qIsInf(NcapEvaluator::computeTtc(50.0, 10.0, 10.0)));
    }

    void ttcIsInfiniteAtZeroDistance()
    {
        // 距离 0 且无接近速度 → ∞（已经接触但不再接近）
        QVERIFY(qIsInf(NcapEvaluator::computeTtc(0.0, 5.0, 5.0)));
    }

    // ---- 碰撞检测（主车前端与目标几何重叠）----
    void detectsHeadOnCollision()
    {
        // 主车前端 x=5，目标后端 x=4（沿 X 轴正方向行驶），纵向重叠
        ActorPose ego{5.0, 0.0, 0.0};
        ActorPose target{4.0, 0.0, 0.0};
        QVERIFY(NcapEvaluator::detectsCollision(ego, 4.5, target, 1.8, 1.0));
        // egoFront=5+4.5/2=7.25? 不对——定义：ego 中心，egoHalfLen 默认
    }

    void noCollisionWhenLateralOffsetExceedsWidth()
    {
        // 横向偏移超过车宽一半 → 无碰撞
        ActorPose ego{5.0, 3.0, 0.0};
        ActorPose target{4.0, 0.0, 0.0};
        QVERIFY(!NcapEvaluator::detectsCollision(ego, 4.5, target, 1.8, 1.0));
    }

    void noCollisionWhenGapAhead()
    {
        // 主车前端尚未到达目标后端 → 无碰撞
        ActorPose ego{0.0, 0.0, 0.0};
        ActorPose target{50.0, 0.0, 0.0};
        QVERIFY(!NcapEvaluator::detectsCollision(ego, 4.5, target, 1.8, 1.0));
    }

    // ---- CCRs 评定：避免碰撞 = PASS ----
    void ccrsAvoidedCollisionPasses()
    {
        ScenarioConfig cfg;
        cfg.type = QStringLiteral("AEB");
        cfg.subgroup = QStringLiteral("CCRs");
        cfg.noCollisionRequired = true;

        TestResult r;
        r.collisionOccurred = false;
        r.minTtcSec = 1.5;
        r.completedNormally = true;

        const NcapVerdict v = NcapEvaluator::evaluate(cfg, r);
        QCOMPARE(v.verdict, NcapOutcome::Pass);
        QVERIFY(v.scorePercent > 0.0);
    }

    // ---- CCRs 评定：发生碰撞 = FAIL ----
    void ccrsCollisionFails()
    {
        ScenarioConfig cfg;
        cfg.type = QStringLiteral("AEB");
        cfg.subgroup = QStringLiteral("CCRs");
        cfg.noCollisionRequired = true;

        TestResult r;
        r.collisionOccurred = true;
        r.speedAtImpactMps = 10.0;
        r.egoTargetSpeed = 13.89;   // 50km/h
        r.completedNormally = true;

        const NcapVerdict v = NcapEvaluator::evaluate(cfg, r);
        QCOMPARE(v.verdict, NcapOutcome::Fail);
    }

    // ---- CCRs 部分制动：碰撞但速度大幅降低 → 部分得分 ----
    void ccrsReducedSpeedCollisionScoresPartially()
    {
        ScenarioConfig cfg;
        cfg.type = QStringLiteral("AEB");
        cfg.subgroup = QStringLiteral("CCRs");
        cfg.noCollisionRequired = true;

        TestResult r;
        r.collisionOccurred = true;
        r.speedAtImpactMps = 3.0;       // 从 50 降到 ~11km/h
        r.egoTargetSpeed = 13.89;
        r.completedNormally = true;

        const NcapVerdict reduced = NcapEvaluator::evaluate(cfg, r);

        r.speedAtImpactMps = 13.89;     // 全速碰撞
        const NcapVerdict full = NcapEvaluator::evaluate(cfg, r);

        QVERIFY(reduced.scorePercent > full.scorePercent);
        QVERIFY(reduced.scorePercent > 0.0);
    }

    // ---- 超时未完成也判定 ----
    void incompleteRunFails()
    {
        ScenarioConfig cfg;
        cfg.type = QStringLiteral("AEB");
        cfg.subgroup = QStringLiteral("CCRs");
        cfg.noCollisionRequired = true;

        TestResult r;
        r.collisionOccurred = false;
        r.completedNormally = false;    // 急停/异常中断

        QCOMPARE(NcapEvaluator::evaluate(cfg, r).verdict, NcapOutcome::Fail);
    }

    // ---- TTC 合格判据 ----
    void passesByTtcCriterion()
    {
        ScenarioConfig cfg;
        cfg.type = QStringLiteral("AEB");
        cfg.passTtcSec = 2.0;       // 要求 TTC ≥ 2s
        cfg.noCollisionRequired = false;

        TestResult r;
        r.collisionOccurred = false;
        r.minTtcSec = 2.5;          // 实际最小 TTC 2.5s > 2s
        r.completedNormally = true;

        QCOMPARE(NcapEvaluator::evaluate(cfg, r).verdict, NcapOutcome::Pass);
    }

    void failsByTtcCriterion()
    {
        ScenarioConfig cfg;
        cfg.type = QStringLiteral("AEB");
        cfg.passTtcSec = 2.0;
        cfg.noCollisionRequired = false;

        TestResult r;
        r.collisionOccurred = false;
        r.minTtcSec = 1.0;          // 1s < 2s
        r.completedNormally = true;

        QCOMPARE(NcapEvaluator::evaluate(cfg, r).verdict, NcapOutcome::Fail);
    }

    // ---- 速度网格生成（CCRs：10-50 步长 5）----
    void generatesCcrsSpeedGrid()
    {
        const QList<int> speeds = NcapEvaluator::speedGrid(QStringLiteral("CCRs"));
        QVERIFY(speeds.contains(10));
        QVERIFY(speeds.contains(50));
        QVERIFY(!speeds.contains(55));    // 55 属于 FCW 范围
        QCOMPARE(speeds.first(), 10);
        QCOMPARE(speeds.last(), 50);
        QCOMPARE(speeds.size(), 9);       // 10,15,...,50 = 9 个点
    }

    // ---- 评分文本 ----
    void verdictTextIsLocalized()
    {
        NcapVerdict v;
        v.verdict = NcapOutcome::Pass;
        QCOMPARE(v.outcomeText(), QStringLiteral("通过"));
        v.verdict = NcapOutcome::Fail;
        QCOMPARE(v.outcomeText(), QStringLiteral("未通过"));
    }

    // ---- 重叠率计算（横向重叠 / 车宽）----
    void computesOverlapRatio()
    {
        // 完全对正（同 y）→ 100% 重叠
        QCOMPARE(NcapEvaluator::lateralOverlap(0.0, 0.0, 1.8), 1.0);
        // 偏移半个车宽 → 50%
        QVERIFY(qAbs(NcapEvaluator::lateralOverlap(0.0, 0.9, 1.8) - 0.5) < 1e-9);
        // 偏移超过车宽 → 0%
        QCOMPARE(NcapEvaluator::lateralOverlap(0.0, 2.0, 1.8), 0.0);
    }
};

QTEST_GUILESS_MAIN(TestNcapEvaluator)
#include "TestNcapEvaluator.moc"
