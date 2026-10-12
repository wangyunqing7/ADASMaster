// =============================================================================
// 功能：参数验证、专用线程仿真、排队发布有界曲线数据及原子 CSV 导出。
// 仿真时间按固定步长推进，暂停不影响控制器的单调仿真时钟。
// =============================================================================
#include "MpcSimulationModel.h"
#include "SimulationPlant.h"
#include "SteeringRobotController.h"
#include <QDir>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>
#include <memory>

namespace lateral {
namespace {
struct Parameters {
    VehicleParameters vehicle;
    MpcSettings settings;
    QString kind = QStringLiteral("lane-change");
    double speedMps = 12.0, offsetM = 0.4, durationS = 15.0;
    double radiusM = 80.0, laneLengthM = 60.0, laneWidthM = 3.5;
    double playbackRate = 1.0;
};
QString parse(const QVariantMap &map, Parameters *p) {
    p->kind = map.value(QStringLiteral("scenario"), p->kind).toString();
    if (p->kind != QLatin1String("straight") && p->kind != QLatin1String("circle")
        && p->kind != QLatin1String("lane-change")) return QStringLiteral("请选择有效的仿真场景");
    QString error;
    const auto number = [&](const char *key, double fallback, double min, double max) {
        bool ok = false;
        const double value = map.value(QString::fromLatin1(key), fallback).toDouble(&ok);
        if (!ok || !std::isfinite(value) || value < min || value > max)
            error = QStringLiteral("参数 %1 需在 %2–%3 之间").arg(QString::fromLatin1(key)).arg(min).arg(max);
        return value;
    };
    p->speedMps = number("speedKph", 43.2, 1.08, 126.0) / 3.6;
    p->offsetM = number("initialOffsetM", 0.4, -1.8, 1.8);
    p->durationS = number("durationS", 15.0, 0.5, 60.0);
    p->radiusM = number("radiusM", 80.0, 10.0, 300.0);
    p->laneLengthM = number("laneLengthM", 60.0, 20.0, 200.0);
    p->laneWidthM = number("laneWidthM", 3.5, 0.5, 6.0);
    p->playbackRate = number("playbackRate", 1.0, 0.25, 5.0);
    p->vehicle.steeringRatio = number("steeringRatio", 16.0, 4.0, 30.0);
    p->vehicle.actuatorTimeConstantS = number("actuatorLagMs", 80.0, 20.0, 500.0) / 1000.0;
    p->settings.sampleTimeS = number("sampleTimeMs", 50.0, 20.0, 100.0) / 1000.0;
    const double horizon = number("horizon", 30.0, 5.0, 60.0);
    if (std::isfinite(horizon) && std::floor(horizon) != horizon) error = QStringLiteral("预测步数必须为整数");
    if (!error.isEmpty()) return error;
    p->settings.horizon = static_cast<int>(horizon);
    p->settings.stateWeights[0] = number("lateralWeight", 40.0, 1.0, 200.0);
    p->settings.stateWeights[1] = number("headingWeight", 12.0, 1.0, 100.0);
    p->settings.inputRateWeight = number("rateWeight", 20.0, 1.0, 200.0);
    p->settings.maximumLateralAccelerationMps2 = number("maxLateralAcceleration", 3.0, 0.5, 6.0);
    p->settings.maximumRobotRateRadS = radians(number("maxRobotRateDegS", 600.0, 60.0, 1000.0));
    p->settings.solverTimeLimitMs = p->settings.sampleTimeS * 1000.0 - 5.0;
    if (!error.isEmpty()) return error;
    const QString issue = validateConfiguration(p->vehicle, p->settings);
    if (!issue.isEmpty()) return issue;
    if (p->kind == QLatin1String("circle")
        && p->speedMps * p->speedMps / p->radiusM > p->settings.maximumLateralAccelerationMps2)
        return QStringLiteral("圆弧速度过高：请降低车速或增大半径");
    if (p->kind == QLatin1String("circle")
        && p->speedMps * p->durationS + 5.0 > std::min(350.0, p->radiusM * 1.8 * pi))
        return QStringLiteral("圆弧轨迹不足以覆盖仿真时长，请缩短时长");
    if (p->speedMps * p->durationS + 5.0 > 600.0)
        return QStringLiteral("仿真距离超过 600 m，请缩短时长");
    return {};
}
std::vector<ReferencePoint> points(const Parameters &p) {
    const double length = std::min(600.0, p.speedMps * (p.durationS
        + p.settings.sampleTimeS * p.settings.horizon) + 10.0);
    return simulation::makePath(p.kind, p.radiusM, p.laneLengthM, p.laneWidthM, length);
}
QVariantMap emptyMetrics() {
    return {{"timeS", 0.0}, {"progress", 0.0}, {"rmsM", 0.0}, {"peakM", 0.0},
            {"errorM", 0.0}, {"targetDeg", 0.0}, {"actualDeg", 0.0}, {"solveMs", 0.0},
            {"meanSolveMs", 0.0}, {"maxSolveMs", 0.0}, {"xM", 0.0}, {"yM", 0.0}, {"yawRad", 0.0}};
}
} // namespace

class MpcSimulationWorker : public QObject {
    Q_OBJECT
public:
    explicit MpcSimulationWorker() {
        m_timer = new QTimer(this);
        m_timer->setTimerType(Qt::PreciseTimer);
        connect(m_timer, &QTimer::timeout, this, &MpcSimulationWorker::tick);
    }
public slots:
    void begin(quint64 token, const QVariantMap &map) {
        shutdown();
        m_token = token;
        m_parameters = {};
        const QString issue = parse(map, &m_parameters);
        if (!issue.isEmpty() || m_emergency) { publish(MpcSimulationModel::Fault, issue.isEmpty() ? QStringLiteral("系统急停已激活") : issue); return; }
        m_robot = std::make_unique<simulation::Robot>();
        m_controller = std::make_unique<SteeringRobotController>();
        m_plant = {};
        m_plant.vehicle = m_parameters.vehicle;
        m_plant.state.longitudinalSpeedMps = m_parameters.speedMps;
        m_plant.state.yM = m_parameters.offsetM;
        m_plant.state.timestampMs = 0;
        m_plant.state.localizationValid = true;
        m_step = 0; m_squareError = 0.0; m_peak = 0.0; m_sumSolve = 0.0; m_maxSolve = 0.0;
        m_metrics = emptyMetrics();
        QString error;
        if (!m_controller->configure(m_parameters.vehicle, m_parameters.settings, &error)
            || !m_controller->setReferencePath(points(m_parameters), &error)
            || !m_controller->setRobot(m_robot.get())) { publish(MpcSimulationModel::Fault, error); return; }
        m_controller->updateVehicleState(m_plant.state);
        m_robot->publish(0, m_plant.robotAngleRad());
        if (!m_controller->arm(0) || !m_controller->start(0)) { publish(MpcSimulationModel::Fault, m_controller->faultReason()); return; }
        publish(MpcSimulationModel::Running);
        m_timer->start(std::max(1, static_cast<int>(std::lround(m_parameters.settings.sampleTimeS
                                                               * 1000.0 / m_parameters.playbackRate))));
    }
    void pauseResume() {
        if (!m_controller) return;
        if (m_phase == MpcSimulationModel::Running) {
            m_timer->stop(); publish(MpcSimulationModel::Paused);
        } else if (m_phase == MpcSimulationModel::Paused) {
            m_timer->start(); publish(MpcSimulationModel::Running);
        }
    }
    void stop() {
        if (m_phase != MpcSimulationModel::Running && m_phase != MpcSimulationModel::Paused) return;
        shutdown(); publish(MpcSimulationModel::Stopped);
    }
    void emergency(bool active) {
        m_emergency = active;
        if (active && (m_phase == MpcSimulationModel::Running || m_phase == MpcSimulationModel::Paused)) {
            m_timer->stop();
            if (m_controller) m_controller->setExternalEmergencyStop(true);
            publish(MpcSimulationModel::Fault, QStringLiteral("系统急停：仿真已停止"));
        }
    }
    void shutdown() {
        m_timer->stop();
        if (m_controller) m_controller->stop();
        m_controller.reset();
        m_robot.reset();
    }
signals:
    void snapshot(const QVariantMap &value);
private:
    void publish(int phase, const QString &message = {}, const QVariantMap &sample = {}) {
        m_phase = phase;
        emit snapshot({{"token", QVariant::fromValue(m_token)}, {"phase", phase},
                       {"message", message}, {"metrics", m_metrics}, {"sample", sample}});
    }
    void tick() {
        const auto &p = m_parameters;
        const qint64 now = static_cast<qint64>(std::llround(m_step * p.settings.sampleTimeS * 1000.0));
        m_plant.state.timestampMs = now;
        m_controller->updateVehicleState(m_plant.state);
        m_robot->publish(now, m_plant.robotAngleRad());
        m_controller->processOnce(now);
        if (m_controller->controlState() != SteeringRobotController::State::Running) {
            m_timer->stop(); publish(MpcSimulationModel::Fault, m_controller->faultReason()); return;
        }
        const auto &r = m_controller->lastResult();
        const double t = now / 1000.0;
        m_squareError += r.lateralErrorM * r.lateralErrorM;
        m_peak = std::max(m_peak, std::abs(r.lateralErrorM));
        m_sumSolve += r.solveTimeMs; m_maxSolve = std::max(m_maxSolve, r.solveTimeMs);
        const QVariantMap sample{{"timeS", t}, {"xM", m_plant.state.xM}, {"yM", m_plant.state.yM},
            {"yawRad", m_plant.state.yawRad}, {"errorM", r.lateralErrorM},
            {"targetDeg", degrees(r.robotTargetRad)}, {"actualDeg", degrees(m_plant.robotAngleRad())},
            {"solveMs", r.solveTimeMs}, {"iterations", r.iterations}};
        m_metrics = sample;
        m_metrics["rmsM"] = std::sqrt(m_squareError / (m_step + 1));
        m_metrics["peakM"] = m_peak;
        m_metrics["meanSolveMs"] = m_sumSolve / (m_step + 1);
        m_metrics["maxSolveMs"] = m_maxSolve;
        m_plant.advance(r.roadWheelTargetRad, p.settings.sampleTimeS, p.settings.maximumRobotRateRadS,
                        p.settings.dynamicModelMinSpeedMps);
        ++m_step;
        const double elapsed = std::min(p.durationS, m_step * p.settings.sampleTimeS);
        m_metrics["timeS"] = elapsed;
        m_metrics["progress"] = elapsed / p.durationS;
        if (elapsed >= p.durationS - 1e-9) {
            m_timer->stop(); m_controller->stop();
            publish(MpcSimulationModel::Completed, {}, sample);
        } else publish(MpcSimulationModel::Running, {}, sample);
    }
    QTimer *m_timer = nullptr;
    quint64 m_token = 0;
    int m_step = 0, m_phase = MpcSimulationModel::Ready;
    bool m_emergency = false;
    double m_squareError = 0.0, m_peak = 0.0, m_sumSolve = 0.0, m_maxSolve = 0.0;
    QVariantMap m_metrics = emptyMetrics();
    Parameters m_parameters;
    simulation::Plant m_plant;
    std::unique_ptr<simulation::Robot> m_robot;
    std::unique_ptr<SteeringRobotController> m_controller;
};

MpcSimulationModel::MpcSimulationModel(QObject *parent) : QObject(parent), m_metrics(emptyMetrics()) {
    m_worker = new MpcSimulationWorker;
    m_worker->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(this, &MpcSimulationModel::beginRequested, m_worker, &MpcSimulationWorker::begin);
    connect(this, &MpcSimulationModel::pauseRequested, m_worker, &MpcSimulationWorker::pauseResume);
    connect(this, &MpcSimulationModel::stopRequested, m_worker, &MpcSimulationWorker::stop);
    connect(this, &MpcSimulationModel::emergencyRequested, m_worker, &MpcSimulationWorker::emergency);
    connect(m_worker, &MpcSimulationWorker::snapshot, this, &MpcSimulationModel::acceptSnapshot);
    m_thread.setObjectName(QStringLiteral("MpcSimulation"));
    m_thread.start();
    preview(defaults());
}
MpcSimulationModel::~MpcSimulationModel() {
    QMetaObject::invokeMethod(m_worker, &MpcSimulationWorker::shutdown, Qt::BlockingQueuedConnection);
    m_thread.quit();
    m_thread.wait();
}
QVariantMap MpcSimulationModel::defaults() const {
    return {{"scenario", "lane-change"}, {"speedKph", 43.2}, {"initialOffsetM", 0.4},
        {"durationS", 15.0}, {"radiusM", 80.0}, {"laneLengthM", 60.0}, {"laneWidthM", 3.5},
        {"playbackRate", 1.0}, {"steeringRatio", 16.0}, {"actuatorLagMs", 80.0},
        {"sampleTimeMs", 50.0}, {"horizon", 30}, {"lateralWeight", 40.0},
        {"headingWeight", 12.0}, {"rateWeight", 20.0},
        {"maxLateralAcceleration", 3.0}, {"maxRobotRateDegS", 600.0}};
}
QString MpcSimulationModel::statusText() const {
    switch (m_phase) {
    case Running: return QStringLiteral("仿真运行中");
    case Paused: return QStringLiteral("已暂停");
    case Completed: return QStringLiteral("仿真完成");
    case Stopped: return QStringLiteral("已停止");
    case Fault: return QStringLiteral("仿真故障");
    default: return QStringLiteral("准备就绪");
    }
}
QUrl MpcSimulationModel::defaultExportUrl() const {
    return QUrl::fromLocalFile(QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
        .filePath(QStringLiteral("ADASMaster_MPC.csv")));
}
void MpcSimulationModel::clearRun() {
    m_samples.clear(); m_chart.clear(); m_metrics = emptyMetrics(); m_message.clear();
}
bool MpcSimulationModel::preview(const QVariantMap &map) {
    if (busy()) return false;
    Parameters p;
    const QString issue = parse(map, &p);
    if (!issue.isEmpty()) { m_message = issue; emit dataChanged(); return false; }
    ++m_token; clearRun(); m_phase = Ready; m_reference.clear();
    const auto path = points(p);
    const size_t stride = std::max<size_t>(1, (path.size() + 799) / 800);
    for (size_t i = 0; i < path.size(); i += stride)
        m_reference.push_back(QVariantMap{{"xM", path[i].xM}, {"yM", path[i].yM}});
    if (!path.empty()) m_reference.push_back(QVariantMap{{"xM", path.back().xM}, {"yM", path.back().yM}});
    m_metrics["yM"] = p.offsetM;
    emit referenceChanged(); emit dataChanged();
    return true;
}
bool MpcSimulationModel::start(const QVariantMap &parameters) {
    if (busy()) return false;
    if (m_safetyBlocked) { m_message = QStringLiteral("系统急停已激活，请先解除急停并复位"); emit dataChanged(); return false; }
    if (!preview(parameters)) return false;
    m_phase = Running; emit dataChanged();
    emit beginRequested(m_token, parameters);
    return true;
}
void MpcSimulationModel::pauseResume() { if (busy()) emit pauseRequested(); }
void MpcSimulationModel::stop() { if (busy()) emit stopRequested(); }
void MpcSimulationModel::setSafetyEmergencyStop(bool active) {
    m_safetyBlocked = active;
    emit emergencyRequested(active);
    emit dataChanged();
}
void MpcSimulationModel::acceptSnapshot(const QVariantMap &snapshot) {
    if (snapshot.value("token").toULongLong() != m_token) return;
    m_phase = snapshot.value("phase").toInt();
    m_message = snapshot.value("message").toString();
    m_metrics = snapshot.value("metrics").toMap();
    const auto sample = snapshot.value("sample").toMap();
    if (!sample.isEmpty()) {
        m_samples.push_back(sample);
        m_chart.clear();
        const qsizetype stride = std::max<qsizetype>(1, (m_samples.size() + 599) / 600);
        for (qsizetype i = 0; i < m_samples.size(); i += stride) m_chart.push_back(m_samples[i]);
        if ((m_samples.size() - 1) % stride != 0) m_chart.push_back(m_samples.back());
    }
    emit dataChanged();
}
bool MpcSimulationModel::exportCsv(const QUrl &url) {
    if (!url.isLocalFile() || url.toLocalFile().isEmpty() || m_samples.isEmpty()) {
        m_message = QStringLiteral("请先运行仿真，再选择本地 CSV 文件"); emit dataChanged(); return false;
    }
    QSaveFile file(url.toLocalFile());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_message = file.errorString(); emit dataChanged(); return false;
    }
    QTextStream out(&file);
    out.setRealNumberPrecision(12);
    out << "time_s,x_m,y_m,yaw_rad,lateral_error_m,robot_target_deg,robot_actual_deg,solve_ms,iterations\n";
    const QStringList keys{"timeS", "xM", "yM", "yawRad", "errorM", "targetDeg", "actualDeg", "solveMs", "iterations"};
    for (const auto &value : m_samples) {
        const auto row = value.toMap();
        for (int i = 0; i < keys.size(); ++i) {
            if (i) out << ',';
            out << row.value(keys[i]).toDouble();
        }
        out << '\n';
    }
    out.flush();
    if (!file.commit()) { m_message = file.errorString(); emit dataChanged(); return false; }
    m_message = QStringLiteral("已导出：%1").arg(url.toLocalFile());
    emit dataChanged(); return true;
}
} // namespace lateral
#include "MpcSimulationModel.moc"
