// =============================================================================
// 功能：构建双线性离散模型、消元状态生成 QP、滚动求解和首指令限幅。
// 模型 x=[ey,epsi,vy,r,delta]，u 是等效前轮目标角，delta 是实际前轮角。
// =============================================================================
#include "LateralMpc.h"
#include <algorithm>
#include <chrono>
#include <cmath>

namespace lateral {
namespace {
constexpr int nx = 5;
// 五阶矩阵带部分选主元的求解，避免低速刚性模型用显式欧拉失稳。
bool linearSolve(Matrix a, Matrix b, Matrix *solution) {
    for (int i = 0; i < a.rows; ++i) {
        int pivot = i;
        for (int k = i + 1; k < a.rows; ++k)
            if (std::abs(a(k, i)) > std::abs(a(pivot, i))) pivot = k;
        if (std::abs(a(pivot, i)) < 1e-12) return false;
        for (int j = 0; j < a.cols; ++j) std::swap(a(i, j), a(pivot, j));
        for (int j = 0; j < b.cols; ++j) std::swap(b(i, j), b(pivot, j));
        const double d = a(i, i);
        for (int j = i; j < a.cols; ++j) a(i, j) /= d;
        for (int j = 0; j < b.cols; ++j) b(i, j) /= d;
        for (int k = 0; k < a.rows; ++k) {
            if (k == i) continue;
            const double f = a(k, i);
            for (int j = i; j < a.cols; ++j) a(k, j) -= f * a(i, j);
            for (int j = 0; j < b.cols; ++j) b(k, j) -= f * b(i, j);
        }
    }
    *solution = b;
    return true;
}
} // namespace

bool LateralMpc::configure(const VehicleParameters &v, const MpcSettings &s, QString *error) {
    const QString issue = validateConfiguration(v, s);
    if (error) *error = issue;
    if (!issue.isEmpty()) return false;
    m_vehicle = v;
    m_settings = s;
    reset();
    return true;
}
void LateralMpc::reset() {
    m_warmStart.clear();
    m_warmDual.clear();
    m_projectionHintM = -1.0;
}

MpcResult LateralMpc::compute(const VehicleState &state, double measuredAngle,
                             double previousTarget, const ReferencePath &path) {
    const auto start = std::chrono::steady_clock::now();
    MpcResult result;
    const auto &v = m_vehicle;
    const auto &s = m_settings;
    auto elapsed = [&] {
        return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    };
    auto fail = [&](ControlStatus status, const QString &message) {
        result.status = status;
        result.message = message;
        result.solveTimeMs = elapsed();
        m_warmStart.clear();
        m_warmDual.clear();
        return result;
    };
    const QString issue = validateConfiguration(v, s);
    if (!issue.isEmpty()) return fail(ControlStatus::InvalidConfiguration, issue);
    if (!finiteState(state) || !state.localizationValid || !std::isfinite(measuredAngle)
        || !std::isfinite(previousTarget))
        return fail(ControlStatus::InvalidInput, QStringLiteral("Invalid state or steering measurement"));
    const double speed = state.longitudinalSpeedMps;
    if (speed < s.minimumSpeedMps || speed > s.maximumSpeedMps)
        return fail(ControlStatus::SpeedOutOfRange, QStringLiteral("Forward speed outside calibrated range"));
    PathProjection projection;
    if (!path.project(state, &projection, m_projectionHintM,
                       std::max(5.0, speed * s.sampleTimeS * 5.0)))
        return fail(ControlStatus::ReferenceUnavailable, QStringLiteral("No valid path projection"));
    result.lateralErrorM = projection.lateralErrorM;
    result.headingErrorRad = projection.headingErrorRad;
    if (std::abs(result.lateralErrorM) > s.maximumLateralErrorM
        || std::abs(result.headingErrorRad) > s.maximumHeadingErrorRad)
        return fail(ControlStatus::TrackingEnvelopeExceeded, QStringLiteral("Tracking envelope exceeded"));
    if (projection.reference.sM + speed * s.sampleTimeS >= path.lengthM())
        return fail(ControlStatus::ReferenceUnavailable, QStringLiteral("Path end reached; longitudinal stop required"));
    m_projectionHintM = projection.reference.sM;
    result.dynamicModel = speed >= s.dynamicModelMinSpeedMps;
    const int n = s.horizon;
    const double dt = s.sampleTimeS;
    const double lf = v.frontAxleToCgM, lr = v.rearAxleToCgM, wheelbase = v.wheelbaseM();
    const double cf = v.frontCorneringStiffnessNPerRad, cr = v.rearCorneringStiffnessNPerRad;
    Matrix ac(nx, nx), continuousInput(nx, 2); // 第二列为单位曲率扰动
    Vector accelerationRow(nx, 0.0);
    ac(0, 1) = speed;
    continuousInput(1, 1) = -speed;
    if (result.dynamicModel) {
        ac(0, 2) = 1.0;
        ac(1, 3) = 1.0;
        ac(2, 2) = -(cf + cr) / (v.massKg * speed);
        ac(2, 3) = (cr * lr - cf * lf) / (v.massKg * speed) - speed;
        ac(2, 4) = cf / v.massKg;
        ac(3, 2) = (cr * lr - cf * lf) / (v.yawInertiaKgM2 * speed);
        ac(3, 3) = -(cf * lf * lf + cr * lr * lr) / (v.yawInertiaKgM2 * speed);
        ac(3, 4) = cf * lf / v.yawInertiaKgM2;
        accelerationRow[2] = ac(2, 2);
        accelerationRow[3] = ac(2, 3) + speed;
        accelerationRow[4] = ac(2, 4); // ay = vy_dot + v*r
    } else {
        // 质心运动学模型：beta≈lr/L*delta；vy/r 不参与低速模型的代价。
        ac(0, 4) = speed * lr / wheelbase;
        ac(1, 4) = speed / wheelbase;
        accelerationRow[4] = speed * speed / wheelbase;
    }
    ac(4, 4) = -1.0 / v.actuatorTimeConstantS;
    continuousInput(4, 0) = 1.0 / v.actuatorTimeConstantS;
    Matrix left(nx, nx), right(nx, nx + 2), discrete;
    for (int i = 0; i < nx; ++i) {
        for (int j = 0; j < nx; ++j) {
            left(i, j) = (i == j ? 1.0 : 0.0) - 0.5 * dt * ac(i, j);
            right(i, j) = (i == j ? 1.0 : 0.0) + 0.5 * dt * ac(i, j);
        }
        for (int j = 0; j < 2; ++j) right(i, nx + j) = dt * continuousInput(i, j);
    }
    if (!linearSolve(left, right, &discrete))
        return fail(ControlStatus::SolverFailed, QStringLiteral("Model discretization failed"));
    // delta 和机器人编码器共同限定有效目标范围，零位偏置也必须计入。
    const double encoderA = v.roadWheelAngle(-s.maximumRobotAngleRad);
    const double encoderB = v.roadWheelAngle(s.maximumRobotAngleRad);
    const double angleMin = std::max(-s.maximumRoadWheelAngleRad, std::min(encoderA, encoderB));
    const double angleMax = std::min(s.maximumRoadWheelAngleRad, std::max(encoderA, encoderB));
    const double rateStep = s.maximumRobotRateRadS / v.steeringRatio * dt;
    if (measuredAngle < angleMin || measuredAngle > angleMax
        || previousTarget < angleMin || previousTarget > angleMax)
        return fail(ControlStatus::InvalidInput, QStringLiteral("Steering outside physical limits"));

    Matrix h(n, n), constraints(4 * n, n), sensitivity(nx, n);
    Vector g(n, 0.0), lower(4 * n, 0.0), upper(4 * n, 0.0);
    Vector nominal{result.lateralErrorM, result.headingErrorRad,
                   state.lateralSpeedMps, state.yawRateRadS, measuredAngle};
    std::vector<Vector> offsets;
    std::vector<Matrix> sensitivities;
    offsets.reserve(n);
    sensitivities.reserve(n);
    for (int k = 0; k < n; ++k) {
        const auto ref = path.sample(projection.reference.sM + speed * dt * k);
        if (std::abs(speed * speed * ref.curvaturePerM) > s.maximumLateralAccelerationMps2)
            return fail(ControlStatus::ReferenceUnavailable, QStringLiteral("Path curvature requires lower speed"));
        Vector next(nx, 0.0);
        Matrix nextSensitivity(nx, n);
        for (int i = 0; i < nx; ++i) {
            next[i] = discrete(i, nx + 1) * ref.curvaturePerM;
            for (int j = 0; j < nx; ++j) {
                next[i] += discrete(i, j) * nominal[j];
                for (int u = 0; u < n; ++u)
                    nextSensitivity(i, u) += discrete(i, j) * sensitivity(j, u);
            }
            nextSensitivity(i, k) += discrete(i, nx);
        }
        nominal = next;
        sensitivity = nextSensitivity;
        offsets.push_back(nominal);
        sensitivities.push_back(sensitivity);
        const auto targetRef = path.sample(projection.reference.sM + speed * dt * (k + 1));
        const double curvature = targetRef.curvaturePerM;
        const double yawRef = speed * curvature;
        const double vyRef = result.dynamicModel
            ? (lr - lf * v.massKg * speed * speed / (wheelbase * cr)) * yawRef
            : speed * lr * curvature;
        const double deltaRef = result.dynamicModel
            ? (wheelbase + v.massKg / wheelbase * (lr / cf - lf / cr) * speed * speed) * curvature
            : wheelbase * curvature;
        if (deltaRef < angleMin || deltaRef > angleMax
            || std::abs(speed * speed * curvature) > s.maximumLateralAccelerationMps2)
            return fail(ControlStatus::ReferenceUnavailable,
                        QStringLiteral("Reference outside steering or lateral acceleration limits"));
        const Vector target{0.0, -vyRef / speed, vyRef, yawRef, deltaRef};
        for (int i = 0; i < nx; ++i) {
            double weight = s.stateWeights[i];
            if (!result.dynamicModel && (i == 2 || i == 3)) weight = 0.0;
            if (k == n - 1) weight *= s.terminalWeightMultiplier;
            for (int u = 0; u < n; ++u) {
                g[u] += 2.0 * weight * sensitivity(i, u) * (nominal[i] - target[i]);
                for (int w = 0; w < n; ++w)
                    h(u, w) += 2.0 * weight * sensitivity(i, u) * sensitivity(i, w);
            }
        }
        h(k, k) += 2.0 * s.inputWeight;
        g[k] -= 2.0 * s.inputWeight * deltaRef;
        // 变化惩罚使用上一条已被传输层接受的目标值，而非本轮测量值。
        h(k, k) += 2.0 * s.inputRateWeight;
        if (k == 0) g[k] -= 2.0 * s.inputRateWeight * previousTarget;
        else {
            h(k - 1, k - 1) += 2.0 * s.inputRateWeight;
            h(k, k - 1) -= 2.0 * s.inputRateWeight;
            h(k - 1, k) -= 2.0 * s.inputRateWeight;
        }
        constraints(k, k) = 1.0;
        lower[k] = angleMin;
        upper[k] = angleMax;
        constraints(n + k, k) = 1.0;
        if (k > 0) constraints(n + k, k - 1) = -1.0;
        lower[n + k] = -rateStep + (k == 0 ? previousTarget : 0.0);
        upper[n + k] = rateStep + (k == 0 ? previousTarget : 0.0);
        double nominalAcceleration = 0.0;
        for (int i = 0; i < nx; ++i) nominalAcceleration += accelerationRow[i] * nominal[i];
        for (int u = 0; u < n; ++u) {
            constraints(2 * n + k, u) = sensitivity(4, u);
            for (int i = 0; i < nx; ++i)
                constraints(3 * n + k, u) += accelerationRow[i] * sensitivity(i, u);
        }
        lower[2 * n + k] = angleMin - nominal[4];
        upper[2 * n + k] = angleMax - nominal[4];
        lower[3 * n + k] = -s.maximumLateralAccelerationMps2 - nominalAcceleration;
        upper[3 * n + k] = s.maximumLateralAccelerationMps2 - nominalAcceleration;
    }
    QpSettings qpSettings{s.maximumSolverIterations,
        s.solverTimeLimitMs - elapsed(), s.solverAbsoluteTolerance, s.solverRelativeTolerance};
    if (qpSettings.timeLimitMs <= 0.0)
        return fail(ControlStatus::SolverFailed, QStringLiteral("MPC construction exceeded deadline"));
    const auto qp = DenseQpSolver{}.solve(h, g, constraints, lower, upper, qpSettings,
                                         m_warmStart, m_warmDual);
    result.iterations = qp.iterations;
    if (qp.status != QpStatus::Solved)
        return fail(ControlStatus::SolverFailed,
                    QStringLiteral("QP failed (status %1, primal %2, dual %3)")
                        .arg(static_cast<int>(qp.status)).arg(qp.primalResidual).arg(qp.dualResidual));
    Vector solution = qp.solution;
    // 修复 ADMM 数值容差带来的角度/速度越界，随后重新校验整条预测。
    double last = previousTarget;
    for (double &u : solution) {
        u = std::clamp(u, std::max(angleMin, last - rateStep), std::min(angleMax, last + rateStep));
        last = u;
    }
    for (int k = 0; k < n; ++k) {
        Vector prediction = offsets[k];
        for (int i = 0; i < nx; ++i)
            for (int u = 0; u < n; ++u) prediction[i] += sensitivities[k](i, u) * solution[u];
        double acceleration = 0.0;
        for (int i = 0; i < nx; ++i) acceleration += accelerationRow[i] * prediction[i];
        if (prediction[4] < angleMin - 1e-5 || prediction[4] > angleMax + 1e-5
            || std::abs(acceleration) > s.maximumLateralAccelerationMps2 + 1e-3)
            return fail(ControlStatus::SolverFailed, QStringLiteral("Prediction violates physical constraints"));
        result.predictedLateralErrorsM.push_back(prediction[0]);
        result.predictedLateralAccelerationsMps2.push_back(acceleration);
    }
    if (elapsed() >= s.solverTimeLimitMs)
        return fail(ControlStatus::SolverFailed, QStringLiteral("MPC exceeded deadline"));
    result.predictedRoadWheelTargetsRad = solution;
    result.roadWheelTargetRad = solution.front();
    result.robotTargetRad = v.robotAngle(solution.front());
    result.status = ControlStatus::Solved;
    result.message = QStringLiteral("Solved");
    result.solveTimeMs = elapsed();
    m_warmStart = solution;
    std::rotate(m_warmStart.begin(), m_warmStart.begin() + 1, m_warmStart.end());
    m_warmStart.back() = solution.back();
    m_warmDual = qp.dualSolution;
    for (int block = 0; block < 4; ++block) {
        auto begin = m_warmDual.begin() + block * n;
        std::rotate(begin, begin + 1, begin + n);
        *(begin + n - 1) = 0.0; // 新增的末端约束尚无对偶估计
    }
    return result;
}
} // namespace lateral
