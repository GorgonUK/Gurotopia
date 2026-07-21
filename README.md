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

---

# VPS Server Configuration

The recommended VPS deployment uses Docker Compose. It builds Gurotopia in an
Ubuntu container, runs MariaDB in a separate container, and stores the database
in a persistent Docker volume.

> [!IMPORTANT]
> Use an Ubuntu 22.04/24.04 VPS with at least **2 GB RAM**, a public IPv4
> address, and root or sudo access. The examples below install the project in
> `/opt/gurotopia`.

## 1. Point the client at the VPS

On every computer that will connect, edit the system **hosts** file:

- **Windows**: `C:\Windows\System32\drivers\etc\hosts`
- **Linux/macOS**: `/etc/hosts`

Replace `<VPS_IP>` with the public IPv4 address of the VPS:

```
<VPS_IP> www.growtopia1.com
<VPS_IP> www.growtopia2.com
```

## 2. Install Docker and Git

Connect to the VPS:

```bash
ssh root@<VPS_IP>
```

Install Docker from Ubuntu's package repository:

```bash
apt-get update
apt-get install -y docker.io docker-compose-v2 git
systemctl enable --now docker
```

Confirm both commands work:

```bash
docker --version
docker compose version
```

## 3. Clone Gurotopia

```bash
git clone https://github.com/GorgonUK/Gurotopia.git /opt/gurotopia
cd /opt/gurotopia
```

## 4. Create the runtime configuration

Copy the provided templates:

```bash
cp deploy/database.cfg.example deploy/database.cfg
cp deploy/server.cfg.example deploy/server.cfg
cp deploy/server_data.php.example deploy/server_data.php
```

Copy a compatible `items.dat` from your Growtopia installation/cache:

```bash
cp /path/to/items.dat deploy/items.dat
```

Your `deploy/` directory should now contain:

```
deploy/
├── database.cfg
├── server.cfg
├── server_data.php
└── items.dat
```

### Database configuration

Edit `deploy/database.cfg`:

```text
host|db
user|gurotopia
password|<STRONG_DATABASE_PASSWORD>
```

`host|db` is required because `db` is the MariaDB service name inside Docker
Compose. The password must match `MYSQL_PASSWORD` in the next step.

### Game configuration

Edit `deploy/server.cfg`:

```text
growth_speed|10
gem_drop_multiplier|10
```

- `growth_speed|10` makes trees/providers mature 10× faster (`1` is normal).
- `gem_drop_multiplier|10` multiplies gems dropped by eligible blocks.

### Server-data configuration

Edit `deploy/server_data.php` and replace the server address:

```text
server|<VPS_IP>
port|17091
type|1
type2|1
#maint|Server under maintenance. Please try again later.
loginurl|gtlogin-six.vercel.app
meta|gurotopia
RTENDMARKERBS1001
```

Use your own GTLogin deployment for `loginurl` if applicable. Do not include
`https://` in this value.

## 5. Set database secrets

Create `/opt/gurotopia/.env`:

```bash
cat > .env <<'EOF'
MYSQL_ROOT_PASSWORD=replace_with_a_long_random_root_password
MYSQL_PASSWORD=replace_with_the_same_password_used_in_database_cfg
EOF

chmod 600 .env
```

Generate strong values rather than using the Compose defaults:

```bash
openssl rand -hex 32
```

> [!WARNING]
> Never commit `.env`, `deploy/database.cfg`, or other files containing
> production credentials.

## 6. Configure the firewall

The game needs TCP `443` and UDP `17091`:

```bash
ufw allow OpenSSH
ufw allow 443/tcp
ufw allow 17091/udp
ufw enable
ufw status
```

MariaDB is published on TCP `3306` so a separately hosted GTLogin instance can
validate users. Do **not** expose it broadly with a weak password.

- If GTLogin runs on the same VPS, bind port `3306` to `127.0.0.1` in
  `docker-compose.yml`.
- If GTLogin runs elsewhere, allow `3306/tcp` only from its trusted egress IP,
  or use a VPN/private network/managed database.
- Vercel does not provide a stable egress IP on every plan; consider a secure
  database proxy or managed MariaDB instead of opening `3306` to the internet.

Example for one trusted address:

```bash
ufw allow from <TRUSTED_LOGIN_SERVER_IP> to any port 3306 proto tcp
```

## 7. Build and start the server

```bash
cd /opt/gurotopia
docker compose up -d --build
```

The first build can take several minutes. Check the container status:

```bash
docker compose ps
```

Expected services:

- `gurotopia-server-1` — Gurotopia server
- `gurotopia-db-1` — MariaDB (healthy)

Follow the server logs:

```bash
docker compose logs -f server
```

A healthy startup ends with messages similar to:

```text
connected to MariaDB server on db:3306
listening on <VPS_IP>:17091
items.dat parsed successfully!
```

## 8. Update an existing deployment

```bash
cd /opt/gurotopia
git pull --ff-only
docker compose up -d --build
docker compose ps
```

`deploy/` runtime files and the MariaDB volume survive container rebuilds.

## 9. Common operations

```bash
# Restart only the game server
docker compose restart server

# View recent logs
docker compose logs --tail=100 server

# Stop the stack without deleting the database
docker compose down

# Start the existing stack
docker compose up -d
```

> [!CAUTION]
> Do not run `docker compose down -v` unless you intentionally want to delete
> the MariaDB volume and all player/world data.

## 10. Back up MariaDB

Create a SQL backup:

```bash
cd /opt/gurotopia
set -a; source .env; set +a
docker compose exec -T db \
  mariadb-dump -ugurotopia -p"$MYSQL_PASSWORD" gurotopia \
  > "gurotopia-$(date +%F-%H%M).sql"
```

Restore a backup into a running database container:

```bash
set -a; source .env; set +a
docker compose exec -T db \
  mariadb -ugurotopia -p"$MYSQL_PASSWORD" gurotopia \
  < gurotopia-YYYY-MM-DD-HHMM.sql
```

## Network reference

| Port | Protocol | Purpose |
|------|----------|---------|
| `443` | TCP | HTTPS bootstrap / `server_data.php` |
| `17091` | UDP | ENet game traffic |
| `3306` | TCP | MariaDB for GTLogin; restrict access |

MariaDB data is persisted in the Docker volume `gurotopia-db`.
