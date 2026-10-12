# Qt/C++ 转向机器人横向 MPC

本实现面向安装在方向盘上的角度模式转向机器人：输入参考轨迹、RTK/INS 融合质心状态和机器人编码器反馈，输出方向盘目标角度、最大角速度、转矩限值和指令有效期。伺服驱动器负责电机位置、速度和电流闭环。

ABD 官方公开的 [SR 35 产品说明](https://www.abdynamics.com/track-testing/driving-robots/sr35/)可用来理解方向盘机器人这一设备形态。本工程没有 ABD 私有通信协议，也不声称采用其内部控制算法。实际机器人只需实现 `ISteeringRobot` 的三个方法和反馈信号。

## 已实现的内容

- C++17 / Qt 6 Core 独立库 `adasmaster_lateral`，可链接到现有工程或单独使用。
- 中高速线性动力学自行车模型、低速质心运动学模型、实际转向角一阶滞后。
- 曲率预瞄、稳态前馈、转向传动比/零位/方向标定。
- 约束凸 QP：目标角度、相邻目标变化、预测实际转角、预测横向加速度。
- 小规模稠密 ADMM 求解器，残差收敛校验、KKT 候选校验、目标/约束缩放、原始/对偶热启动、迭代上限、整体 MPC 时间预算。
- Qt 调度外壳：定位/反馈新鲜度、使能确认、求解失败、调度超时、超转矩、急停锁存和显式复位。
- 非线性自行车/RK4 闭环示例，直线、圆弧、变道和 CSV 日志。

## 源码位置

| 文件 | 用途 |
|---|---|
| `src/control/LateralControlTypes.h` | 参数、坐标/单位约定、车辆状态和 MPC 结果 |
| `src/control/ReferencePath.*` | 弧长插值、曲率预瞄、线段投影和交叉路径搜索窗口 |
| `src/control/DenseQpSolver.*` | 正定稠密 QP 的 ADMM 求解 |
| `src/control/LateralMpc.*` | 模型、QP 构建和滚动优化 |
| `src/control/ISteeringRobot.h` | 硬件协议适配接口 |
| `src/control/SteeringRobotController.*` | Qt 控制调度及故障/急停状态机 |
| `examples/lateral_mpc/` | 仅依赖 Qt Core/Test 的独立构建、非线性仿真 |
| `tests/TestLateralMpc.cpp` | 控制效果、数值解、约束和故障注入回归 |

现有 `AppController` 增加了 `steeringController()` 和相应 Qt 属性，并把 `SafetyManager` 的急停状态接到控制器。这个实例初始为 Disabled，没有硬件适配器，也不会自动向现有 `FakeDevice` 发控制指令。现有 `ScenarioEngine` 的主车直线运动仿真继续按原逻辑运行；MPC 闭环可通过独立示例或新增的“MPC 调试”页运行。

## 可视化调试页

启动 `build/ADASMaster.exe` 并登录后选择左侧 **MPC 调试**，也可以用 `build/ADASMaster.exe --mpc` 预先选择该页，登录后显示。

- 直线、恒半径圆弧和平滑变道三个场景。
- 参考/实际质心轨迹、车辆朝向、跟随视图及缩放。
- 横向误差、方向盘目标/实际角度曲线，当前误差、RMS、峰值和求解耗时。
- 车速、初始偏差、时长、轨迹尺寸、播放倍速、转向比、执行器滞后、采样周期、预测步数、权重和约束调整。
- 开始、暂停/继续、停止、清空/更新预览和通过保存对话框导出 CSV。

界面输入参数在下一次开始时生效，运行/暂停期间禁止改参。图表最多显示约 600 个采样点，CSV 导出保留全部样本。全局急停会停止仿真并禁止重启，解除并复位后可开始新一轮。

`MpcSimulationModel` 在 GUI 线程中保存显示数据，`MpcSimulationWorker` 在独立 Qt 工作线程中执行模拟机器人闭环，排队发送数据到界面。界面默认 50 ms/20 Hz，适合当前 Debug 构建；20 ms/50 Hz 请使用 Release 构建。播放倍速只改变仿真时钟的推进速度，不改变控制模型的采样步长。界面始终使用软件模拟机器人。

可运行 `ctest --test-dir build -R 'test_mpc_simulation|mpc_ui_smoke' --output-on-failure` 验证异步仿真与真实 QML 按钮/CSV 绑定。UI smoke test 使用透明窗口生成 `build/mpc-ui-preview.png` 和 `build/mpc-ui-smoke.csv`，不会占用交互界面。

## 构建与运行

推荐用独立 Release 构建验证默认 50 Hz 配置，避免改变上位机现有 Debug 构建。PowerShell：

```powershell
$env:PATH = 'C:/Qt/6.11.1/mingw_64/bin;C:/Qt/Tools/mingw1310_64/bin;' + $env:PATH
cmake -S examples/lateral_mpc -B build/lateral-release -G Ninja `
  -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/mingw_64 -DCMAKE_BUILD_TYPE=Release
cmake --build build/lateral-release
ctest --test-dir build/lateral-release --output-on-failure
./build/lateral-release/lateral_mpc_demo.exe straight build/lateral-release/straight.csv
./build/lateral-release/lateral_mpc_demo.exe circle build/lateral-release/circle.csv
./build/lateral-release/lateral_mpc_demo.exe lane-change build/lateral-release/lane-change.csv
```

也可 `cmake --build build` 构建整个 ADASMaster。Debug 下稠密计算较慢，默认 15 ms 预算可能触发保护停机；功能回归用 50 ms 采样周期/45 ms 预算，默认 20 ms 周期在独立 Release 示例中验证。示例虚拟推进 15 秒，不代表 Windows 实际线程调度或实车性能。

## 控制模型

采用 SI 单位，左转/逆时针为正。`xM/yM` 是场地 ENU 平面质心坐标；`yawRad` 是车身朝向，不能直接用有侧滑时的 GNSS 航迹角代替。`lateralSpeedMps` 是车身坐标系左向速度，`yawRateRadS` 来自融合估计器。GNSS 天线坐标要根据安装偏移及姿态转换到质心。

状态为 `x=[ey, epsi, vy, r, delta]`，其中 `delta` 是实际等效前轮角。控制输入 `u` 是等效前轮目标角。

```text
ey_dot   = vy + v * epsi
epsi_dot = r - v * kappa
vy_dot   = -(Cf+Cr)/(m*v) * vy
           + ((Cr*lr-Cf*lf)/(m*v)-v) * r + Cf/m * delta
r_dot    = (Cr*lr-Cf*lf)/(Iz*v) * vy
           - (Cf*lf^2+Cr*lr^2)/(Iz*v) * r + Cf*lf/Iz * delta
delta_dot = (u-delta)/tau
```

`Cf/Cr` 是整个车轴的侧偏刚度，单位 N/rad。等效前轮角是自行车模型输入，不是左右车轮中某一轮的实际角度。

默认 3 m/s 以下改用低速模型：

```text
ey_dot   = v * epsi + v * lr/L * delta
epsi_dot = v/L * delta - v * kappa
delta_dot = (u-delta)/tau
```

低速模型中 `vy/r` 不参与状态代价，不用 `1/v` 动力学项。低于 0.3 m/s、倒车或高于设定最大速度时返回失败，外壳请求停机。速度在一个预瞄区间内视为常值；模型切换为硬阈值，实车需要验证阈值附近性能。

连续模型通过双线性变换离散。由 `X=F*x0+G*U+d(kappa)` 消元预测状态，得到正定凸 QP：

```text
J = Σ (x[k+1]-xref[k+1])'Qk(x[k+1]-xref[k+1])
    + R*(u[k]-delta_ff[k+1])^2
    + Rd*(u[k]-u[k-1])^2
```

`u[-1]` 使用上次被传输层接受的目标角，第一次使用编码器测量值。曲率稳态参考：

```text
r_ref     = v*kappa
vy_ref    = (lr - lf*m*v^2/(L*Cr))*r_ref
epsi_ref  = -vy_ref/v
delta_ff  = (L + m/L*(lr/Cf-lf/Cr)*v^2)*kappa
```

质心沿曲线运动时车身朝向通常与路径切线存在侧滑角差，所以 `epsi_ref` 并非总为零。低速参考相应使用 `vy_ref=v*lr*kappa` 和 `delta_ff=L*kappa`。

动力学预测横向加速度为 `ay=vy_dot+v*r`；低速为 `ay=v^2/L*delta`。约束适用于预测模型，不能保证实车低附着时的实际加速度。当前实现采用硬约束，没有松弛变量；无法收敛或不可满足时停止下发，而不是沿用旧 MPC 输出。

QP 的形式和残差判停参考 [OSQP 求解器文档](https://osqp.org/docs/solver/index.html)及其 [MPC 示例](https://osqp.org/docs/examples/mpc.html)。本工程使用自行实现的 ADMM，未链接 OSQP，未实现不可行证书；`IterationLimit/TimeLimit` 表示求解失败，不能解释为已证明不可行。需要更大规模、稀疏求解或认证时，可替换这一数值层。

## 参数与标定

以下全部是示例参数，需替换为实测/辨识结果：

| 参数 | 默认值 | 说明 |
|---|---:|---|
| 质量 / 横摆惯量 | 1600 kg / 2700 kg·m² | 包含机器人、安装件和载荷 |
| 前/后轴到质心 | 1.2 / 1.6 m | 轴距 2.8 m |
| 前/后轴侧偏刚度 | 80000 / 80000 N/rad | 两轮合计 |
| 转向传动比 | 16 | 方向盘角 / 等效前轮角 |
| 执行器时间常数 | 80 ms | 综合位置伺服响应；不含显式纯延迟 |
| 采样周期 / 预测步数 | 20 ms / 30 | 50 Hz，0.6 s 预瞄 |
| 最大前轮角 / 编码器角 | ±25° / ±540° | 两者映射后的交集作为约束 |
| 最大方向盘目标变化率 | 600°/s | 驱动器也必须执行此限值 |
| 最大预测横向加速度 | 3 m/s² | 按场地与附着条件设定 |
| 最大转矩 | 20 N·m | 反馈监测与命令限值；不是力矩控制器 |
| 状态/反馈超时 | 100 / 100 ms | 与本地单调时钟比较 |
| MPC 预算 / 迭代上限 | 15 ms / 3000 | 超时或不收敛停止输出 |

映射为：

```text
frontAngle = steeringSign * (robotEncoderAngle - steeringZero) / steeringRatio
robotTarget = steeringZero + steeringSign * steeringRatio * frontTarget
```

`steeringSign` 仅允许 ±1，`steeringZero` 是直行时编码器读数。实际车辆的可变转向传动比、机械间隙、柔性、摩擦和 EPS 助力未单独建模；若不能由位置伺服和简单标定覆盖，应增加查表或辨识模型。

调参顺序建议为：先验证角度方向/零位/限位，再辨识位置伺服响应，随后辨识车辆动力学，最后调整 Q/R/Rd 和预瞄时间。使用质心横向误差评估，而非只检查方向盘目标是否平滑。

## 接入硬件

派生 `ISteeringRobot` 并实现：

1. `requestEnable()`：请求角度控制模式和使能，清除旧目标并以当前实测位置初始化伺服目标；返回值表示请求接受。只有后续反馈 `enabled=true` 才允许 `start()`。
2. `sendAngleCommand()`：把弧度转换为厂商要求的单位，发送序号、角度、角速度/转矩限值及期限。返回 `true` 表示传输层接受，不能代替设备执行确认。
3. `requestStop(emergency, reason)`：按设备协议停止/解除使能；不能将停机实现为方向盘自动回正。物理安全系统决定急停时的电机状态。
4. 收到编码器、速度、转矩、在线/故障/急停状态后发 `feedbackReceived()`。

使用方法：

```cpp
lateral::SteeringRobotController controller;
lateral::VehicleParameters vehicle;  // 填入已标定的参数
lateral::MpcSettings settings;
QString error;
controller.configure(vehicle, settings, &error);
controller.setReferencePath(referencePoints, &error);
controller.setRobot(robotAdapter);

// 在同一个控制线程：先更新本地单调时钟时间戳，再执行状态机。
controller.updateVehicleState(fusedCgState);
// 编码器反馈通过 robotAdapter->feedbackReceived() 接入。
controller.arm(monotonicNowMs);       // 先请求使能
// 等待实际 enabled 反馈到达后：
controller.start(monotonicNowMs);
// 周期任务，每 settings.sampleTimeS 调用：
controller.processOnce(monotonicNowMs);
```

上述所有返回值均需检查，失败原因从 `faultReason()` 或配置/路径 `error` 读取。

`ReferencePoint` 需要已平滑的 `sM/xM/yM/headingRad/curvaturePerM`。弧长从 0 开始、严格递增，位置与弧长相符。任意折线航点必须先生成可行的平滑车辆轨迹；现有目标物航点不能直接当成主车轨迹。路径不做碰撞检测或避障。初次使用全局投影，此后在有限弧长窗口内跟踪；重复/交叉路径初始支路需要上层规划保证无歧义。

Qt/线程要求：控制器、调度调用和协议适配逻辑应在同一控制工作线程内串行执行。跨线程传感器使用 Qt 排队信号传入，所有时间戳统一到一个本地单调时钟。`QElapsedTimer` 或统一时钟服务均可，不混用 UTC/GNSS 时间。通信适配器不得阻塞求解线程，不得积压旧反馈/旧命令。

现有 `AppController` 提供的控制器实例具有 GUI 线程父对象，适合先进行同线程软件接线验证。实车接入时应在专用线程中创建控制器和协议适配器，把诊断用排队信号复制回上位机显示对象，不能跨线程直接读取工作线程的 Qt 属性。`QTimer::PreciseTimer` 可以用于软件仿真调度，但 Windows/Qt 不保证硬实时。

## 故障行为和实车边界

```text
Disabled --arm--> Armed --使能反馈+start--> Running
Running/Armed --异常--> Fault
任意状态 --急停--> Emergency
Fault/Emergency --急停解除+已停用+显式resetFault--> Disabled
```

普通 `stop()`、急停信号解除、重新调用 `arm()` 均不能解除故障锁存。已接入适配器时，复位还要求新鲜安全反馈及 `enabled=false`；缺少轨迹或定位可以先退回 Disabled 再配置，重新 arm 时仍完整校验。控制器销毁时对仍运行的适配器请求停机。求解失败、状态/反馈过期、时间戳倒退、定位无效、超转矩、拒收指令和调度迟到都会触发停止请求。

默认命令有效期为本轮开始后 30 ms；设备侧必须在到期、通信丢失或主机冻结时停止接受旧目标。Qt 事件线程完全阻塞时，软件自身无法及时发送停止请求，因此设备侧看门狗及独立硬件急停是接入条件。当前只提供接口契约，没有实际 CAN/EtherCAT/UDP/SDK 协议驱动，也没有代替驱动器实现上述保护。

轨迹到达末端、参考稳态转角超出机械限位、参考曲率要求的 `v²*|kappa|` 超过限值或运行速度过低时，会请求停止横向控制；上层必须同步管理纵向减速和制动。该模块不负责停车距离规划，也不会把超速轨迹自动修正为安全轨迹。

该实现为可编译、可仿真的控制原型，尚未经实际机器人、SIL/HIL 故障覆盖和场地实车验证。线性小侧滑模型不覆盖轮胎饱和、低附着、倒车或大姿态误差；参数与运行包络必须结合具体车辆确定。
