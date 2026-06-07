# SMAAT

`SMAAT` stands for `Smart Modular Automation Terminal with Intelligence Layer`.

It is a substantial terminal-based C++ project designed as an application-driven personal operating environment instead of a raw command shell.

It includes:

- Login and registration with salted password hashing
- Branded launcher interface with named applications
- Mission Control dashboard
- Notes Suite for tasks, vault notes, and habits
- Alarm Center
- Calculator Lab
- Music Player
- Video Player
- File Manager
- System Center for time, calendar, and system details
- Per-user persistent storage in local files

## Build

### Windows quick build

```powershell
.\build.bat
```

### Manual g++ build

```powershell
g++ -std=c++20 -Wall -Wextra -pedantic -Iinclude src\main.cpp src\Utils.cpp src\OS.cpp -o build\smaat.exe -lpsapi
```

## Run

```powershell
.\build\smaat.exe
```

## Notes

- User data is stored in the local `data/` folder.
- Music and video entries should point to real local media files on your machine.
- Alarm times use `YYYY-MM-DD HH:MM` and fire while SMAAT is open and signed in.
- Vault note entry ends when you type a single `.` on a new line.
