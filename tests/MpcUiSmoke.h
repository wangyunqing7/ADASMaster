// =============================================================================
// 功能：从实际 QML 按钮启动圆弧仿真，校验界面状态、导出 CSV 和窗口截图。
// 仅在 --mpc-ui-smoke <png> 参数下执行，不影响正常启动。
// =============================================================================
#pragma once
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QQuickWindow>
#include <QSaveFile>
#include <QTimer>
#include <memory>
#include "src/app/AppController.h"

namespace MpcUiSmoke {
inline QMutex logMutex;
inline QStringList warnings;
inline void logger(QtMsgType, const QMessageLogContext &, const QString &message) {
    QMutexLocker locker(&logMutex);
    if (message.contains("MpcPage") || message.contains("MpcTrajectoryView") || message.contains("MpcLineChart"))
        warnings.push_back(message);
}
inline void run(QObject *root, AppController *controller, const QString &pngPath) {
    auto *model = controller->mpcSimulation();
    auto finished = std::make_shared<bool>(false);
    const auto finish = [root, model, pngPath, finished](const QString &error) {
        if (*finished) return;
        *finished = true;
        QString failure = error;
        auto *window = qobject_cast<QQuickWindow *>(root);
        const QFileInfo target(pngPath);
        QDir().mkpath(target.absolutePath());
        if (failure.isEmpty() && (!window || !window->grabWindow().save(pngPath)))
            failure = QStringLiteral("Window screenshot failed");
        const QString csv = target.absolutePath() + QLatin1String("/mpc-ui-smoke.csv");
        if (failure.isEmpty()) {
            auto *dialog = root->findChild<QObject *>(QStringLiteral("mpcExportDialog"));
            if (!dialog || !dialog->setProperty("selectedFile", QUrl::fromLocalFile(csv))
                || !QMetaObject::invokeMethod(dialog, "accepted")
                || !model->message().startsWith(QStringLiteral("已导出：")))
                failure = QStringLiteral("QML CSV export binding failed");
        }
        QStringList logs;
        { QMutexLocker locker(&logMutex); logs = warnings; }
        if (failure.isEmpty() && !logs.isEmpty()) failure = logs.join('\n');
        QSaveFile report(target.absolutePath() + QLatin1String("/mpc-ui-smoke.json"));
        if (report.open(QIODevice::WriteOnly)) {
            report.write(QJsonDocument(QJsonObject{{"passed", failure.isEmpty()}, {"error", failure},
                {"samples", model->sampleCount()}, {"phase", model->phase()},
                {"screenshot", pngPath}, {"csv", csv}}).toJson());
            report.commit();
        }
        QCoreApplication::exit(failure.isEmpty() ? 0 : 20);
    };
    // 使用项目测试账号正常登录，不依赖工作区中的临时登录界面改动。
    if (!controller->authManager()->isLoggedIn()
        && !controller->authManager()->login(QStringLiteral("admin"), QStringLiteral("admin123"))) {
        finish(QStringLiteral("MPC UI test account login failed"));
        return;
    }
    QObject::connect(model, &lateral::MpcSimulationModel::dataChanged, root, [model, root, finish] {
        if (model->phase() == lateral::MpcSimulationModel::Fault) { finish(model->message()); return; }
        if (model->phase() != lateral::MpcSimulationModel::Completed) return;
        auto *start = root->findChild<QObject *>(QStringLiteral("mpcStartButton"));
        auto *pause = root->findChild<QObject *>(QStringLiteral("mpcPauseButton"));
        auto *exportButton = root->findChild<QObject *>(QStringLiteral("mpcExportButton"));
        if (!start || !pause || !exportButton || !start->property("visible").toBool()
            || !start->property("enabled").toBool()
            || pause->property("enabled").toBool() || !exportButton->property("enabled").toBool()
            || model->sampleCount() != 160) { finish(QStringLiteral("MPC button state or sample count incorrect")); return; }
        QTimer::singleShot(200, root, [finish] { finish({}); });
    });
    QTimer::singleShot(250, root, [root, model, finish] {
        auto *scenario = root->findChild<QObject *>(QStringLiteral("mpcScenarioInput"));
        auto *duration = root->findChild<QObject *>(QStringLiteral("mpcDurationInput"));
        auto *playback = root->findChild<QObject *>(QStringLiteral("mpcPlaybackInput"));
        auto *start = root->findChild<QObject *>(QStringLiteral("mpcStartButton"));
        if (!scenario || !duration || !playback || !start) { finish(QStringLiteral("MPC page controls missing")); return; }
        scenario->setProperty("currentIndex", 1);
        duration->setProperty("text", QStringLiteral("8"));
        playback->setProperty("currentIndex", 2);
        if (!QMetaObject::invokeMethod(start, "clicked") || !model->busy())
            finish(QStringLiteral("QML start button did not start the simulation"));
    });
    QTimer::singleShot(10000, root, [finish] { finish(QStringLiteral("MPC UI smoke test timed out")); });
}
} // namespace MpcUiSmoke
