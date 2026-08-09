# <sub>[<img src="./assets/icon.png" width="40" height="40">](https://github.com/ArKT-7/QuickRotate/releases/latest)</sub> Quick Rotate
### A modern, ultra-lightweight screen rotation utility for Windows.

---

## 🌟 Overview

**Quick Rotate** is a standalone desktop utility built for fast, reliable display orientation management. Written entirely in pure C++ using the native Win32 API and GDI+, it delivers a custom Windows 11-style graphical interface without the overhead of heavy runtimes or bulky external frameworks.

**Thanks to this native approach (fr),** it compiles into a single, statically-linked x86 executable (only ~60KB in size), ensuring universal compatibility across Windows 32-bit, 64-bit, and Windows on ARM (WoA) via native OS emulation.⚡

---
## 📥 [Download Latest Version Here!](https://github.com/ArKT-7/QuickRotate/releases/latest)
---

## ✨ Features

- **Zero Dependencies:** A single `.exe` file. No background services, DLLs, or runtimes required.
- **Modern UI:** Custom built GDI+ interface featuring anti-aliased icons, rounded corners, and dynamic tracking of Windows Immersive Dark/Light modes.
- **Tray Integration:** Runs silently in the background with a customizable tray icon click action [and more...](https://github.com/ArKT-7/QuickRotate/blob/main/README.md#3-%EF%B8%8F-system-tray--context-menu)
* **Self-Updating:** Includes a built-in updater that grabs and applies new versions right from GitHub releases.
- **CLI Support:** Automate rotations using command-line arguments for scripts and batch files.

---

## 📝 Usage Guide

### 1. 🖥️ Main Interface & Orientations
 **`The main app panel offers five actions for your display:`**
* **Landscape:** Resets display to the default **`horizontal view`**
* **Portrait:** Sets display to **`vertical (90°)`**
* **Rotate Clockwise:** Cycles to the next available orientation **`(↻)`**
* **Flipped Landscape:** Rotates display **`180° (Upside Down)`**
* **Flipped Portrait:** Sets display to reverse **`vertical (270°)`**

### 2. ⚙️ Settings & Automation
 **`Click Settings ⚙️ to configure tool behavior and automation:`**
* **Minimize to Tray on Close:** Hides the app in the system tray instead of exiting when you click close.
* **Start with Windows:** Integrates with the registry for automatic startup on boot.
* **`Tray Click Mode` (3-Way Toggle):** Configure the **Single-Click** behavior of the tray icon to cycle between:
    * **Landscape ↔ Portrait**
    * **(F) Landscape ↔ Portrait** (Flipped orientations)
    * **Cycle Rotation** (Next ↻)
* **Desktop Shortcuts:** Click any shortcut toggle to instantly create orientation links on your Desktop for one-click access.

### 3. 🖱️ System Tray & Context Menu
 **`The tray icon provides fast access to rotation without opening the main window:`**

* **Left-Click:** Triggers your chosen **`Tray Click Mode`**.
* **Right-Click:** Opens a custom **Custom built Menu** for direct control:
    * **Quick Access:** Select any specific rotation directly from the list.
    * **Restore Window:** Returns the main control panel to the center of your screen.
    * **Exit:** Fully terminates the application.

---

## ⌨️ Command Line Interface (CLI)
**`Quick Rotate supports CLI automation for scripts, batch files, or custom hotkeys:`**

| Command | Action |
| :--- | :--- |
| `QuickRotate.exe 0` | Sets display to **Landscape**. |
| `QuickRotate.exe 90` | Sets display to **Portrait**. |
| `QuickRotate.exe 180` | Sets display to **Flipped Landscape**. |
| `QuickRotate.exe 270` | Sets display to **Flipped Portrait**. |
| `QuickRotate.exe next` | Rotates to the **Next** orientation in the cycle. |
| `QuickRotate.exe -tray` | Launches the application silently to the system tray. |

---

## 🗑️ Uninstalling

Quick Rotate acts as a portable application, but registers itself in **Settings → Apps → Installed apps** (or Control Panel's **Programs and Features**) under the name **Quick Rotate**, just like a normal installed application. To remove it:

1. Open **Settings → Apps → Installed apps**.
2. Find **Quick Rotate** and select **Uninstall**.

This removes the autostart entry, all Start Menu/Desktop shortcuts, the settings file, and the copied program files.

---

## ❤️ Support My Work

If you find my projects helpful, consider supporting my work! Your contributions help me keep developing and sharing useful resources.

<p align="left">
  <a href="https://www.buymeacoffee.com/ArKT" target="_blank">
    <img src="https://github.com/ArKT-7/Temp-files/blob/main/assets/buymecoffee.png" alt="Buy Me A Coffee" style="height: 60px !important; width: 217px !important;">
  </a>
  <a href="https://www.paypal.me/arkt7" target="_blank">
    <img src="https://github.com/ArKT-7/Temp-files/blob/main/assets/Paypal.png" alt="Donate with PayPal" style="height: 60px !important; width: 217px !important;">
  </a>
</p>

---

### 🎉 Efficiency meets elegance (xD) with Quick Rotate v6.2!
