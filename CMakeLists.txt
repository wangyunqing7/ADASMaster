cmake_minimum_required(VERSION 3.16)

# =============================================================================
# ADASMaster —— ADAS 场地测试主控系统
# 工控上位机：驾驶机器人/目标车/RTK/安全系统统一调度，执行 NCAP/ADAS 工况。
# 技术栈：C++17 + Qt6 + QML(手写) + CMake + Ninja
# =============================================================================

project(ADASMaster VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Quick Qml Sql Test Network)

# 横向 MPC 可单独链接；只依赖 Qt Core，不依赖 GUI、车辆协议或第三方 QP 库。
add_subdirectory(src/control)

add_executable(lateral_mpc_demo examples/lateral_mpc/main.cpp)
target_link_libraries(lateral_mpc_demo PRIVATE adasmaster_lateral)

# Qt >= 6.10（qt_standard_project_setup 要求），项目使用现代 QML 模块系统
qt_standard_project_setup(REQUIRES 6.10)

# 显式声明 Theme.qml 为 QML 单例（仅靠 pragma Singleton 在某些版本下不被自动识别）
set_source_files_properties(qml/theme/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

# -----------------------------------------------------------------------------
# 可执行目标 + C++ 源码（含所有含 Q_OBJECT/Q_GADGET/Q_NAMESPACE 的头文件，
# 以便 AUTOMOC 生成元对象代码）
# -----------------------------------------------------------------------------
add_library(adasmaster_core STATIC
    src/config/DeviceConfig.h
    src/config/ScenarioParser.h
    src/config/ScenarioParser.cpp
    src/config/ConfigManager.h
    src/config/ConfigManager.cpp

    src/ncap/TestResult.h
    src/ncap/NcapEvaluator.h
    src/ncap/NcapEvaluator.cpp

    src/persistence/PersistenceTypes.h
    src/persistence/DatabaseManager.h
    src/persistence/DatabaseManager.cpp
    src/persistence/DataLogger.h
    src/persistence/DataLogger.cpp
    src/persistence/ReplayListModel.h
    src/persistence/ReplayListModel.cpp

    src/app/AppController.h
    src/app/AppController.cpp

    src/device/DeviceStatus.h
    src/device/IDevice.h
    src/device/FakeDevice.h
    src/device/FakeDevice.cpp
    src/device/DeviceListModel.h
    src/device/DeviceListModel.cpp
    src/device/DeviceManager.h
    src/device/DeviceManager.cpp

    src/auth/UserAccount.h
    src/auth/AuthManager.h
    src/auth/AuthManager.cpp

    src/safety/SafetyStatus.h
    src/safety/SafetyManager.h
    src/safety/SafetyManager.cpp

    src/scenario/ScenarioStatus.h
    src/scenario/Waypoint.h
    src/scenario/ScenarioConfig.h
    src/scenario/ScenarioCatalog.h
    src/scenario/ScenarioCatalog.cpp
    src/scenario/TrajectorySegment.h
    src/scenario/TrajectoryGeometry.h
    src/scenario/TrajectoryGeometry.cpp
    src/scenario/ScenarioEngine.h
    src/scenario/ScenarioEngine.cpp

    src/alarm/EventLog.h
    src/alarm/AlarmListModel.h
    src/alarm/AlarmListModel.cpp
    src/alarm/AlarmFilterProxyModel.h
    src/alarm/AlarmFilterProxyModel.cpp
    src/alarm/AlarmManager.h
    src/alarm/AlarmManager.cpp

    src/state/StateManager.h
    src/state/StateManager.cpp
)

target_include_directories(adasmaster_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(adasmaster_core PUBLIC adasmaster_lateral Qt6::Core Qt6::Sql Qt6::Network)

qt_add_executable(ADASMaster
    main.cpp
    tests/MpcUiSmoke.h
    app_icon.rc
)

# -----------------------------------------------------------------------------
# QML 模块（编译期注册，main.cpp 用 loadFromModule("ADASMaster","Main") 加载）
# 新增 .qml 必须登记到 QML_FILES，否则运行时无法加载。
# -----------------------------------------------------------------------------
qt_add_qml_module(ADASMaster
    URI ADASMaster
    QML_FILES
        qml/Main.qml
        qml/theme/Theme.qml
        qml/components/TopStatusBar.qml
        qml/components/SideNavBar.qml
        qml/components/BottomLogBar.qml
        qml/components/StatusLamp.qml
        qml/components/EmergencyButton.qml
        qml/components/DeviceCard.qml
        qml/components/Icon.qml
        qml/components/IconButton.qml
        qml/components/PanelFrame.qml
        qml/components/StatusCard.qml
        qml/components/MetricCell.qml
        qml/components/StateFlow.qml
        qml/components/VehiclePlatformStatusMonitor.qml
        qml/components/PathPreviewMap.qml
        qml/components/WaypointEditor.qml
        qml/components/TrajectorySegmentEditor.qml
        qml/components/MpcLineChart.qml
        qml/components/MpcTrajectoryView.qml
        qml/pages/DevicePage.qml
        qml/pages/MapPage.qml
        qml/pages/ScenarioPage.qml
        qml/pages/AlarmPage.qml
        qml/pages/SettingsPage.qml
        qml/pages/LoginView.qml
        qml/pages/MpcPage.qml
    RESOURCES
        config/app_config.json
        config/app_icon.ico
        config/devices.json
        config/users.json
        scenarios/AEB_CCR_50.json
        scenarios/AEB_CCRs_30.json
        scenarios/AEB_CCRs_40.json
        scenarios/AEB_CCRm_50.json
        scenarios/AEB_CCRb_50.json
        scenarios/AEB_VRU_CPNA_50.json
        scenarios/AEB_VRU_CBNA_50.json
        scenarios/FCW_CCRs_75.json
        scenarios/RCTA_01.json
        scenarios/LKA_01.json
        qml/icons/dashboard.svg
        qml/icons/device.svg
        qml/icons/map.svg
        qml/icons/scenario.svg
        qml/icons/test.svg
        qml/icons/replay.svg
        qml/icons/alarm.svg
        qml/icons/settings.svg
        qml/icons/load.svg
        qml/icons/check.svg
        qml/icons/arm.svg
        qml/icons/start.svg
        qml/icons/stop.svg
        qml/icons/estop.svg
        qml/icons/reset.svg
        qml/icons/normal.svg
        qml/icons/warning.svg
        qml/icons/error.svg
        qml/icons/emergency.svg
        qml/icons/power.svg
        qml/icons/logout.svg
)

target_link_libraries(ADASMaster PRIVATE
    adasmaster_core
    Qt6::Quick
    Qt6::Qml
)

enable_testing()

function(add_adasmaster_test target source)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE adasmaster_core Qt6::Test)
    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endfunction()

add_adasmaster_test(test_config tests/TestConfig.cpp)
add_adasmaster_test(test_state_machine tests/TestStateMachine.cpp)
add_adasmaster_test(test_safety tests/TestSafety.cpp)
add_adasmaster_test(test_database tests/TestDatabase.cpp)
add_adasmaster_test(test_data_logger tests/TestDataLogger.cpp)
add_adasmaster_test(test_alarm_persistence tests/TestAlarmPersistence.cpp)
add_adasmaster_test(test_replay_list_model tests/TestReplayListModel.cpp)
add_adasmaster_test(test_app_controller tests/TestAppController.cpp)
add_adasmaster_test(test_device_telemetry tests/TestDeviceTelemetry.cpp)
add_adasmaster_test(test_auth tests/TestAuth.cpp)
add_adasmaster_test(test_scenario_parser tests/TestScenarioParser.cpp)
add_adasmaster_test(test_waypoint_simulation tests/TestWaypointSimulation.cpp)
add_adasmaster_test(test_scenario_catalog tests/TestScenarioCatalog.cpp)
add_adasmaster_test(test_trajectory_geometry tests/TestTrajectoryGeometry.cpp)
add_adasmaster_test(test_ncap_evaluator tests/TestNcapEvaluator.cpp)
add_adasmaster_test(test_lateral_mpc tests/TestLateralMpc.cpp)
add_adasmaster_test(test_mpc_simulation tests/TestMpcSimulation.cpp)

add_test(NAME qml_lint_gate
    COMMAND powershell -NoProfile -ExecutionPolicy Bypass
            -File ${CMAKE_CURRENT_SOURCE_DIR}/tests/CheckQmlLint.ps1)
set_tests_properties(qml_lint_gate PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_test(NAME status_monitor_gate
    COMMAND powershell -NoProfile -ExecutionPolicy Bypass
            -File ${CMAKE_CURRENT_SOURCE_DIR}/tests/CheckStatusMonitor.ps1)
set_tests_properties(status_monitor_gate PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_test(NAME scenario_management_gate
    COMMAND powershell -NoProfile -ExecutionPolicy Bypass
            -File ${CMAKE_CURRENT_SOURCE_DIR}/tests/CheckScenarioManagement.ps1)
set_tests_properties(scenario_management_gate PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_test(NAME login_gate
    COMMAND powershell -NoProfile -ExecutionPolicy Bypass
            -File ${CMAKE_CURRENT_SOURCE_DIR}/tests/CheckLogin.ps1)
set_tests_properties(login_gate PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_test(NAME startup_smoke
    COMMAND powershell -NoProfile -ExecutionPolicy Bypass
            -File ${CMAKE_CURRENT_SOURCE_DIR}/tests/CheckStartup.ps1)
set_tests_properties(startup_smoke PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_test(NAME mpc_ui_smoke
    COMMAND powershell -NoProfile -ExecutionPolicy Bypass
            -File ${CMAKE_CURRENT_SOURCE_DIR}/tests/CheckMpcUi.ps1)
set_tests_properties(mpc_ui_smoke PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    TIMEOUT 20)

set_target_properties(ADASMaster PROPERTIES
    WIN32_EXECUTABLE TRUE    # 无控制台窗口的 GUI 程序；调试时改 FALSE 可看 qWarning
    MACOSX_BUNDLE TRUE
)

install(TARGETS ADASMaster
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)
