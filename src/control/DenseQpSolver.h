// =============================================================================
// 功能：小规模稠密凸 QP 的 ADMM 求解器；输入 H/g/C/l/u，输出残差/状态/解。
// 仅支持正定 H；不是 OSQP，未实现不可行证书。迭代或时间耗尽视为失败。
// =============================================================================
#pragma once
#include <vector>

namespace lateral {
using Vector = std::vector<double>;
struct Matrix {
    int rows = 0, cols = 0;
    Vector data;
    Matrix() = default;
    Matrix(int r, int c) : rows(r), cols(c), data(static_cast<size_t>(r * c), 0.0) {}
    double &operator()(int r, int c) { return data[static_cast<size_t>(r * cols + c)]; }
    double operator()(int r, int c) const { return data[static_cast<size_t>(r * cols + c)]; }
};

enum class QpStatus { Solved, InvalidProblem, NumericalFailure, IterationLimit, TimeLimit };
struct QpSettings {
    int maximumIterations = 600;
    double timeLimitMs = 15.0;
    double absoluteTolerance = 1e-5;
    double relativeTolerance = 1e-4;
};
struct QpResult {
    QpStatus status = QpStatus::InvalidProblem;
    Vector solution;
    Vector dualSolution; // 缩放后的对偶变量，仅用于后续求解热启动
    int iterations = 0;
    double primalResidual = 0.0;
    double dualResidual = 0.0;
    double elapsedMs = 0.0;
};

class DenseQpSolver {
public:
    // min 0.5*x'H*x + g'x, subject to lower <= C*x <= upper
    QpResult solve(const Matrix &h, const Vector &g, const Matrix &c,
                   const Vector &lower, const Vector &upper,
                   const QpSettings &settings, const Vector &warmStart = {},
                   const Vector &warmDual = {}) const;
};
} // namespace lateral
