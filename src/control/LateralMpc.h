// =============================================================================
// 功能：动力学/低速运动学自行车模型 + 执行器滞后的约束横向 MPC。
// 输入：质心状态、编码器前轮角、上次已发送目标、参考路径；输出首个方向盘目标。
// 调用者负责时间戳、安全状态、通信、纵向减速及真实伺服的位置闭环。
// =============================================================================
#pragma once
#include "DenseQpSolver.h"
#include "ReferencePath.h"

namespace lateral {
class LateralMpc {
public:
    bool configure(const VehicleParameters &vehicle, const MpcSettings &settings,
                   QString *error = nullptr);
    void reset();
    MpcResult compute(const VehicleState &state, double measuredRoadAngleRad,
                      double previousRoadTargetRad, const ReferencePath &path);
    const VehicleParameters &vehicle() const { return m_vehicle; }
    const MpcSettings &settings() const { return m_settings; }
private:
    VehicleParameters m_vehicle;
    MpcSettings m_settings;
    Vector m_warmStart;
    Vector m_warmDual;
    double m_projectionHintM = -1.0;
};
} // namespace lateral
