# SuperC - The Ultra-Fast Windows Power Utility & Quick Launcher

A blazing-fast (< 2 ms startup, ~4 MB RAM), zero-dependency, native C++ command runner, inline calculator, app launcher, and developer toolkit for Windows.

SuperC comes with **two unified experiences** powered by the same shared C++ engine:

1. **`SuperC-Launcher.exe` (Floating Quick Launcher)**  
   Press **`Ctrl + Space`** anywhere to summon a modern floating bar with real-time live preview as you type.
2. **`c.exe` (Win + R / Command-Line Tool)**  
   Press **`Win + R`** and type `c <anything>` to run commands with interactive terminals, instant calculations, or developer quickies.

---

## ⚡ Why SuperC?

| Feature | SuperC | PowerToys Run / Wox / Flow |
| :--- | :--- | :--- |
| **Language & Runtime** | **Pure Native C++ / Win32** | C# / .NET / WPF / WinUI |
| **Idle Memory Usage** | **~4 MB RAM** | **150 MB – 300 MB RAM** |
| **Hotkey Latency** | **< 2 ms** (instant) | 50 – 150 ms (garbage collection lag) |
| **External Dependencies** | **Zero** | .NET Runtime, Desktop SDKs |
| **Dual Interface** | **Floating Bar (`Ctrl+Space`) + Win+R (`c ...`)** | Floating bar only |

---

## 🚀 Quick Start

### 1. The Quick Launcher (`Ctrl + Space`)
Run `SuperC-Launcher.exe`. It quietly docks into your system tray (near the clock).
* Press **`Ctrl + Space`** anywhere on your desktop or inside any app.
* A sleek, floating launcher appears in the center of your screen.
* Results preview in real-time as you type!
* **`Enter`**: Executes action or copies result to clipboard.
* **`Ctrl + Enter`**: Runs command as **Administrator**.
* **`Esc`**: Dismisses the bar immediately.

### 2. Windows Run Integration (`Win + R`)
Double-click `register.bat` to register `c` into your Windows Run dialog.
* Press **`Win + R`**, type `c 5+7` or `c ipconfig`.
* Math expressions pop up with zero console flashing and auto-copy to clipboard.
* Commands keep the terminal open (`cmd /k`) so you can read the output.

---

## 📖 Feature Matrix & Command Guide

### 1. Live Math & Inline Calculator
Results appear live under the search bar as you type:
* `5+7` $\rightarrow$ `= 12`
* `(100 - 35.5) * 2` $\rightarrow$ Parentheses, decimals, order of operations
* `2^16` $\rightarrow$ Power: `65536`
* `100 * 15%` $\rightarrow$ Percentage calculations: `15`
* `sqrt(144)` $\rightarrow$ Functions: `sqrt`, `cbrt`, `abs`, `round`, `floor`, `ceil`, `sin`, `cos`, `tan`, `log`, `ln`
* `0xFF + 1` $\rightarrow$ Hex arithmetic

### 2. Offline Unit Conversions
Convert measurements instantly without needing the internet or opening a browser:
* `100c in f` $\rightarrow$ `212 f` (Temperature)
* `10km in miles` $\rightarrow$ `6.21371 mi` (Length)
* `1024mb in gb` $\rightarrow$ `1 gb` (Digital Storage)
* `150 lbs to kg` $\rightarrow$ `68.0389 kg` (Weight)
* `24h in days` $\rightarrow$ `1 d` (Time)

### 3. Installed Apps & Window Walker (Task Switcher)
Fast, zero-allocation fuzzy matching across your system:
* **Launch Applications:** Type `notep` (Notepad), `spot` (Spotify), `vsc` (Visual Studio Code), `calc` (Calculator).
* **Switch Open Windows:** Type the title of any open program to bring it directly to the foreground.

### 4. System Power & Control
Execute core Windows system actions right from the keyboard:
* `lock` $\rightarrow$ Locks workstation
* `sleep` $\rightarrow$ Puts PC to sleep
* `restart` / `reboot` $\rightarrow$ Restarts computer
* `shutdown` $\rightarrow$ Shuts down computer
* `empty trash` or `trash` $\rightarrow$ Empties Recycle Bin silently

### 5. Developer & Network Toolkit
* `port 3000` or `port 8080` $\rightarrow$ Detects process name & PID listening on that port, with one-click **Kill Process**.
* `wifi` or `wifi pass` $\rightarrow$ Reveals connected Wi-Fi network and saved security password.
* `pass 16` $\rightarrow$ Generates a cryptographically strong 16-character random password.
* `hex 255` $\rightarrow$ Displays Hex (`0xFF`), Decimal (`255`), and Binary (`0b11111111`).
* `ts 1727940000` $\rightarrow$ Converts Unix epoch timestamp to local date and time.
* `kill node` $\rightarrow$ Force-terminates hung background processes.
* `ip` / `myip` $\rightarrow$ Lists active local IP addresses.

### 6. Quick Web Searches & Direct URLs
* `g <query>` $\rightarrow$ Google search in default browser
* `yt <query>` $\rightarrow$ YouTube search
* `gh <repo>` $\rightarrow$ GitHub repository or search
* `localhost:3000` or `reddit.com` $\rightarrow$ Opens URL directly in browser

---

## 📁 Repository Architecture

```
Super C/
├── bin/                       # Compiled 64-bit binaries
│   ├── c.exe                  # Win+R / CLI executable
│   └── SuperC-Launcher.exe    # Floating Quick Launcher
├── src/
│   ├── common/                # Shared C++ core engine
│   │   ├── math_eval.h/.cpp   # Recursive descent math parser
│   │   ├── unit_conv.h/.cpp   # Offline unit conversion engine
│   │   ├── app_index.h/.cpp   # Start Menu .lnk scanner & fuzzy matcher
│   │   ├── window_walker.h/.cpp # Open window switcher (EnumWindows)
│   │   ├── sys_control.h/.cpp # System power commands (lock, sleep, trash)
│   │   └── handlers.h/.cpp    # Network, port inspector, password gen
│   ├── cli/                   # Win+R / CLI component
│   │   ├── main_cli.cpp       # CLI entry point & command router
│   │   └── ui.h/.cpp          # Win32 result dialogs & clipboard auto-copy
│   └── launcher/              # Floating Quick Launcher component
│       ├── main_launcher.cpp  # Tray daemon & Ctrl+Space global hotkey
│       └── launcher_ui.h/.cpp # Frameless DWM rounded window & live preview
├── tests/
│   └── test_math.cpp          # Automated test suite
├── build.bat                  # One-click MSVC compile script
├── register.bat               # Registers c.exe in Windows Run (App Paths)
├── unregister.bat             # Unregisters c.exe
├── LICENSE                    # MIT License
└── README.md                  # Documentation
```

---

## 🛠️ Building From Source

### Prerequisites
* Windows 10 or 11 (64-bit)
* Visual Studio 2022 (Community or Build Tools with C++ workload)

### Build Command
Simply run:
```cmd
build.bat
```
Both `c.exe` and `SuperC-Launcher.exe` will be compiled and copied to the root folder ready for use.

---

## 📄 License

This project is licensed under the [MIT License](LICENSE).
