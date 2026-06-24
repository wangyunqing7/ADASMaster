#pragma once

// =============================================================================
// StateManager.h
// 功能：系统实时状态的中央仓库。统一保存并对外暴露：安全状态、场景状态、
//       主车状态、目标物状态、RTK 状态、相对距离/TTC 等指标。
// 输入：由 ScenarioEngine 在运行时推进；由 SafetyManager 同步安全状态；
//       由 AppController 的统一时钟驱动模拟量更新。
// 输出：QML 通过 Q_PROPERTY 绑定，实时显示运行监控页面的所有数值。
// 说明：本类只保存"状态"，不做控制决策。决策由 ScenarioEngine/SafetyManager 完成。
// =============================================================================

#include <QObject>
#include <QVariantList>
#include "../safety/SafetyStatus.h"
#include "../scenario/ScenarioStatus.h"

class StateManager : public QObject
{
    Q_OBJECT
    // ---- 安全 ----
    Q_PROPERTY(int  safetyLevel        READ safetyLevelInt    NOTIFY stateChanged)
    Q_PROPERTY(QString safetyText      READ safetyText        NOTIFY stateChanged)
    // ---- 测试流程 ----
    Q_PROPERTY(int  testState          READ testStateInt      NOTIFY stateChanged)
    Q_PROPERTY(QString testStateText   READ testStateText     NOTIFY stateChanged)
    Q_PROPERTY(QString scenarioName    READ scenarioName      NOTIFY stateChanged)
    Q_PROPERTY(QString scenarioType    READ scenarioType      NOTIFY stateChanged)
    Q_PROPERTY(double progress         READ progress          NOTIFY stateChanged)
    Q_PROPERTY(int    countdown        READ countdown         NOTIFY stateChanged)
    Q_PROPERTY(QString resultText      READ resultText        NOTIFY stateChanged)
    // ---- RTK ----
    Q_PROPERTY(QString rtkMode         READ rtkMode           NOTIFY stateChanged)
    Q_PROPERTY(int     satelliteCount  READ satelliteCount    NOTIFY stateChanged)
    Q_PROPERTY(double  positionAccuracy READ positionAccuracy NOTIFY stateChanged)
    // ---- 主车 ----
    Q_PROPERTY(double egoX     READ egoX     NOTIFY stateChanged)
    Q_PROPERTY(double egoY     READ egoY     NOTIFY stateChanged)
    Q_PROPERTY(double egoYaw   READ egoYaw   NOTIFY stateChanged)
    Q_PROPERTY(double egoSpeed READ egoSpeed NOTIFY stateChanged)
    // ---- 目标物 ----
    Q_PROPERTY(double tgtX     READ tgtX     NOTIFY stateChanged)
    Q_PROPERTY(double tgtY     READ tgtY     NOTIFY stateChanged)
    Q_PROPERTY(double tgtYaw   READ tgtYaw   NOTIFY stateChanged)
    Q_PROPERTY(double tgtSpeed READ tgtSpeed NOTIFY stateChanged)
    Q_PROPERTY(QString targetType READ targetType NOTIFY stateChanged)
    // ---- 相对指标 ----
    Q_PROPERTY(double distance     READ distance     NOTIFY stateChanged)
    Q_PROPERTY(double relativeSpeed READ relativeSpeed NOTIFY stateChanged)
    Q_PROPERTY(double ttc          READ ttc          NOTIFY stateChanged)
    Q_PROPERTY(bool   collisionRisk READ collisionRisk NOTIFY stateChanged)

    // ---- 轨迹可视化（实时地图绘制）----
    // egoTrail/targetTrail：运行中累积的主车/目标物位姿历史 [{x,y}, ...]
    // targetWaypoints：当前工况的目标物预设航点（"应该如何走"），[{x,y}, ...]
    Q_PROPERTY(QVariantList egoTrail        READ egoTrail        NOTIFY stateChanged)
    Q_PROPERTY(QVariantList targetTrail     READ targetTrail     NOTIFY stateChanged)
    Q_PROPERTY(QVariantList targetWaypoints READ targetWaypoints NOTIFY stateChanged)
    // ---- NCAP 评估结果（运行结束给出 PASS/FAIL + 评分）----
    Q_PROPERTY(QString verdictText  READ verdictText  NOTIFY stateChanged)
    Q_PROPERTY(int     verdictCode  READ verdictCode  NOTIFY stateChanged)
    Q_PROPERTY(double  scorePercent READ scorePercent NOTIFY stateChanged)
    Q_PROPERTY(QString verdictDetail READ verdictDetail NOTIFY stateChanged)

public:
    explicit StateManager(QObject *parent = nullptr);

    // 读属性
    int safetyLevelInt() const { return static_cast<int>(m_safetyLevel); }
    QString safetyText() const;
    int testStateInt() const { return static_cast<int>(m_testState); }
    QString testStateText() const;
    QString scenarioName() const { return m_scenarioName; }
    QString scenarioType() const { return m_scenarioType; }
    double progress() const { return m_progress; }
    int countdown() const { return m_countdown; }
    QString resultText() const { return m_result; }
    QString rtkMode() const { return m_rtkMode; }
    int satelliteCount() const { return m_satelliteCount; }
    double positionAccuracy() const { return m_positionAccuracy; }
    double egoX() const { return m_egoX; }
    double egoY() const { return m_egoY; }
    double egoYaw() const { return m_egoYaw; }
    double egoSpeed() const { return m_egoSpeed; }
    double tgtX() const { return m_tgtX; }
    double tgtY() const { return m_tgtY; }
    double tgtYaw() const { return m_tgtYaw; }
    double tgtSpeed() const { return m_tgtSpeed; }
    QString targetType() const { return m_targetType; }
    double distance() const { return m_distance; }
    double relativeSpeed() const { return m_relativeSpeed; }
    double ttc() const { return m_ttc; }
    bool collisionRisk() const { return m_collisionRisk; }
    // 轨迹可视化
    QVariantList egoTrail() const { return m_egoTrail; }
    QVariantList targetTrail() const { return m_targetTrail; }
    QVariantList targetWaypoints() const { return m_targetWaypoints; }
    // NCAP 评估结果
    QString verdictText() const { return m_verdictText; }
    int verdictCode() const { return m_verdictCode; }       // 0=Pass 1=Fail 2=Pending
    double scorePercent() const { return m_scorePercent; }
    QString verdictDetail() const { return m_verdictDetail; }

    // 写接口（由 ScenarioEngine / SafetyManager / AppController 调用）
    void setSafetyLevel(SafetyLevel l);
    void setTestState(TestState s);
    void setScenario(const QString &name, const QString &type);
    void setProgress(double p);
    void setCountdown(int c);
    void setResult(const QString &r);
    void setRtk(const QString &mode, int sats, double acc);
    void setEgo(double x, double y, double yaw, double speed);
    void setTarget(double x, double y, double yaw, double speed, const QString &type);
    void setRelative(double distance, double relSpeed, double ttc, bool risk);
    // 追加一帧位姿到主车/目标物轨迹历史（带最大长度上限，超出丢弃最旧）
    void appendTrail(double egoX, double egoY, double tgtX, double tgtY);
    // 设置当前工况的目标物预设航点（"应该如何走"）
    void setTargetWaypoints(const QVariantList &waypoints);
    // 清空轨迹历史（新测试开始时）
    void clearTrail();
    // 设置 NCAP 评估结果（由 ScenarioEngine 在测试结束时调用）
    void setVerdict(const QString &text, int code, double score, const QString &detail);

signals:
    void stateChanged();

private:
    SafetyLevel m_safetyLevel = SafetyLevel::Normal;
    TestState   m_testState   = TestState::Idle;

    QString m_scenarioName = QStringLiteral("未加载");
    QString m_scenarioType;
    double  m_progress = 0.0;
    int     m_countdown = 0;
    QString m_result;

    QString m_rtkMode = QStringLiteral("Fixed");
    int     m_satelliteCount = 18;
    double  m_positionAccuracy = 0.02;

    double m_egoX = 0, m_egoY = 0, m_egoYaw = 0, m_egoSpeed = 0;
    double m_tgtX = 0, m_tgtY = 0, m_tgtYaw = 0, m_tgtSpeed = 0;
    QString m_targetType = QStringLiteral("车");

    double m_distance = 0;
    double m_relativeSpeed = 0;
    double m_ttc = 999.0;
    bool   m_collisionRisk = false;

    // 轨迹可视化
    QVariantList m_egoTrail;           // 主车位姿历史 [{x,y}, ...]
    QVariantList m_targetTrail;        // 目标物位姿历史
    QVariantList m_targetWaypoints;    // 当前工况目标物预设航点
    static constexpr int MaxTrailPoints = 2000;  // 轨迹最大点数（防内存爆）

    // NCAP 评估结果
    QString m_verdictText;
    int     m_verdictCode = 2;       // Pending（未评定）
    double  m_scorePercent = 0.0;
    QString m_verdictDetail;
};
