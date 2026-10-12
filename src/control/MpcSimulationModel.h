// =============================================================================
// 功能：MPC 调试页的 Qt 数据模型；输入参数和启停操作，输出曲线/指标/CSV。
// 闭环在专用工作线程中执行；仅使用模拟机器人，不绑定真实硬件。
// =============================================================================
#pragma once
#include <QObject>
#include <QThread>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

namespace lateral {
class MpcSimulationWorker;
class MpcSimulationModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(int phase READ phase NOTIFY dataChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY dataChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY dataChanged)
    Q_PROPERTY(QString message READ message NOTIFY dataChanged)
    Q_PROPERTY(bool safetyBlocked READ safetyBlocked NOTIFY dataChanged)
    Q_PROPERTY(QVariantMap metrics READ metrics NOTIFY dataChanged)
    Q_PROPERTY(QVariantMap defaults READ defaults CONSTANT)
    Q_PROPERTY(QVariantList referencePath READ referencePath NOTIFY referenceChanged)
    Q_PROPERTY(QVariantList chartSamples READ chartSamples NOTIFY dataChanged)
    Q_PROPERTY(int sampleCount READ sampleCount NOTIFY dataChanged)
    Q_PROPERTY(QUrl defaultExportUrl READ defaultExportUrl CONSTANT)
public:
    enum Phase { Ready = 0, Running, Paused, Completed, Stopped, Fault };
    Q_ENUM(Phase)
    explicit MpcSimulationModel(QObject *parent = nullptr);
    ~MpcSimulationModel() override;
    int phase() const { return m_phase; }
    bool busy() const { return m_phase == Running || m_phase == Paused; }
    QString statusText() const;
    QString message() const { return m_message; }
    bool safetyBlocked() const { return m_safetyBlocked; }
    QVariantMap metrics() const { return m_metrics; }
    QVariantMap defaults() const;
    QVariantList referencePath() const { return m_reference; }
    QVariantList chartSamples() const { return m_chart; }
    int sampleCount() const { return m_samples.size(); }
    QUrl defaultExportUrl() const;
    Q_INVOKABLE bool start(const QVariantMap &parameters);
    Q_INVOKABLE bool preview(const QVariantMap &parameters);
    Q_INVOKABLE void pauseResume();
    Q_INVOKABLE void stop();
    Q_INVOKABLE bool exportCsv(const QUrl &url);
public slots:
    void setSafetyEmergencyStop(bool active);
signals:
    void dataChanged();
    void referenceChanged();
    void beginRequested(quint64 token, const QVariantMap &parameters);
    void pauseRequested();
    void stopRequested();
    void emergencyRequested(bool active);
private:
    void acceptSnapshot(const QVariantMap &snapshot);
    void clearRun();
    QThread m_thread;
    MpcSimulationWorker *m_worker = nullptr;
    quint64 m_token = 0;
    int m_phase = Ready;
    bool m_safetyBlocked = false;
    QString m_message;
    QVariantMap m_metrics;
    QVariantList m_reference, m_chart, m_samples;
};
} // namespace lateral
