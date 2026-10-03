# SuperC - The Instant Windows Run Power Utility

A blazing-fast (< 2 ms startup), zero-dependency, native C++ command runner, inline calculator, and developer toolkit designed for the **Windows Run (`Win + R`)** dialog.

---

## ⚡ Why `C.exe`?

When you press `Win + R`:
1. **Running a command directly** (e.g. `ipconfig`) flashes a black terminal window and immediately closes before you can read the results.
2. **Typing `cmd /k ipconfig`** keeps it open, but typing `cmd /k ` every time is slow and annoying.
3. **Calculating `5+7`** normally requires opening Calculator.
4. **Checking Wi-Fi passwords, free ports, or generating passwords** takes multiple menus and clicks.

**`C.exe` fixes all of this.** It is compiled as a native **Windows Subsystem** application (`/SUBSYSTEM:WINDOWS`), which means:
* **Math & Quick Dialogs:** Open instantly with **zero console flashing**.
* **Commands:** Launch directly in an interactive terminal and stay open.
* **Auto-Copy:** Results are automatically copied to your Windows Clipboard!

---

## 🚀 Quick Install (No Admin Needed)

1. Double-click `register.bat` (or open a terminal in this folder and type `.\c.exe install`).
2. That's it! Press **`Win + R`** and type `c 5+7` or `c ipconfig`.

*(To remove at any time, run `unregister.bat` or type `c uninstall`)*.

---

## 📖 Command Reference

### 1. Instant Math & Calculations
Pops up a clean dialog displaying the result and **automatically copies it to your clipboard**:
* `c 5+7` $\rightarrow$ Result: `12`
* `c (100 - 35.5) * 2` $\rightarrow$ Parentheses, decimals, order of operations
* `c 2^16` $\rightarrow$ Power: `65536`
* `c 100 * 15%` $\rightarrow$ Percentage calculations: `15`
* `c sqrt(144)` $\rightarrow$ Functions: `sqrt`, `abs`, `round`, `floor`, `ceil`, `sin`, `cos`, `tan`, `log`, `ln`
* `c 0xFF + 1` $\rightarrow$ Hex arithmetic

### 2. General Shell Commands
Runs your command and **keeps the terminal open** (`cmd /k`) so you can view the output:
* `c ipconfig`
* `c ping 1.1.1.1`
* `c git status`
* `c npm run dev`
* `c` *(no arguments)* $\rightarrow$ Opens a fresh Command Prompt

### 3. Administrator / Elevation (`#` or `sudo`)
Triggers the Windows UAC elevation prompt and runs elevated in an Administrator terminal:
* `c # sfc /scannow`
* `c # ipconfig /renew`
* `c sudo net start wuauserv`

### 4. Shell Switchers
* `c ps Get-Process` $\rightarrow$ Runs inside **PowerShell** (`powershell.exe -NoExit`)
* `c wt ping 8.8.8.8` $\rightarrow$ Runs inside **Windows Terminal** (`wt.exe`)
* `c -p ping 8.8.8.8` $\rightarrow$ Runs, prints output, and pauses (*"Press any key to close"*), then exits

### 5. Developer & Port Tools
* `c port 3000` or `c port 8080`  
  Detects which process/PID is listening on that port (e.g. `node.exe (PID: 14208)`) and includes a **"Kill Process"** button right in the dialog!
* `c kill node` or `c kill python`  
  Force terminates hung background processes (`taskkill /F /IM node.exe`).
* `c pass 16` (or any length)  
  Generates a cryptographically strong random password, copies it to clipboard, and displays it in the dialog.

### 6. Networking
* `c wifi` or `c wifi pass`  
  Detects your connected Wi-Fi SSID and **reveals the saved password** in a popup dialog (auto-copied to clipboard).
* `c ip` or `c myip`  
  Lists all active local IP addresses (Wi-Fi, Ethernet, WSL) and copies primary LAN IP.
* `c p 8.8.8.8`  
  Shorthand for ping.

### 7. Converters
* `c hex 255` $\rightarrow$ Displays Hex (`0xFF`), Decimal (`255`), and Binary (`0b11111111`).
* `c ts 1727940000` $\rightarrow$ Converts Unix timestamp to local date and time.
* `c ts` $\rightarrow$ Shows current Unix timestamp.

### 8. Clipboard Tools
* `c lower` $\rightarrow$ Converts whatever is currently on your clipboard to lowercase.
* `c upper` $\rightarrow$ Converts clipboard to uppercase.
* `c trim` $\rightarrow$ Strips leading/trailing whitespace & newlines from clipboard.
* `c count` $\rightarrow$ Shows character count, word count, and line count of clipboard text.

### 9. Quick Web & URLs
* `c g error 0x80070005` $\rightarrow$ Searches Google in default browser.
* `c yt lofi beats` $\rightarrow$ Searches YouTube.
* `c gh microsoft/terminal` $\rightarrow$ Opens GitHub repository.
* `c localhost:3000` or `c reddit.com` $\rightarrow$ Opens URL directly in browser.

### 10. System Folders
* `c dl` $\rightarrow$ Downloads
* `c dt` $\rightarrow$ Desktop
* `c doc` $\rightarrow$ Documents
* `c temp` $\rightarrow$ %TEMP% folder
* `c appdata` $\rightarrow$ %APPDATA% folder
* `c hosts` $\rightarrow$ Opens `hosts` file in Notepad as Administrator
* `c env` $\rightarrow$ Opens Windows System Environment Variables dialog

---

## 🛠️ Building From Source

Prerequisites: Visual Studio 2022 (Community or Build Tools with C++ workload).

Simply run:
```cmd
build.bat
```
Output executable is generated at `bin\c.exe` and copied to `.\c.exe`.
