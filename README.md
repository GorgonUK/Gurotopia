<div align="center">

**グロートピア** *(Gurotopia)* : Lightweight & Maintained GTPS written in C/C++

[![](https://github.com/GT-api/GT.api/actions/workflows/make.yml/badge.svg)](https://github.com/GT-api/GT.api/actions/workflows/make.yml)
[![Dockerfile](https://github.com/gurotopia/Gurotopia/actions/workflows/docker.yml/badge.svg)](https://github.com/gurotopia/Gurotopia/actions/workflows/docker.yml)
[![](https://app.codacy.com/project/badge/Grade/fa8603d6ec2b4485b8e24817ef23ca21)](https://app.codacy.com/gh/gurotopia/Gurotopia/dashboard?utm_source=gh&utm_medium=referral&utm_content=&utm_campaign=Badge_grade)
[![](https://dcbadge.limes.pink/api/server/zzWHgzaF7J?style=flat)](https://discord.gg/zzWHgzaF7J)

</div>

***

# <img width="190" height="50" src="https://github.com/user-attachments/assets/1385f762-2c56-465a-aa3b-901a431552bb" />

### ![](https://raw.githubusercontent.com/microsoft/vscode-icons/main/icons/dark/archive.svg) 1. Requirements
   - [**MSYS2**](https://www.msys2.org/)
   - [**Visual Studio Code**](https://code.visualstudio.com/): install [C/C++ extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools) for VSCode
   - [**MariaDB Server**](https://mariadb.org/download/?t=mariadb&p=mariadb&r=12.2.2&os=windows&cpu=x86_64&pkg=msi&mirror=accretive)

### 2. Setup MSYS2
   - Locate your MSYS2 folder at `C:\msys64`, open `ucrt64.exe`, and run the following command:
   
     ```bash
     pacman -S --needed mingw-w64-ucrt-x86_64-{gcc,openssl} make
     ```

### ![](https://raw.githubusercontent.com/microsoft/vscode-icons/main/icons/dark/build.svg) 3. Compile
   - Open the project folder in Visual Studio Code.
   - Press **`Ctrl + Shift + B`** to start the build process.

### ![](https://raw.githubusercontent.com/microsoft/vscode-icons/main/icons/dark/debug-alt-small.svg) 4. Run
   - After compiling, press **`F5`** to run the server!

# <img src="https://github.com/user-attachments/assets/fecde323-04c5-4b82-a08d-badcb184be6a" width="30" /> Linux

### ![](https://raw.githubusercontent.com/microsoft/vscode-icons/main/icons/dark/archive.svg) 1. Install Dependencies

- enter command associated with your distribution into the terminal to install nessesary tools.
   <details><summary><img width="22" height="22" src="https://github.com/user-attachments/assets/8359ba6e-a9b2-4500-893f-61eaf40e2478" /> Arch</summary>
   <p>
      
   ```bash
   sudo pacman -S base-devel openssl mariadb-libs
   ```
   </p>
   </details> 
   <details><summary><img width="22" height="22" src="https://github.com/user-attachments/assets/742f35c4-3e69-450e-8095-9fabe9ecd0d8" /> Debian <img width="18" height="18" src="https://github.com/user-attachments/assets/46f0770e-f4ed-480b-851d-c90b05fae52f" /> Ubuntu</summary>
   <p>
      
   ```bash
   sudo apt-get update && sudo apt-get install build-essential libssl-dev libmariadb-dev
   ```
        
   </p>
   </details> 

### ![](https://raw.githubusercontent.com/microsoft/vscode-icons/main/icons/dark/build.svg) 2. Compile
   - Navigate to the project's root directory in your terminal and run the `make` command:
   
     ```bash
     make -j$(nproc)
     ```

### ![](https://raw.githubusercontent.com/microsoft/vscode-icons/main/icons/dark/debug-alt-small.svg) 3. Run
   - Execute the compiled binary located in the `main` directory:
   
     ```bash
     ./main.out
     ```

---

# Local Server Configuration

> [!NOTE]
> To connect to your local server, you must modify your system's **hosts** file.
> - **Windows**: `C:\Windows\System32\drivers\etc\hosts`
> - **Linux/macOS**: `/etc/hosts`
>
> ```
> 127.0.0.1 www.growtopia1.com
> 127.0.0.1 www.growtopia2.com
> ```

### Growth speed

Edit `server.cfg` (created on first run, or copy `server.cfg.example`):

```
growth_speed|10
```

`10` = trees and providers mature **10× faster** than normal. Use `1` for stock timing.

### Docker deploy (portable)

1. Copy runtime files into `deploy/`:
   ```bash
   cp deploy/database.cfg.example deploy/database.cfg
   cp deploy/server.cfg.example deploy/server.cfg
   cp deploy/server_data.php.example deploy/server_data.php
   # copy items.dat into deploy/items.dat
   ```
2. Set `server_data.php` → `server|<machine-ip>` (LAN or public).
3. Run:
   ```bash
   docker compose up -d --build
   ```

Ports: UDP `17091` (game), TCP `443` (`server_data.php`). MariaDB data lives in the `gurotopia-db` volume.
