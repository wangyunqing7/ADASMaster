# ADASMaster Safety and SQLite Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Turn the current simulated demo into a deterministic, testable prototype with safe state transitions, JSON-backed configuration, SQLite audit data, real replay records, and a clean QML quality gate.

**Architecture:** Keep the existing AppController/Manager/QML boundary. Add ConfigManager and ScenarioParser as the single configuration source, DatabaseManager/DataLogger as the persistence boundary, and ReplayListModel for read-only UI queries; update ScenarioEngine, DeviceManager, and SafetyManager in place to close the control loop.

**Tech Stack:** C++17, Qt 6.11 Core/Quick/Qml/Sql/Test, QML, SQLite, CMake, Ninja, CTest

---

## File map

- `src/config/DeviceConfig.h`: typed device configuration.
- `src/config/ScenarioParser.*`: JSON-to-ScenarioConfig validation.
- `src/config/ConfigManager.*`: path resolution and authoritative config collections.
- `src/persistence/DatabaseManager.*`: SQLite lifecycle, schema migration, run/sample/event operations.
- `src/persistence/DataLogger.*`: run lifecycle and batched 10Hz sample persistence.
- `src/persistence/ReplayListModel.*`: QML model backed by `test_runs`.
- `src/scenario/ScenarioEngine.*`: guarded state machine, member countdown timer, device/data lifecycle.
- `src/device/DeviceManager.*`: config-driven devices and required-device health checks.
- `src/safety/SafetyManager.*`: physical-source-aware reset semantics.
- `tests/*.cpp`: Qt Test coverage.
- `tests/CheckQmlLint.ps1`: QML warning-category quality gate.

### Task 1: Introduce the core test target

**Files:**
- Modify: `CMakeLists.txt`
- Create: `tests/TestConfig.cpp`

- [ ] Add `Sql` and `Test` to `find_package`, enable CTest, and extract existing C++ sources into `adasmaster_core` so the GUI and tests use the same production code.
- [ ] Add `adasmaster_tests` linked to `adasmaster_core` and `Qt6::Test`; register it with `add_test(NAME adasmaster_tests COMMAND adasmaster_tests)`.
- [ ] Add an initial failing test that expects `ScenarioParser::parseFile()` to load `scenarios/AEB_CCR_50.json`.
- [ ] Configure and build; expected RED result is compilation failure because `ScenarioParser` does not exist.

### Task 2: Implement authoritative JSON configuration

**Files:**
- Create: `src/config/DeviceConfig.h`
- Create: `src/config/ScenarioParser.h`
- Create: `src/config/ScenarioParser.cpp`
- Create: `src/config/ConfigManager.h`
- Create: `src/config/ConfigManager.cpp`
- Modify: `src/scenario/ScenarioConfig.h`
- Modify: `config/devices.json`
- Modify: `CMakeLists.txt`
- Test: `tests/TestConfig.cpp`

- [ ] Test valid AEB/RCTA/LKA parsing, required device flags, duplicate names, and missing numeric fields.
- [ ] Verify RED with targeted Qt Test functions.
- [ ] Implement strict parsing with field-path errors and retain compact original JSON as `rawJson`.
- [ ] Implement path priority: explicit root/`ADASMASTER_DATA_DIR`, AppConfigLocation, then QRC defaults.
- [ ] Replace DeviceManager hardcoded construction with `configure(const QList<DeviceConfig>&)` and inject parsed scenarios into ScenarioEngine.
- [ ] Verify all configuration tests GREEN.

### Task 3: Close device, safety, and scenario state transitions

**Files:**
- Modify: `src/device/DeviceManager.h`
- Modify: `src/device/DeviceManager.cpp`
- Modify: `src/device/DeviceListModel.*`
- Modify: `src/safety/SafetyManager.h`
- Modify: `src/safety/SafetyManager.cpp`
- Modify: `src/scenario/ScenarioEngine.h`
- Modify: `src/scenario/ScenarioEngine.cpp`
- Create: `tests/TestStateMachine.cpp`
- Create: `tests/TestSafety.cpp`

- [ ] Write failing tests for duplicate start, non-Idle load, required-device checks, startAll, all termination paths, and physical-source reset rejection.
- [ ] Verify RED before each behavior group.
- [ ] Replace dynamic countdown timers with one member timer and `countdownActive` guard.
- [ ] Add required-device health methods and immediate model updates from `statusChanged`.
- [ ] Add `startAll()` and centralize controlled stop/emergency cleanup.
- [ ] Add `updateHardwareEStop(bool)`, `updateWirelessEStop(bool)`, and boolean `requestReset()` with a failure reason.
- [ ] Branch the simulation result logic by AEB/RCTA/LKA type and mark outputs as simulated.
- [ ] Verify state-machine and safety tests GREEN.

### Task 4: Add SQLite schema and repositories

**Files:**
- Create: `src/persistence/PersistenceTypes.h`
- Create: `src/persistence/DatabaseManager.h`
- Create: `src/persistence/DatabaseManager.cpp`
- Create: `tests/TestDatabase.cpp`
- Modify: `CMakeLists.txt`

- [ ] Write failing tests for migration, run creation/completion, sample batch insertion, event insertion/acknowledgement, rollback, and run listing.
- [ ] Verify RED because DatabaseManager does not exist.
- [ ] Implement uniquely named QSQLITE connections, WAL/foreign keys/busy timeout, schema version 1, prepared statements, and transactional batch writes.
- [ ] Ensure destructor closes the database and removes the named connection after all queries are destroyed.
- [ ] Verify database tests GREEN using `QTemporaryDir`.

### Task 5: Persist run samples and alarms

**Files:**
- Create: `src/persistence/DataLogger.h`
- Create: `src/persistence/DataLogger.cpp`
- Modify: `src/alarm/EventLog.h`
- Modify: `src/alarm/AlarmListModel.*`
- Modify: `src/alarm/AlarmManager.*`
- Modify: `src/scenario/ScenarioEngine.*`
- Modify: `src/app/AppController.*`
- Create: `tests/TestDataLogger.cpp`

- [ ] Write failing tests for begin/capture/finish, emergency completion, event persistence, acknowledgement persistence, and the 1000-row alarm capacity boundary.
- [ ] Verify RED.
- [ ] Implement DataLogger with a 10-sample buffer and explicit flush on every terminal path.
- [ ] Inject DatabaseManager/DataLogger into ScenarioEngine and AlarmManager; maintain current run id.
- [ ] Fix AlarmListModel so insert and capacity removal notifications are not nested.
- [ ] On persistence failure, emit a fatal signal; ScenarioEngine performs a controlled stop and records `DATA_ERROR` to Qt logging.
- [ ] Verify logger and alarm tests GREEN.

### Task 6: Replace fake replay data with SQLite queries

**Files:**
- Create: `src/persistence/ReplayListModel.h`
- Create: `src/persistence/ReplayListModel.cpp`
- Modify: `src/app/AppController.*`
- Modify: `qml/pages/DataReplayPage.qml`
- Create: `tests/TestReplayListModel.cpp`
- Modify: `CMakeLists.txt`

- [ ] Write a failing model-role test using two temporary database runs.
- [ ] Verify RED because ReplayListModel does not exist.
- [ ] Implement descending run queries and roles for id/time/scenario/status/result/operator/duration.
- [ ] Expose the model through AppController and refresh it after DataLogger finishes a run.
- [ ] Replace QML's embedded records with the backend model; disable report export with a clear unavailable label.
- [ ] Verify model tests and GUI build GREEN.

### Task 7: Remove critical QML lint categories

**Files:**
- Modify: `qml/components/DeviceCard.qml`
- Modify: `qml/components/IconButton.qml`
- Modify: `qml/components/PanelFrame.qml`
- Modify: `qml/components/StateFlow.qml`
- Modify: `qml/pages/DashboardPage.qml`
- Modify: `qml/pages/DevicePage.qml`
- Modify: `qml/pages/MapPage.qml`
- Modify: `qml/pages/ScenarioPage.qml`
- Modify: `qml/pages/TestPage.qml`
- Modify: `qml/pages/DataReplayPage.qml`
- Modify: `qml/pages/AlarmPage.qml`
- Modify: `qml/pages/SettingsPage.qml`
- Create: `tests/CheckQmlLint.ps1`
- Modify: `CMakeLists.txt`

- [ ] Add a failing PowerShell check that runs `all_qmllint` and rejects `missing-property`, `Quick.layout-positioning`, or `property-override`.
- [ ] Verify RED against current warnings.
- [ ] Rename `MapPage.scale` to `pixelsPerMeter`.
- [ ] Give Buttons ids and reference their own text/down/enabled properties.
- [ ] Replace layout-owned width/height with `Layout.preferred*` or implicit sizes.
- [ ] Remove the duplicate IconButton enabled property and unused imports.
- [ ] Verify the three critical warning categories are zero.

### Task 8: Integrate startup, failure states, and final verification

**Files:**
- Modify: `src/app/AppController.h`
- Modify: `src/app/AppController.cpp`
- Modify: `qml/components/TopStatusBar.qml`
- Modify: `qml/pages/TestPage.qml`
- Modify: `CMakeLists.txt`

- [ ] Initialize ConfigManager, DatabaseManager, managers, DataLogger, and ReplayListModel in dependency order.
- [ ] Expose persistence readiness/error properties and disable check/arm/start when configuration or database is unavailable.
- [ ] Verify qsqlite driver deployment exists at `build/sqldrivers/qsqlite.dll`.
- [ ] Run `cmake --build build`, `ctest --test-dir build --output-on-failure`, `cmake --build build --target all_qmllint`, the sidebar regression check, and an offscreen startup smoke test.
- [ ] Confirm the generated EXE is newer than all modified sources.

