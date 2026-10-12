// =============================================================================
// main.cpp
// 功能：ADAS 场地测试主控系统入口。
//       创建 AppController（C++ 后端总控），通过 context property 暴露给 QML，
//       然后加载编译注册的 QML 模块 ADASMaster.Main。
// 输入：QML 模块 "ADASMaster" 的 "Main" 组件。
// 输出：Qt Quick 应用窗口。
//
// 说明：使用 qt_add_qml_module 编译期注册，故用 engine.loadFromModule 加载。
//       AppController 作为 appController 暴露给 QML 的根上下文。
// =============================================================================

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QIcon>
#include "src/app/AppController.h"
#include "tests/MpcUiSmoke.h"

int main(int argc, char *argv[])
{
    // 高 DPI 适配，工控现场常接高分屏
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QGuiApplication app(argc, argv);
    const int mpcSmokeIndex = app.arguments().indexOf(QStringLiteral("--mpc-ui-smoke"));
    if (mpcSmokeIndex >= 0) qInstallMessageHandler(MpcUiSmoke::logger);
    QGuiApplication::setApplicationName(QStringLiteral("ADASMaster"));
    QGuiApplication::setOrganizationName(QStringLiteral("ADASLab"));
    QGuiApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    // 设置窗口/任务栏图标（运行时显示）
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/qt/qml/ADASMaster/config/app_icon.ico")));

    AppController controller;
    controller.startup();   // 连接模拟设备、启动刷新时钟

    QQmlApplicationEngine engine;
    if (mpcSmokeIndex >= 0)
        engine.setInitialProperties({{QStringLiteral("opacity"), 0.0}});
    engine.rootContext()->setContextProperty(QStringLiteral("appController"), &controller);

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    // 加载编译注册的 QML 模块（URI=ADASMaster，主组件=Main）
    engine.loadFromModule("ADASMaster", "Main");

    if ((app.arguments().contains(QStringLiteral("--mpc")) || mpcSmokeIndex >= 0) && !engine.rootObjects().isEmpty())
        engine.rootObjects().first()->setProperty("currentPage", 5);

    if (mpcSmokeIndex >= 0 && !engine.rootObjects().isEmpty()) {
        const QString imagePath = app.arguments().value(mpcSmokeIndex + 1);
        if (imagePath.isEmpty()) return 21;
        MpcUiSmoke::run(engine.rootObjects().first(), &controller, imagePath);
    }

    if (app.arguments().contains(QStringLiteral("--smoke-test")))
        return engine.rootObjects().isEmpty() ? 2 : 0;

    return QGuiApplication::exec();
}
