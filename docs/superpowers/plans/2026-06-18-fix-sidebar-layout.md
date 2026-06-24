# Sidebar Layout Fix Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Keep the 176 px navigation sidebar at full content height so its menu and emergency button remain inside the sidebar at every window size.

**Architecture:** Preserve the existing `RowLayout` structure. Express sidebar geometry through `Layout.*` attached properties because the parent layout owns child geometry, and add a source-level regression check for the required constraints.

**Tech Stack:** Qt 6 QML, PowerShell regression check, CMake/Ninja

---

### Task 1: Add the failing sidebar-layout regression check

**Files:**
- Create: `tests/CheckSideNavLayout.ps1`
- Test: `tests/CheckSideNavLayout.ps1`

- [ ] Read `qml/Main.qml` and locate the `SideNavBar` instance.
- [ ] Assert that it contains both `Layout.preferredWidth: 176` and `Layout.fillHeight: true`.
- [ ] Run `powershell -NoProfile -ExecutionPolicy Bypass -File tests/CheckSideNavLayout.ps1` and confirm it fails before production changes.

### Task 2: Apply the minimal layout fix

**Files:**
- Modify: `qml/Main.qml:51`

- [ ] Add `Layout.preferredWidth: 176` and `Layout.fillHeight: true` to the `SideNavBar` instance.
- [ ] Remove the unmanaged `width: 176` declaration from `qml/components/SideNavBar.qml` so geometry has one owner.
- [ ] Re-run `tests/CheckSideNavLayout.ps1` and confirm it passes.

### Task 3: Verify the application artifact

**Files:**
- Build output: `build/ADASMaster.exe`

- [ ] Run `cmake --build build` and confirm exit code 0.
- [ ] Confirm `build/ADASMaster.exe` is newer than the modified QML sources.
- [ ] Re-run the regression check after the build.

