# 功能：可复用的 Qt Core 横向控制库，与上位机 GUI 和硬件协议解耦。
add_library(adasmaster_lateral STATIC
    LateralControlTypes.h LateralControlTypes.cpp
    ReferencePath.h ReferencePath.cpp
    DenseQpSolver.h DenseQpSolver.cpp
    LateralMpc.h LateralMpc.cpp
    ISteeringRobot.h
    SteeringRobotController.h SteeringRobotController.cpp
    SimulationPlant.h
    MpcSimulationModel.h MpcSimulationModel.cpp
)
get_filename_component(ADASMASTER_LATERAL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
target_include_directories(adasmaster_lateral PUBLIC "${ADASMASTER_LATERAL_ROOT}")
target_link_libraries(adasmaster_lateral PUBLIC Qt6::Core)
