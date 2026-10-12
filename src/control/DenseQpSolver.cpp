// =============================================================================
// 功能：带行缩放、目标缩放、残差判停及 rho 自适应的小规模 ADMM。
// 输出只有收敛且满足残差条件才标记 Solved；超时不会返回可用控制指令。
// =============================================================================
#include "DenseQpSolver.h"
#include <algorithm>
#include <chrono>
#include <cmath>

namespace lateral {
namespace {
double norm(const Vector &x) {
    double result = 0.0;
    for (double v : x) result = std::max(result, std::abs(v));
    return result;
}
bool finite(const Vector &x) {
    return std::all_of(x.begin(), x.end(), [](double v) { return std::isfinite(v); });
}
Vector multiply(const Matrix &a, const Vector &x) {
    Vector result(a.rows, 0.0);
    for (int i = 0; i < a.rows; ++i)
        for (int j = 0; j < a.cols; ++j) result[i] += a(i, j) * x[j];
    return result;
}
bool cholesky(const Matrix &a, Matrix *l) {
    *l = Matrix(a.rows, a.cols);
    for (int i = 0; i < a.rows; ++i) {
        for (int j = 0; j <= i; ++j) {
            double value = a(i, j);
            for (int k = 0; k < j; ++k) value -= (*l)(i, k) * (*l)(j, k);
            if (i == j) {
                if (!std::isfinite(value) || value <= 1e-14) return false;
                (*l)(i, j) = std::sqrt(value);
            } else (*l)(i, j) = value / (*l)(j, j);
        }
    }
    return true;
}
Vector solveCholesky(const Matrix &l, Vector b) {
    for (int i = 0; i < l.rows; ++i) {
        for (int j = 0; j < i; ++j) b[i] -= l(i, j) * b[j];
        b[i] /= l(i, i);
    }
    for (int i = l.rows - 1; i >= 0; --i) {
        for (int j = i + 1; j < l.rows; ++j) b[i] -= l(j, i) * b[j];
        b[i] /= l(i, i);
    }
    return b;
}
} // namespace

QpResult DenseQpSolver::solve(const Matrix &h, const Vector &g, const Matrix &c,
                             const Vector &lower, const Vector &upper,
                             const QpSettings &settings, const Vector &warm, const Vector &warmDual) const {
    const auto start = std::chrono::steady_clock::now();
    QpResult result;
    auto elapsed = [&] {
        return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    };
    auto finish = [&](QpStatus status) {
        result.status = status;
        result.elapsedMs = elapsed();
        return result;
    };
    const int n = h.rows, m = c.rows;
    if (n < 1 || n > 80 || m < 1 || h.cols != n || c.cols != n
        || h.data.size() != static_cast<size_t>(n * n)
        || c.data.size() != static_cast<size_t>(m * n)
        || g.size() != static_cast<size_t>(n) || lower.size() != static_cast<size_t>(m)
        || upper.size() != static_cast<size_t>(m)
        || !finite(h.data) || !finite(g) || !finite(c.data)
        || !finite(lower) || !finite(upper)
        || settings.maximumIterations < 1 || !std::isfinite(settings.timeLimitMs)
        || settings.timeLimitMs <= 0.0 || !std::isfinite(settings.absoluteTolerance)
        || settings.absoluteTolerance <= 0.0 || !std::isfinite(settings.relativeTolerance)
        || settings.relativeTolerance <= 0.0)
        return finish(QpStatus::InvalidProblem);
    for (int i = 0; i < m; ++i)
        if (lower[i] > upper[i]) return finish(QpStatus::InvalidProblem);
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            if (std::abs(h(i, j) - h(j, i)) > 1e-8 * (1.0 + std::abs(h(i, j))))
                return finish(QpStatus::InvalidProblem);

    Matrix p = h, a = c;
    Vector q = g, lo = lower, hi = upper;
    double objectiveScale = 1.0;
    for (int i = 0; i < n; ++i) objectiveScale = std::max(objectiveScale, std::abs(h(i, i)));
    for (double &v : p.data) v /= objectiveScale;
    for (double &v : q) v /= objectiveScale;
    for (int i = 0; i < m; ++i) {
        double scale = 0.0;
        for (int j = 0; j < n; ++j) scale = std::max(scale, std::abs(a(i, j)));
        if (scale < 1e-12) {
            if (lo[i] > 0.0 || hi[i] < 0.0) return finish(QpStatus::InvalidProblem);
            scale = 1.0;
        }
        for (int j = 0; j < n; ++j) a(i, j) /= scale;
        lo[i] /= scale;
        hi[i] /= scale;
    }
    Matrix objectiveFactor, factor;
    if (!cholesky(p, &objectiveFactor)) return finish(QpStatus::InvalidProblem);
    Vector unconstrainedRhs = q;
    for (double &value : unconstrainedRhs) value = -value;
    const Vector unconstrained = solveCholesky(objectiveFactor, unconstrainedRhs);
    Matrix inverseAt(n, m);
    for (int row = 0; row < m; ++row) {
        Vector rhs(n);
        for (int j = 0; j < n; ++j) rhs[j] = a(row, j);
        const Vector column = solveCholesky(objectiveFactor, rhs);
        for (int j = 0; j < n; ++j) inverseAt(j, row) = column[j];
    }
    // 从 ADMM 的对偶变量猜测活跃约束，解 KKT 并验证全部约束及乘子符号。
    // 猜错或约束线性相关时继续 ADMM，不能把未验证的候选当成最优解。
    auto polish = [&](const Vector &dualGuess, Vector *candidate, Vector *candidateDual) {
        std::vector<int> active;
        for (int i = 0; i < m; ++i)
            if (std::abs(dualGuess[i]) > 1e-7) active.push_back(i);
        if (active.size() > static_cast<size_t>(n)) return false;
        *candidate = unconstrained;
        *candidateDual = Vector(m, 0.0);
        const int count = static_cast<int>(active.size());
        if (count > 0) {
            Matrix schur(count, count), activeFactor;
            Vector rhs(count, 0.0);
            for (int i = 0; i < count; ++i) {
                const int row = active[i];
                rhs[i] = -(dualGuess[row] > 0.0 ? hi[row] : lo[row]);
                for (int k = 0; k < n; ++k) {
                    rhs[i] += a(row, k) * unconstrained[k];
                    for (int j = 0; j < count; ++j)
                        schur(i, j) += a(row, k) * inverseAt(k, active[j]);
                }
            }
            if (!cholesky(schur, &activeFactor)) return false;
            const Vector lambda = solveCholesky(activeFactor, rhs);
            for (int i = 0; i < count; ++i) {
                if (lambda[i] * dualGuess[active[i]] < -1e-10) return false;
                (*candidateDual)[active[i]] = lambda[i];
                for (int k = 0; k < n; ++k) (*candidate)[k] -= inverseAt(k, active[i]) * lambda[i];
            }
        }
        if (!finite(*candidate) || !finite(*candidateDual)) return false;
        const auto ax = multiply(a, *candidate);
        const double tolerance = settings.absoluteTolerance + settings.relativeTolerance * norm(ax);
        for (int i = 0; i < m; ++i)
            if (ax[i] < lo[i] - tolerance || ax[i] > hi[i] + tolerance) return false;
        const auto px = multiply(p, *candidate);
        Vector stationarity(n, 0.0), aty(n, 0.0);
        for (int j = 0; j < n; ++j) {
            for (int i = 0; i < m; ++i) aty[j] += a(i, j) * (*candidateDual)[i];
            stationarity[j] = px[j] + q[j] + aty[j];
        }
        return norm(stationarity) <= settings.absoluteTolerance
            + settings.relativeTolerance * std::max({norm(px), norm(q), norm(aty)});
    };
    Matrix ata(n, n);
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            for (int k = 0; k < m; ++k) ata(i, j) += a(k, i) * a(k, j);
    double rho = 0.01;
    const double sigma = 1e-6;
    auto refactor = [&] {
        Matrix system = p;
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < n; ++j) system(i, j) += rho * ata(i, j);
            system(i, i) += sigma;
        }
        return cholesky(system, &factor);
    };
    if (!refactor()) return finish(QpStatus::NumericalFailure);
    Vector x(n, 0.0), y(m, 0.0);
    if (warm.size() == static_cast<size_t>(n) && finite(warm)) x = warm;
    if (warmDual.size() == static_cast<size_t>(m) && finite(warmDual)) y = warmDual;
    Vector z = multiply(a, x);
    for (int i = 0; i < m; ++i) z[i] = std::clamp(z[i], lo[i], hi[i]);
    for (int iteration = 1; iteration <= settings.maximumIterations; ++iteration) {
        if (elapsed() >= settings.timeLimitMs) return finish(QpStatus::TimeLimit);
        Vector rhs(n, 0.0);
        for (int j = 0; j < n; ++j) {
            rhs[j] = sigma * x[j] - q[j];
            for (int i = 0; i < m; ++i) rhs[j] += a(i, j) * (rho * z[i] - y[i]);
        }
        x = solveCholesky(factor, rhs);
        if (!finite(x)) return finish(QpStatus::NumericalFailure);
        const Vector ax = multiply(a, x);
        for (int i = 0; i < m; ++i) {
            // 松弛系数加快收敛，y 保存未缩放的对偶变量。
            const double relaxed = 1.6 * ax[i] - 0.6 * z[i];
            z[i] = std::clamp(relaxed + y[i] / rho, lo[i], hi[i]);
            y[i] += rho * (relaxed - z[i]);
        }
        const Vector px = multiply(p, x);
        Vector dual(n, 0.0), aty(n, 0.0), primal(m, 0.0);
        for (int j = 0; j < n; ++j) {
            for (int i = 0; i < m; ++i) aty[j] += a(i, j) * y[i];
            dual[j] = px[j] + q[j] + aty[j];
        }
        for (int i = 0; i < m; ++i) primal[i] = ax[i] - z[i];
        result.iterations = iteration;
        result.primalResidual = norm(primal);
        result.dualResidual = norm(dual);
        const double primalTolerance = settings.absoluteTolerance
            + settings.relativeTolerance * std::max(norm(ax), norm(z));
        const double dualTolerance = settings.absoluteTolerance
            + settings.relativeTolerance * std::max({norm(px), norm(q), norm(aty)});
        if (result.primalResidual <= primalTolerance && result.dualResidual <= dualTolerance) {
            if (elapsed() >= settings.timeLimitMs) return finish(QpStatus::TimeLimit);
            result.solution = x;
            result.dualSolution = y;
            return finish(QpStatus::Solved);
        }
        if (iteration % 25 == 0) {
            Vector candidate, candidateDual;
            if (polish(y, &candidate, &candidateDual)) {
                if (elapsed() >= settings.timeLimitMs) return finish(QpStatus::TimeLimit);
                result.solution = candidate;
                result.dualSolution = candidateDual;
                // KKT 校验已经完成；不把之前 ADMM 迭代残差写成候选的残差。
                const auto candidateAx = multiply(a, candidate);
                result.primalResidual = 0.0;
                for (int i = 0; i < m; ++i)
                    result.primalResidual = std::max(result.primalResidual,
                        std::abs(candidateAx[i] - std::clamp(candidateAx[i], lo[i], hi[i])));
                const auto candidatePx = multiply(p, candidate);
                result.dualResidual = 0.0;
                for (int j = 0; j < n; ++j) {
                    double stationarity = candidatePx[j] + q[j];
                    for (int i = 0; i < m; ++i) stationarity += a(i, j) * candidateDual[i];
                    result.dualResidual = std::max(result.dualResidual, std::abs(stationarity));
                }
                return finish(QpStatus::Solved);
            }
            const double ratio = result.primalResidual / std::max(result.dualResidual, 1e-12);
            if (ratio > 10.0 || ratio < 0.1) {
                rho = std::clamp(rho * std::sqrt(std::max(ratio, 1e-6)), 1e-4, 1e4);
                if (!refactor()) return finish(QpStatus::NumericalFailure);
            }
        }
    }
    return finish(QpStatus::IterationLimit);
}
} // namespace lateral
