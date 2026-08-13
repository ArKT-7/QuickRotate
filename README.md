# <sub>[<img src="./assets/icon.png" width="40" height="40">](https://github.com/ArKT-7/QuickRotate/releases/latest)</sub> Quick Rotate
### A modern, ultra-lightweight screen rotation utility for Windows.

---

## 🌟 Overview

**Quick Rotate** is a standalone desktop utility built for fast, reliable display orientation management. Written entirely in pure C++ using the native Win32 API and GDI+, it delivers a custom Windows 11-style graphical interface without the bloated overhead of heavy runtimes or bulky frameworks.

**Thanks to this native approach (fr),** it compiles into a single, statically-linked executable (only ~60KB in size), ensuring universal compatibility across Windows 32-bit, 64-bit, and Windows on ARM (WoA) via native OS emulation.⚡

---

## 📥 Installation
**`Choose your preferred installation method:`**

* **Microsoft Store (Recommended)**

<a href="https://aka.ms/AA132927"><img alt="Get it from Microsoft" width="220px" src="https://get.microsoft.com/images/en-us%20dark.svg" /></a>

* **WinGet CLI**
```powershell
winget install QuickRotate    # Install
winget upgrade QuickRotate    # Update
```

* **Direct Download**

<a href="https://github.com/ArKT-7/QuickRotate/releases/latest"><img alt="Direct Download .exe" width="200px" src="./assets/github-button.svg" /></a>

---

## ✨ Features

* **The "Hybrid" Portable App:** It’s a single `.exe` file that acts as both a portable utility and its own installer. Run it from anywhere, and it seamlessly sets up its own AppData file caching and shortcuts in the background. Zero heavy runtimes or background services needed!
* **Modern UI & Themes:** Custom built GDI+ interface with smooth icons, rounded corners, and the option to choose between Light, Dark, or System Default themes.
* **Tray Integration:** Runs silently in the background with a customizable tray icon click action [See here...](https://github.com/ArKT-7/QuickRotate/blob/main/README.md#3-%EF%B8%8F-system-tray--context-menu)
* **Self-Updating:** Includes a built-in updater that grabs and applies new versions directly from GitHub releases.
* **CLI Support:** Automate your rotations using command-line arguments for scripts and batch files.

---

## 📝 Usage Guide

### 1. 🖥️ Main Interface & Orientations
 **`The main app panel gives you five quick actions for your display:`**
* **Landscape:** Resets the display to the default **`horizontal view`**
* **Portrait:** Sets the display to **`vertical (90°)`**
* **Rotate Clockwise:** Cycles to the next available orientation **`(↻)`**
* **Flipped Landscape:** Rotates the display **`180° (Upside Down)`**
* **Flipped Portrait:** Sets the display to reverse **`vertical (270°)`**

### 2. ⚙️ Settings & Automation
 **`Click Settings ⚙️ to configure how the tool behaves:`**
* **Minimize to Tray on Close:** Hides the app in the system tray instead of closing it completely.
* **Start with Windows:** Quietly sets itself to launch automatically so it's always ready when you boot up.
* **`Theme Control` (3-Way Toggle):** Choose exactly how the app looks (Light mode, Dark mode, or System Sync).
* **`Tray Click Mode` (3-Way Toggle):** Configure the **Single-Click** behavior of the tray icon to cycle between:
    * **Landscape ↔ Portrait**
    * **(F) Landscape ↔ Portrait** (Flipped orientations)
    * **Cycle Rotation** (Next ↻)
* **Desktop Shortcuts:** Instantly generate orientation links on your Desktop for one-click access.

### 3. 🖱️ System Tray & Context Menu
 **`The tray icon provides instant access without opening the main window:`**

* **Left-Click:** Triggers your chosen **`Tray Click Mode`**.
* **Right-Click:** Opens a custom menu for direct control:
    * **Quick Access:** Select a specific rotation directly from the list.
    * **Restore Window:** Brings the main control panel back to the center of your screen.
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

Even though you only download and run a single `.exe` file, Quick Rotate automatically set up a proper Windows uninstaller for you. This means you will never have to manually delete leftover files to get rid of it.

It appears in **Settings → Apps → Installed apps** (or Control Panel's **Programs and Features**) just like any standard application.

To completely remove it:

1. Open **Settings → Apps → Installed apps**.
2. Find **Quick Rotate** and hit **Uninstall**.

*This will completely remove the autostart entry, all Start Menu/Desktop shortcuts, your settings file, and the program files. Zero footprints left behind, yeah!*

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
