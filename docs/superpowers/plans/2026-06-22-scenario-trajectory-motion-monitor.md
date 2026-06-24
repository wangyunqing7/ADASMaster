# Scenario Structure, Parametric Trajectory, and Motion Monitor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add persistent three-level scenario structure creation, straight/arc parametric trajectory editing, and two selectable motion-telemetry device cards.

**Architecture:** Introduce two focused C++ units: `ScenarioCatalog` owns persistent hierarchy mutations and `TrajectoryGeometry` owns deterministic geometry/kinematics calculations. Keep `ScenarioEngine` consuming generated waypoints so existing execution behavior remains compatible. Extend the generic device status/model with motion capabilities and telemetry; QML binds only through `AppController` and models.

**Tech Stack:** C++17, Qt 6 Core/Quick/QML/Test, QJson, QSettings, CMake/Ninja, QtTest.

---

## File map

- Create `src/scenario/ScenarioCatalog.h/.cpp`: hierarchy nodes, validation, persistence, tree projection.
- Create `src/scenario/TrajectorySegment.h`: segment type and persisted fields.
- Create `src/scenario/TrajectoryGeometry.h/.cpp`: segment validation, derived values, rotation, propagation, waypoint sampling.
- Create `tests/TestScenarioCatalog.cpp`: hierarchy and persistence tests.
- Create `tests/TestTrajectoryGeometry.cpp`: straight/arc math and continuity tests.
- Create `qml/components/TrajectorySegmentEditor.qml`: parameter editor for ordered straight/arc segments.
- Modify `src/scenario/ScenarioConfig.h`: parametric ego/target segment lists.
- Modify `src/config/ScenarioParser.h/.cpp`: parse and serialize parametric trajectories while retaining waypoints compatibility.
- Modify `src/config/ConfigManager.h/.cpp`: own catalog and save user scenarios.
- Modify `src/app/AppController.h/.cpp`: expose catalog mutations and trajectory load/save APIs.
- Modify `src/config/DeviceConfig.h`, `config/devices.json`: motion telemetry capability.
- Modify `src/device/DeviceStatus.h`, `FakeDevice.cpp`, `DeviceListModel.h/.cpp`, `DeviceManager.cpp`: generic motion telemetry.
- Modify `qml/pages/ScenarioPage.qml`: top create menu, three-level context menus, parametric editor wiring.
- Modify `qml/components/PathPreviewMap.qml`: render sampled straight/arc output read-only.
- Modify `qml/components/VehiclePlatformStatusMonitor.qml`: two independent motion-device slots.
- Modify `tests/TestConfig.cpp`, `tests/TestDeviceTelemetry.cpp`, `tests/TestAppController.cpp`, `tests/CheckStatusMonitor.ps1`: integration and UI gates.
- Modify `CMakeLists.txt`: register all new source, test, and QML files.

## Task 1: Persistent scenario catalog

**Files:**
- Create: `src/scenario/ScenarioCatalog.h`
- Create: `src/scenario/ScenarioCatalog.cpp`
- Create: `tests/TestScenarioCatalog.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write failing hierarchy tests**

Create tests that instantiate a temporary catalog, import the three bundled scenarios, then verify:

```cpp
void createsStrictThreeLevelNodes()
{
    QTemporaryDir dir;
    ScenarioCatalog catalog(dir.filePath("scenario_catalog.json"));
    QVERIFY(catalog.load({}));
    const QString groupId = catalog.createNode(ScenarioNodeKind::Group, {}, "AEB");
    QVERIFY(!groupId.isEmpty());
    const QString subgroupId = catalog.createNode(ScenarioNodeKind::Subgroup, groupId, "CCRs");
    QVERIFY(!subgroupId.isEmpty());
    const QString scenarioId = catalog.createNode(ScenarioNodeKind::Scenario, subgroupId, "CCRs 50");
    QVERIFY(!scenarioId.isEmpty());
    QVERIFY(catalog.createNode(ScenarioNodeKind::Scenario, groupId, "Invalid").isEmpty());
    QVERIFY(catalog.createNode(ScenarioNodeKind::Scenario, subgroupId, "CCRs 50").isEmpty());
}

void persistsEmptyGroupsAndSubgroups()
{
    QTemporaryDir dir;
    const QString path = dir.filePath("scenario_catalog.json");
    {
        ScenarioCatalog catalog(path);
        QVERIFY(catalog.load({}));
        const QString groupId = catalog.createNode(ScenarioNodeKind::Group, {}, "VRU");
        QVERIFY(!catalog.createNode(ScenarioNodeKind::Subgroup, groupId, "Pedestrian").isEmpty());
        QVERIFY(catalog.save());
    }
    ScenarioCatalog reloaded(path);
    QVERIFY(reloaded.load({}));
    QCOMPARE(reloaded.tree().size(), 1);
    QCOMPARE(reloaded.tree().first().toMap().value("name").toString(), QString("VRU"));
}
```

- [ ] **Step 2: Register and run the test to verify RED**

Add `test_scenario_catalog` with `add_adasmaster_test`, configure, and run:

```powershell
cmake --build build --target test_scenario_catalog
ctest --test-dir build -R test_scenario_catalog --output-on-failure
```

Expected: compile failure because `ScenarioCatalog` is missing.

- [ ] **Step 3: Implement the catalog**

Define:

```cpp
enum class ScenarioNodeKind { Group, Subgroup, Scenario };

struct ScenarioNode {
    QString id;
    QString parentId;
    QString name;
    ScenarioNodeKind kind = ScenarioNodeKind::Group;
    int order = 0;
};

class ScenarioCatalog : public QObject {
    Q_OBJECT
public:
    explicit ScenarioCatalog(const QString &storagePath, QObject *parent = nullptr);
    bool load(const QList<ScenarioConfig> &bundled, QString *error = nullptr);
    bool save(QString *error = nullptr) const;
    QString createNode(ScenarioNodeKind kind, const QString &parentId,
                       const QString &name, QString *error = nullptr);
    QVariantList tree() const;
    QString nodeIdForScenario(const QString &scenarioName) const;
signals:
    void changed();
private:
    bool validParent(ScenarioNodeKind kind, const QString &parentId) const;
    QList<ScenarioNode> m_nodes;
    QString m_storagePath;
};
```

Persist JSON as `{ "version": 1, "nodes": [...] }` using `QSaveFile`. Generate IDs with `QUuid::createUuid().toString(QUuid::WithoutBraces)`. Reject empty names, invalid parent kinds, and case-insensitive sibling duplicates. Import bundled scenarios only when an equivalent group/subgroup/scenario path does not already exist.

- [ ] **Step 4: Run the catalog tests to verify GREEN**

Run the commands from Step 2. Expected: all `TestScenarioCatalog` cases pass.

## Task 2: Catalog integration and creation APIs

**Files:**
- Modify: `src/config/ConfigManager.h`
- Modify: `src/config/ConfigManager.cpp`
- Modify: `src/app/AppController.h`
- Modify: `src/app/AppController.cpp`
- Modify: `tests/TestAppController.cpp`

- [ ] **Step 1: Write failing controller tests**

Add a test using temporary data and storage roots:

```cpp
void createsAllScenarioNodeKindsAndPersistsThem()
{
    QTemporaryDir storage;
    AppController controller(QDir::currentPath(), storage.path());
    const QString groupId = controller.createScenarioNode("group", {}, "VRU");
    QVERIFY(!groupId.isEmpty());
    const QString subgroupId = controller.createScenarioNode("subgroup", groupId, "Pedestrian");
    QVERIFY(!subgroupId.isEmpty());
    const QString scenarioId = controller.createScenarioNode("scenario", subgroupId, "Crossing 20");
    QVERIFY(!scenarioId.isEmpty());
    QVERIFY(controller.createScenarioNode("scenario", subgroupId, "Crossing 20").isEmpty());
    QVERIFY(!controller.scenarioTree().isEmpty());

    AppController reloaded(QDir::currentPath(), storage.path());
    const QString serialized = QString::fromUtf8(QJsonDocument::fromVariant(reloaded.scenarioTree()).toJson());
    QVERIFY(serialized.contains("Crossing 20"));
}
```

- [ ] **Step 2: Run the test and verify RED**

```powershell
cmake --build build --target test_app_controller
ctest --test-dir build -R test_app_controller --output-on-failure
```

Expected: compile failure for missing `createScenarioNode`.

- [ ] **Step 3: Wire the catalog through ConfigManager and AppController**

`ConfigManager` owns a `ScenarioCatalog`, initialized with `<storageRoot>/scenario_catalog.json`; `AppController` passes its resolved storage root. Expose:

```cpp
Q_INVOKABLE QVariantList scenarioTree() const;
Q_INVOKABLE QString createScenarioNode(const QString &kind,
                                       const QString &parentId,
                                       const QString &name);
Q_SIGNAL void scenarioTreeChanged();
```

Map `group`, `subgroup`, and `scenario` to the enum. For a new scenario node, create a minimal `ScenarioConfig` whose `group` and `subgroup` are taken from its ancestors, persist it as a sanitized JSON filename under the writable scenario directory, append it to the engine list, then emit `scenarioTreeChanged`.

- [ ] **Step 4: Run controller and config tests**

```powershell
cmake --build build --target test_app_controller test_config
ctest --test-dir build -R "test_(app_controller|config)" --output-on-failure
```

Expected: both test executables pass.

## Task 3: Three-level QML creation UX

**Files:**
- Modify: `qml/pages/ScenarioPage.qml`
- Modify: `tests/CheckQmlLint.ps1`

- [ ] **Step 1: Extend the QML gate so current UI fails**

Add checks for the new contracts:

```powershell
if ($scenario -notmatch 'id:\s*createNodeButton') { throw 'Top create button is missing.' }
if ($scenario -match 'id:\s*btnNewScenario') { throw 'Legacy bottom create button remains.' }
foreach ($kind in @('group','subgroup','scenario')) {
    if ($scenario -notmatch "createScenarioNode\(.*$kind") {
        throw "Missing create action for $kind"
    }
}
```

- [ ] **Step 2: Run the gate and verify RED**

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tests/CheckQmlLint.ps1
```

Expected: failure for missing top create button.

- [ ] **Step 3: Replace the bottom button and hard-coded AEB/RCTA/LKA dialog**

Add a 32 px toolbar above the tree with `createNodeButton`. Its menu contains “新建场景组 / 新建场景子组 / 新建场景”. Replace `newScenarioType` with `nodeKind`, `parentGroup`, and `parentSubgroup`. Use node IDs rather than names when creating.

Add right-button handling to group, subgroup, and scenario delegates. Set context properties (`contextNodeId`, `contextParentId`, `contextKind`) before opening a level-specific menu. On success, refresh the tree, expand the parent, and select the new scenario where applicable.

- [ ] **Step 4: Run QML lint and the UI gate**

```powershell
cmake --build build --target all_qmllint
powershell -NoProfile -ExecutionPolicy Bypass -File tests/CheckQmlLint.ps1
```

Expected: no critical QML lint category and the structural checks pass.

## Task 4: Parametric trajectory geometry

**Files:**
- Create: `src/scenario/TrajectorySegment.h`
- Create: `src/scenario/TrajectoryGeometry.h`
- Create: `src/scenario/TrajectoryGeometry.cpp`
- Create: `tests/TestTrajectoryGeometry.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write failing geometry tests**

Cover a 100 m straight from 0 to 10 km/h, an admissible left arc, invalid radius, whole-track rotation, and propagation:

```cpp
void derivesStraightKinematics()
{
    TrajectorySegment s;
    s.kind = TrajectorySegmentKind::Straight;
    s.start = {0.0, 0.0}; s.end = {100.0, 0.0};
    s.startSpeedMps = 0.0; s.endSpeedMps = 10.0 / 3.6;
    const auto d = TrajectoryGeometry::derive(s);
    QVERIFY(d.valid);
    QCOMPARE(d.lengthM, 100.0);
    QVERIFY(qAbs(d.headingDeg) < 1e-9);
    QVERIFY(qAbs(d.tangentialAccelerationMps2 - 0.0385802469) < 1e-6);
    QVERIFY(qAbs(d.durationSec - 72.0) < 1e-6);
}

void rejectsArcWhoseRadiusIsBelowHalfChord()
{
    TrajectorySegment s;
    s.kind = TrajectorySegmentKind::CircularArc;
    s.start = {0.0, 0.0}; s.end = {10.0, 0.0}; s.radiusM = 4.9;
    QVERIFY(!TrajectoryGeometry::derive(s).valid);
}
```

- [ ] **Step 2: Run and verify RED**

```powershell
cmake --build build --target test_trajectory_geometry
ctest --test-dir build -R test_trajectory_geometry --output-on-failure
```

Expected: missing geometry types.

- [ ] **Step 3: Implement geometry and sampling**

Define segment input fields (`kind`, `start`, `end`, `startSpeedMps`, `endSpeedMps`, `radiusM`, `turnDirection`) and derived result fields (`valid`, `error`, `lengthM`, `headingDeg`, `sweepDeg`, `center`, `durationSec`, `tangentialAccelerationMps2`, `maxLateralAccelerationMps2`).

Use `atan2`, chord geometry, and the minor-arc center selected by left/right direction. Add:

```cpp
static DerivedTrajectorySegment derive(const TrajectorySegment &segment);
static QList<TrajectorySegment> propagate(const QList<TrajectorySegment> &segments);
static QList<TrajectorySegment> rotate(const QList<TrajectorySegment> &segments,
                                       double degrees);
static QList<Waypoint> sample(const QList<TrajectorySegment> &segments,
                              double spacingM = 1.0);
```

Sampling interpolates speed by traveled distance and includes every segment end exactly once.

- [ ] **Step 4: Run geometry tests and verify GREEN**

Run Step 2 commands. Expected: all geometry tests pass.

## Task 5: Trajectory JSON and controller APIs

**Files:**
- Modify: `src/scenario/ScenarioConfig.h`
- Modify: `src/config/ScenarioParser.h`
- Modify: `src/config/ScenarioParser.cpp`
- Modify: `src/config/ConfigManager.h`
- Modify: `src/config/ConfigManager.cpp`
- Modify: `src/app/AppController.h`
- Modify: `src/app/AppController.cpp`
- Modify: `tests/TestScenarioParser.cpp`
- Modify: `tests/TestAppController.cpp`

- [ ] **Step 1: Write failing round-trip tests**

Add JSON with `trajectory.segments` under `ego` and assert parse values plus generated waypoints. Add a controller test that calls `saveScenarioTrajectory(name, actor, segments)` and reloads it.

- [ ] **Step 2: Run parser/controller tests and verify RED**

```powershell
cmake --build build --target test_scenario_parser test_app_controller
ctest --test-dir build -R "test_(scenario_parser|app_controller)" --output-on-failure
```

Expected: missing segment fields/APIs.

- [ ] **Step 3: Implement parse, serialize, and controller conversion**

Add `QList<TrajectorySegment> egoTrajectory` and `targetTrajectory` to `ScenarioConfig`. Parse objects shaped as:

```json
{"trajectory":{"segments":[
  {"kind":"straight","start_x":0,"start_y":0,"end_x":100,"end_y":0,
   "start_speed_mps":0,"end_speed_mps":2.7777778},
  {"kind":"arc","end_x":120,"end_y":20,"end_speed_mps":4,
   "radius_m":20,"turn":"left"}
]}}
```

Generate legacy `egoPath`/`targetPath` with `TrajectoryGeometry::sample` after parsing. Expose QML-friendly `QVariantList scenarioTrajectory(name, actor)` and `bool saveScenarioTrajectory(name, actor, segments)`; validate every segment before writing with `QSaveFile`.

- [ ] **Step 4: Run parser, controller, waypoint, and state-machine tests**

```powershell
cmake --build build --target test_scenario_parser test_app_controller test_waypoint_simulation test_state_machine
ctest --test-dir build -R "test_(scenario_parser|app_controller|waypoint_simulation|state_machine)" --output-on-failure
```

Expected: all pass.

## Task 6: Parametric trajectory QML editor

**Files:**
- Create: `qml/components/TrajectorySegmentEditor.qml`
- Modify: `qml/pages/ScenarioPage.qml`
- Modify: `qml/components/PathPreviewMap.qml`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Add a structural gate and verify RED**

Require `TrajectorySegmentEditor`, “直线段”, “圆弧段”, `radiusM`, `turnDirection`, `rotateDegrees`, and `saveScenarioTrajectory` in the registered QML sources. Run `CheckQmlLint.ps1`; expect failure before the component exists.

- [ ] **Step 2: Implement `TrajectorySegmentEditor`**

The component accepts `segments`, exposes `actor`, and emits `saveRequested(var segments)`. Render each segment as a compact card with type, start values (editable only for first), end values, speed in km/h, and derived read-only distance/angle/acceleration/time. Arc cards add radius and left/right selectors. Provide add-straight, add-arc, copy, delete, move-up, move-down, and whole-track rotation controls.

All numeric edits create a copied JS array rather than mutating bound objects in place. Downstream start/end propagation is performed by the C++ preview API so QML and save validation use identical math.

- [ ] **Step 3: Replace `WaypointEditor` usage and keep map read-only**

Keep the main-car/target-platform tabs. Load segments using `scenarioTrajectory`, preview sampled points in `PathPreviewMap`, and save through `saveScenarioTrajectory`. Remove any map `MouseArea` that mutates geometry; the canvas only paints.

- [ ] **Step 4: Register QML, lint, and build**

```powershell
cmake --build build --target all_qmllint ADASMaster
powershell -NoProfile -ExecutionPolicy Bypass -File tests/CheckQmlLint.ps1
```

Expected: build succeeds and lint gate passes.

## Task 7: Generic motion telemetry

**Files:**
- Modify: `src/config/DeviceConfig.h`
- Modify: `config/devices.json`
- Modify: `src/config/ConfigManager.cpp`
- Modify: `src/device/DeviceStatus.h`
- Modify: `src/device/FakeDevice.cpp`
- Modify: `src/device/DeviceListModel.h`
- Modify: `src/device/DeviceListModel.cpp`
- Modify: `tests/TestConfig.cpp`
- Modify: `tests/TestDeviceTelemetry.cpp`

- [ ] **Step 1: Write failing capability and role tests**

Assert that DrivingRobot and TargetPlatform have `motionTelemetry=true`, other bundled devices do not, and roles expose `deviceMotionCapable`, `deviceX`, `deviceY`, `deviceSpeed`, and `deviceYaw`. Assert a running fake motion device changes X and preserves yaw normalization.

- [ ] **Step 2: Run tests and verify RED**

```powershell
cmake --build build --target test_config test_device_telemetry
ctest --test-dir build -R "test_(config|device_telemetry)" --output-on-failure
```

Expected: missing fields and roles.

- [ ] **Step 3: Implement motion capability and telemetry**

Add `bool motionTelemetry` to `DeviceConfig` and `DeviceStatus`; add `double xM`, `yM`, `speedMps`, `yawDeg` to status. Parse optional JSON boolean `motion_telemetry`, defaulting false. Mark the two movable bundled devices true.

Pass capability into `FakeDevice`. While connected and running, update position with `dt` from the previous tick and the configured/fake speed; keep yaw normalized with `std::fmod`. Add corresponding model roles and role names.

- [ ] **Step 4: Run config/device tests and verify GREEN**

Run Step 2 commands. Expected: all pass.

## Task 8: Two selectable motion-device cards

**Files:**
- Modify: `qml/components/VehiclePlatformStatusMonitor.qml`
- Modify: `tests/CheckStatusMonitor.ps1`

- [ ] **Step 1: Update the static gate and verify RED**

Require two selectors (`primaryDeviceCombo`, `secondaryDeviceCombo`), motion capability filtering, X/Y/speed/yaw roles, km/h conversion, and duplicate prevention. Run the gate and expect failure.

- [ ] **Step 2: Implement the dual-slot monitor**

Build `motionDeviceRows` from the full device model by filtering `deviceMotionCapable`. Persist selected device names with `Settings` keys `primaryMotionDevice` and `secondaryMotionDevice`. Resolve names back to source rows on startup and model reset. Each card reads its source row and displays online/heartbeat, X, Y, speed (`m/s * 3.6`), yaw normalized to 0–360, latency, packet loss, and signal. If offline, display “—” for motion values. When one selector chooses the other card’s device, swap selections.

- [ ] **Step 3: Run QML lint, status gate, and build**

```powershell
cmake --build build --target all_qmllint ADASMaster
powershell -NoProfile -ExecutionPolicy Bypass -File tests/CheckStatusMonitor.ps1
```

Expected: all checks pass.

## Task 9: Full verification and desktop interaction QA

**Files:**
- Modify only files needed to correct failures found by verification.

- [ ] **Step 1: Run the complete automated suite**

```powershell
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: build exit code 0 and all tests pass.

- [ ] **Step 2: Run startup and QML gates explicitly**

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tests/CheckStartup.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tests/CheckQmlLint.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tests/CheckStatusMonitor.ps1
```

Expected: each script prints `PASS`.

- [ ] **Step 3: Verify the target interactions in the desktop app**

Launch `build/ADASMaster.exe`, log in, and verify:

1. 工况管理顶部“＋” can create a group, subgroup, and scenario.
2. Right-click at each tree level offers only valid creation actions and pre-fills parents.
3. A straight `(0,0,0 km/h)` to `(100,0,10 km/h)` shows 100 m, 0°, computed acceleration, and 72 s.
4. Adding a left circular arc with valid radius renders continuously; invalid radius prevents save.
5. Whole-track rotation changes the read-only map while preserving segment lengths.
6. Bottom cards select two different motion devices and show X, Y, speed, yaw, and communication values.
7. Restart preserves catalog nodes, trajectories, and device choices.

- [ ] **Step 4: Inspect runtime warnings**

Confirm no QML binding loop, undefined property, layout, or file-write warnings occur during the target interactions.

## Repository note

This workspace currently has no `.git` directory. The normal per-task commit checkpoints cannot be executed. If Git metadata is restored before implementation, commit each task independently using `feat(scenario): ...`, `feat(trajectory): ...`, and `feat(device): ...` messages.

