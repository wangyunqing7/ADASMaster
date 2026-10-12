// =============================================================================
// 功能：验证 QP 数值解、非线性闭环跟踪、约束及转向机器人故障停止链路。
// 输入：仿真状态和故障注入；输出：QtTest 断言，无真实硬件输出。
// =============================================================================
#include <QtTest>
#include <limits>
#include "src/control/SteeringRobotController.h"
#include "examples/lateral_mpc/SimulationPlant.h"

using namespace lateral;

class TestLateralMpc : public QObject {
    Q_OBJECT
private slots:
    void qpKnownSolution() {
        Matrix h(2, 2), c(3, 2);
        h(0, 0) = 2.0; h(1, 1) = 2.0;
        c(0, 0) = 1.0; c(1, 1) = 1.0;
        c(2, 0) = 1.0; c(2, 1) = 1.0;
        QpSettings settings;
        settings.timeLimitMs = 200.0;
        settings.absoluteTolerance = 1e-8;
        settings.relativeTolerance = 1e-8;
        const auto result = DenseQpSolver{}.solve(h, {-2.0, -4.0}, c,
                                                 {0.0, 0.0, 0.0}, {10.0, 10.0, 2.0}, settings);
        QCOMPARE(result.status, QpStatus::Solved);
        QVERIFY(std::abs(result.solution[0] - 0.5) < 1e-5);
        QVERIFY(std::abs(result.solution[1] - 1.5) < 1e-5);
    }
    void qpRejectsInvalidAndInfeasibleProblems() {
        Matrix h(1, 1), c(2, 1);
        h(0, 0) = 1.0; c(0, 0) = 1.0; c(1, 0) = 1.0;
        QpSettings settings;
        settings.timeLimitMs = 100.0;
        settings.maximumIterations = 100;
        auto r = DenseQpSolver{}.solve(h, {0.0}, c, {1.0, 0.0}, {0.0, 1.0}, settings);
        QCOMPARE(r.status, QpStatus::InvalidProblem);
        r = DenseQpSolver{}.solve(h, {0.0}, c, {1.0, -2.0}, {2.0, -1.0}, settings);
        QVERIFY(r.status != QpStatus::Solved);
        QVERIFY(r.solution.empty());
        h(0, 0) = -1.0;
        r = DenseQpSolver{}.solve(h, {0.0}, c, {-1.0, -1.0}, {1.0, 1.0}, settings);
        QCOMPARE(r.status, QpStatus::InvalidProblem);
    }
    void qpTimeLimitNeverReturnsUsableSolution() {
        Matrix h(1, 1), c(1, 1);
        h(0, 0) = 1.0; c(0, 0) = 1.0;
        QpSettings settings;
        settings.timeLimitMs = 1e-12;
        const auto r = DenseQpSolver{}.solve(h, {-1.0}, c, {-2.0}, {2.0}, settings);
        QCOMPARE(r.status, QpStatus::TimeLimit);
        QVERIFY(r.solution.empty());
    }
    void configurationAndPathValidation() {
        LateralMpc mpc;
        VehicleParameters vehicle;
        MpcSettings settings;
        QString error;
        vehicle.steeringRatio = 0.0;
        QVERIFY(!mpc.configure(vehicle, settings, &error));
        QVERIFY(!error.isEmpty());
        vehicle = {};
        settings.horizon = 1000;
        QVERIFY(!mpc.configure(vehicle, settings));
        ReferencePath path;
        QVERIFY(path.setPoints(simulation::makePath(QStringLiteral("circle"))));
        QVERIFY(!path.setPoints({{0, 0, 0, 0, 0}, {0, 1, 0, 0, 0}}, &error));
        QVERIFY(!path.empty()); // 非法替换不破坏已有路径
        const double nan = std::numeric_limits<double>::quiet_NaN();
        QVERIFY(!path.setPoints({{0, 0, nan, 0, 0}, {1, 1, 0, 0, 0}}));
        QVERIFY(!path.setPoints({{0, 0, 0, pi, 0}, {1, 1, 0, pi, 0}}));
    }
    void pathProjectionAndAngleWrap() {
        ReferencePath path;
        QVERIFY(path.setPoints({{0, 0, 0, pi - 0.01, 0}, {10, -10, 0, -pi + 0.01, 0}}));
        QVERIFY(std::abs(std::abs(path.sample(5).headingRad) - pi) < 1e-8);
        VehicleState state;
        state.xM = -5.0; state.yM = 1.0; state.yawRad = -pi;
        PathProjection projection;
        QVERIFY(path.project(state, &projection));
        QVERIFY(std::abs(projection.reference.sM - 5.0) < 1e-6);
        QVERIFY(std::abs(projection.lateralErrorM + 1.0) < 1e-6);
        QVERIFY(std::abs(projection.headingErrorRad) < 1e-6);
    }
    void nonlinearTracking_data() {
        QTest::addColumn<QString>("kind");
        QTest::addColumn<double>("speed");
        QTest::addColumn<double>("sign");
        QTest::newRow("straight-dynamic") << QStringLiteral("straight") << 12.0 << 1.0;
        QTest::newRow("circle-dynamic") << QStringLiteral("circle") << 12.0 << 1.0;
        QTest::newRow("lane-change-dynamic") << QStringLiteral("lane-change") << 12.0 << 1.0;
        QTest::newRow("circle-low-speed") << QStringLiteral("circle") << 2.0 << 1.0;
        QTest::newRow("reversed-encoder") << QStringLiteral("lane-change") << 10.0 << -1.0;
    }
    void nonlinearTracking() {
        QFETCH(QString, kind);
        QFETCH(double, speed);
        QFETCH(double, sign);
        VehicleParameters vehicle;
        vehicle.steeringSign = sign;
        vehicle.steeringZeroRad = radians(7.0);
        MpcSettings settings;
        // 功能测试留出机器负载余量，实时预算用独立 demo 统计。
        settings.sampleTimeS = 0.05;
        settings.solverTimeLimitMs = 45.0;
        LateralMpc mpc;
        QVERIFY(mpc.configure(vehicle, settings));
        ReferencePath path;
        QVERIFY(path.setPoints(simulation::makePath(kind)));
        simulation::Plant plant;
        plant.vehicle = vehicle;
        plant.state.localizationValid = true;
        plant.state.longitudinalSpeedMps = speed;
        plant.state.yM = 0.4;
        double previous = 0.0, squareError = 0.0;
        constexpr int count = 240;
        for (int i = 0; i < count; ++i) {
            const auto r = mpc.compute(plant.state, plant.roadAngleRad, previous, path);
            QVERIFY2(r.ok(), qPrintable(QStringLiteral("step %1: %2").arg(i).arg(r.message)));
            QCOMPARE(r.dynamicModel, speed >= settings.dynamicModelMinSpeedMps);
            QVERIFY(std::abs(r.robotTargetRad) <= settings.maximumRobotAngleRad + 1e-9);
            QVERIFY(std::abs(r.roadWheelTargetRad - previous)
                    <= settings.maximumRobotRateRadS / vehicle.steeringRatio * settings.sampleTimeS + 1e-9);
            double last = previous;
            for (double command : r.predictedRoadWheelTargetsRad) {
                QVERIFY(std::abs(command - last) <= settings.maximumRobotRateRadS
                        / vehicle.steeringRatio * settings.sampleTimeS + 1e-9);
                last = command;
            }
            for (double acceleration : r.predictedLateralAccelerationsMps2)
                QVERIFY(std::abs(acceleration) <= settings.maximumLateralAccelerationMps2 + 1e-3);
            squareError += r.lateralErrorM * r.lateralErrorM;
            previous = r.roadWheelTargetRad;
            plant.advance(previous, settings.sampleTimeS, settings.maximumRobotRateRadS);
            if (i == count - 1) QVERIFY2(std::abs(r.lateralErrorM) < 0.10, qPrintable(QString::number(r.lateralErrorM)));
        }
        QVERIFY(std::sqrt(squareError / count) < 0.15);
    }
    void trackingAndSpeedFailures() {
        LateralMpc mpc;
        ReferencePath path;
        QVERIFY(path.setPoints(simulation::makePath(QStringLiteral("straight"))));
        VehicleState state;
        state.localizationValid = true;
        state.longitudinalSpeedMps = -1.0;
        QCOMPARE(mpc.compute(state, 0, 0, path).status, ControlStatus::SpeedOutOfRange);
        state.longitudinalSpeedMps = 10.0;
        state.yM = 3.0;
        QCOMPARE(mpc.compute(state, 0, 0, path).status, ControlStatus::TrackingEnvelopeExceeded);
        state.yM = 0; state.xM = path.lengthM();
        QCOMPARE(mpc.compute(state, 0, 0, path).status, ControlStatus::ReferenceUnavailable);
        state.xM = 0; state.yawRad = std::numeric_limits<double>::quiet_NaN();
        QCOMPARE(mpc.compute(state, 0, 0, path).status, ControlStatus::InvalidInput);
        state.yawRad = 0; state.longitudinalSpeedMps = 25.0;
        QVERIFY(path.setPoints(simulation::makePath(QStringLiteral("circle"))));
        QCOMPARE(mpc.compute(state, 0, 0, path).status, ControlStatus::ReferenceUnavailable);
        MpcSettings settings;
        settings.maximumRobotAngleRad = radians(5.0);
        QVERIFY(mpc.configure({}, settings));
        state.longitudinalSpeedMps = 12.0;
        QCOMPARE(mpc.compute(state, 0, 0, path).status, ControlStatus::ReferenceUnavailable);
    }
    void robotFaultHandling_data() {
        QTest::addColumn<QString>("fault");
        for (const char *name : {"feedback-timeout", "state-timeout", "estop", "torque",
                                 "rejected-command", "invalid-state", "scheduler", "backward-feedback",
                                 "iteration-limit", "future-state", "robot-disconnect"})
            QTest::newRow(name) << QString::fromLatin1(name);
    }
    void robotFaultHandling() {
        QFETCH(QString, fault);
        simulation::Robot robot;
        SteeringRobotController controller;
        MpcSettings settings;
        settings.sampleTimeS = 0.05;
        settings.solverTimeLimitMs = 45.0;
        if (fault == QLatin1String("iteration-limit")) settings.maximumSolverIterations = 1;
        QVERIFY(controller.configure({}, settings));
        QVERIFY(controller.setReferencePath(simulation::makePath(QStringLiteral("straight"))));
        QVERIFY(controller.setRobot(&robot));
        VehicleState state;
        state.timestampMs = 1000; state.localizationValid = true;
        state.longitudinalSpeedMps = 10.0; state.yM = 0.4;
        controller.updateVehicleState(state);
        robot.publish(1000, 0.0);
        QVERIFY(controller.arm(1000));
        QVERIFY(controller.start(1000));
        QSignalSpy stopped(&controller, &SteeringRobotController::stopRequested);
        qint64 tick = 1000;
        if (fault == QLatin1String("feedback-timeout")) {
            tick = 1101; state.timestampMs = tick; controller.updateVehicleState(state);
        } else if (fault == QLatin1String("state-timeout")) {
            tick = 1101; robot.publish(tick, 0.0);
        } else if (fault == QLatin1String("estop")) controller.setExternalEmergencyStop(true);
        else if (fault == QLatin1String("torque")) {
            robot.feedback.torqueNm = 21.0; robot.publish(1000, 0.0);
        } else if (fault == QLatin1String("rejected-command")) robot.rejectCommands = true;
        else if (fault == QLatin1String("invalid-state")) {
            state.localizationValid = false; controller.updateVehicleState(state);
        } else if (fault == QLatin1String("scheduler")) {
            controller.processOnce(1000);
            QCOMPARE(controller.controlState(), SteeringRobotController::State::Running);
            tick = 1080; state.timestampMs = tick;
            controller.updateVehicleState(state); robot.publish(tick, 0);
        } else if (fault == QLatin1String("backward-feedback")) robot.publish(999, 0);
        else if (fault == QLatin1String("future-state")) {
            state.timestampMs = 1010; controller.updateVehicleState(state);
        } else if (fault == QLatin1String("robot-disconnect")) {
            robot.feedback.online = false;
            emit robot.feedbackReceived(robot.feedback);
        }
        const int before = robot.commandCount;
        controller.processOnce(tick);
        QVERIFY(controller.controlState() == SteeringRobotController::State::Fault
                || controller.controlState() == SteeringRobotController::State::Emergency);
        QVERIFY(!controller.faultReason().isEmpty());
        QCOMPARE(robot.commandCount, before);
        QCOMPARE(stopped.count(), 1);
        QVERIFY(robot.stopCount >= 1);
        controller.stop(); // 普通 stop 不能解除故障锁存
        QVERIFY(!controller.arm(tick));
        controller.processOnce(tick + 50);
        QCOMPARE(robot.commandCount, before);
    }
    void emergencyRequiresResetAndFreshFeedback() {
        simulation::Robot robot;
        SteeringRobotController controller;
        QVERIFY(controller.setRobot(&robot));
        QVERIFY(controller.setReferencePath(simulation::makePath(QStringLiteral("straight"))));
        VehicleState state;
        state.timestampMs = 0; state.localizationValid = true; state.longitudinalSpeedMps = 10;
        controller.updateVehicleState(state); robot.publish(0, 0);
        QVERIFY(controller.arm(0)); QVERIFY(controller.start(0));
        controller.setExternalEmergencyStop(true);
        QVERIFY(!controller.resetFault(0));
        controller.setExternalEmergencyStop(false);
        QCOMPARE(controller.controlState(), SteeringRobotController::State::Emergency);
        QVERIFY(!controller.resetFault(101));
        state.timestampMs = 101; controller.updateVehicleState(state); robot.publish(101, 0);
        QVERIFY(controller.resetFault(101));
        QCOMPARE(controller.controlState(), SteeringRobotController::State::Disabled);
        QVERIFY(controller.arm(101)); QVERIFY(controller.start(101));
    }
    void resetAllowsConfigurationAfterEarlyFault() {
        SteeringRobotController controller;
        controller.setExternalEmergencyStop(true);
        QVERIFY(!controller.resetFault(0));
        controller.setExternalEmergencyStop(false);
        QVERIFY(controller.resetFault(0));
        simulation::Robot robot;
        QVERIFY(controller.setRobot(&robot));
        robot.publish(0, 0);
        QVERIFY(!controller.arm(0)); // 尚无路径和车辆状态
        QCOMPARE(controller.controlState(), SteeringRobotController::State::Fault);
        QVERIFY(controller.resetFault(0));
        QVERIFY(controller.setReferencePath(simulation::makePath(QStringLiteral("straight"))));
    }
    void waitsForEnableConfirmation() {
        class AsyncRobot : public simulation::Robot {
        public:
            bool requestEnable() override { return true; }
        } robot;
        SteeringRobotController controller;
        QVERIFY(controller.setRobot(&robot));
        QVERIFY(controller.setReferencePath(simulation::makePath(QStringLiteral("straight"))));
        VehicleState state;
        state.timestampMs = 0; state.localizationValid = true; state.longitudinalSpeedMps = 10;
        controller.updateVehicleState(state); robot.publish(0, 0);
        controller.processOnce(0);
        QCOMPARE(robot.commandCount, 0);
        QVERIFY(controller.arm(0));
        controller.processOnce(0);
        QCOMPARE(robot.commandCount, 0);
        QVERIFY(!controller.start(0));
        QCOMPARE(controller.controlState(), SteeringRobotController::State::Fault);
    }
};

QTEST_GUILESS_MAIN(TestLateralMpc)
#include "TestLateralMpc.moc"
