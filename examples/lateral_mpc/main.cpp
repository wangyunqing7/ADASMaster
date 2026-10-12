// =============================================================================
// 功能：Qt 控制器闭环仿真；输入场景/CSV 路径，输出跟踪与求解时间统计。
// 不连接真实硬件；虚拟时间推进不代表工控系统的实际调度性能。
// =============================================================================
#include "SimulationPlant.h"
#include "src/control/SteeringRobotController.h"
#include <QCoreApplication>
#include <QFile>
#include <QTextStream>

using namespace lateral;
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QTextStream output(stdout);
    const QString kind = app.arguments().value(1, QStringLiteral("lane-change"));
    if (kind != QLatin1String("straight") && kind != QLatin1String("circle")
        && kind != QLatin1String("lane-change")) {
        output << "Usage: lateral_mpc_demo [straight|circle|lane-change] [output.csv]\n";
        return 1;
    }
    simulation::Robot robot;
    SteeringRobotController controller;
    VehicleParameters vehicle;
    MpcSettings settings;
    QString error;
    if (!controller.configure(vehicle, settings, &error)
        || !controller.setReferencePath(simulation::makePath(kind), &error)
        || !controller.setRobot(&robot)) { output << error << '\n'; return 1; }
    simulation::Plant plant;
    plant.vehicle = vehicle;
    plant.state.localizationValid = true;
    plant.state.timestampMs = 0;
    plant.state.longitudinalSpeedMps = 12.0;
    plant.state.yM = 0.4;
    controller.updateVehicleState(plant.state);
    robot.publish(0, plant.robotAngleRad());
    if (!controller.arm(0) || !controller.start(0)) { output << controller.faultReason() << '\n'; return 1; }
    QFile csv;
    QTextStream rows;
    if (app.arguments().size() >= 3) {
        csv.setFileName(app.arguments().at(2));
        if (!csv.open(QIODevice::WriteOnly | QIODevice::Text)) { output << csv.errorString() << '\n'; return 1; }
        rows.setDevice(&csv);
        rows << "time_s,x_m,y_m,yaw_rad,lateral_error_m,robot_target_deg,robot_actual_deg,solve_ms,iterations\n";
    }
    double squareError = 0.0, peakError = 0.0, maxSolveMs = 0.0, sumSolveMs = 0.0;
    constexpr int count = 750;
    for (int i = 0; i < count; ++i) {
        const qint64 now = i * 20;
        plant.state.timestampMs = now;
        controller.updateVehicleState(plant.state);
        robot.publish(now, plant.robotAngleRad());
        controller.processOnce(now);
        if (controller.controlState() != SteeringRobotController::State::Running) {
            output << "Stopped at " << now << "ms: " << controller.faultReason() << '\n'; return 2;
        }
        const auto &r = controller.lastResult();
        squareError += r.lateralErrorM * r.lateralErrorM;
        peakError = std::max(peakError, std::abs(r.lateralErrorM));
        maxSolveMs = std::max(maxSolveMs, r.solveTimeMs);
        sumSolveMs += r.solveTimeMs;
        if (rows.device()) rows << now / 1000.0 << ',' << plant.state.xM << ',' << plant.state.yM << ','
            << plant.state.yawRad << ',' << r.lateralErrorM << ',' << degrees(r.robotTargetRad) << ','
            << degrees(plant.robotAngleRad()) << ',' << r.solveTimeMs << ',' << r.iterations << '\n';
        plant.advance(vehicle.roadWheelAngle(robot.lastCommand.targetAngleRad), settings.sampleTimeS,
                      settings.maximumRobotRateRadS);
    }
    controller.stop();
    output << kind << ": RMS=" << std::sqrt(squareError / count) << "m, peak=" << peakError
           << "m, final=" << controller.lastResult().lateralErrorM << "m, mean solve="
           << sumSolveMs / count << "ms, max solve=" << maxSolveMs << "ms\n";
    return 0;
}
