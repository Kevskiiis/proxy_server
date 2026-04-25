# CS Guardian – Proxy Server

CS Guardian is a locally-run HTTP/HTTPS proxy server written in C, with a React frontend for managing a site blocklist. Blocked sites return a `403 Forbidden` response to the browser. The blocklist is stored in a SQLite database shared between the proxy and the management API.

The primary goal of this project is to explore low-level networking concepts and implement a functional proxy using the C programming language.

---

## Table of Contents

- [Core Concepts](#core-concepts)
- [Design Decisions](#design-decisions)
- [Challenges and Lessons Learned](#challenges-and-lessons-learned)
- [Prerequisites](#prerequisites)
- [Project Setup](#project-setup)
- [Running the Project](#running-the-project)
- [Testing the Proxy](#testing-the-proxy)
- [Using the Proxy with a Browser](#using-the-proxy-with-a-browser-wsl-users)
- [Notes & Limitations](#notes--limitations)

---

## Core Concepts

### Client-Server Architecture
The proxy operates as an intermediary between the client and external servers. All browser requests are routed through the proxy before reaching their destination. If a site is on the blocklist, the proxy intercepts the request and returns a `403 Forbidden` response instead of forwarding it.

### Networking Fundamentals
TCP connections are managed manually using POSIX sockets. This includes creating, binding, listening, accepting, and closing connections — giving full visibility into how data moves across a network.

### Abstraction vs. Control
C provides control over system resources such as memory, buffers, and file descriptors. This comes at the cost of reduced abstraction — things that are handled automatically in higher-level languages must be managed explicitly here.

### Modularity
Responsibilities are split across separate files: connection handling, request forwarding, blocklist management, host file syncing, and database access. This keeps each component focused and easier to maintain.

---

## Design Decisions

### Why C?
This project was built in C as a learning exercise in low-level systems and networking. It forces explicit handling of sockets, memory, and process management that higher-level languages abstract away.

### Why SQLite?
SQLite gives the project a single source of truth for the blocklist. Both the proxy and the API server read from the same database file, so adding or removing a site via the frontend takes effect immediately without restarting the proxy.

### Trade-offs

| | Detail |
|---|---|
| Full control | Direct socket programming gives complete control over networking behavior |
| Real-time updates | SQLite shared between proxy and API means no restart needed |
| ⚠️ IPv4 only | Hostnames are resolved to IPv4 addresses only |
| ⚠️ HTTPS limitation | HTTPS traffic is tunneled but not inspected — the proxy cannot decrypt it without certificates |
| ⚠️ Local only | Designed to run on a single machine's network |

### Comparison to Other Languages

| Language | HTTP Handling |
|---|---|
| C | Manual socket programming, full control, high complexity |
| Python | High-level libraries like `requests` handle most of the work |
| Node.js | Built-in HTTP server capabilities with minimal setup |

---

## Challenges and Lessons Learned

### Socket Programming
Managing sockets manually required learning how to correctly open and close connections, handle buffer sizes, and deal with blocking `recv()` calls that can hang indefinitely without timeouts.

### HTTP vs HTTPS
HTTP requests are forwarded directly. HTTPS uses a `CONNECT` tunnel — the proxy connects to the remote server and blindly forwards encrypted bytes in both directions without being able to read them. This means HTTPS sites can be blocked by hostname but not inspected for content.

### Process Management
Each incoming connection is handled by a forked child process. This required careful management of file descriptors — the parent and child both hold copies of the client socket, and both must close their copy correctly to avoid resource leaks.

### Working in C
Careful handling of memory, string buffers, and data sizes was required throughout. Common issues included buffer overflows, null terminator handling, and ensuring sockets were always closed on every code path.

---

## Prerequisites

Before setting up the project, make sure the following are installed.

### 1. Linux Environment
The C components use POSIX APIs that are only available on Linux. If you are on Windows, install WSL first:

- [Install WSL](https://learn.microsoft.com/en-us/windows/wsl/install)
- Once installed, open a WSL terminal and follow all remaining steps inside it.

### 2. GCC (C Compiler)
Check if GCC is installed:
```bash
gcc --version
```

If not installed:
```bash
sudo apt update
sudo apt install build-essential
```

### 3. Make
Check if Make is installed:
```bash
make --version
```

If not installed:
```bash
sudo apt install make
```

### 4. SQLite Development Library
```bash
sudo apt install sqlite3 libsqlite3-dev
```

### 5. Node.js and NPM
The frontend requires Node.js. Download and install using:
```bash
# Step 1:
sudo apt install curl

# Step 2:
curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.39.7/install.sh | bash

# Step 3:
source ~/.bashrc

# Step 4:
nvm install 22.12
```

Verify the installation:
```bash
node --version
npm --version
```

---

## Project Setup

### Step 1 — Clone the Repository
```bash
git clone https://github.com/Kevskiiis/proxy_server.git
cd proxy_server
```

### Step 2 — Install Frontend Dependencies
From the root project directory: /proxy_server/
```bash
npm install
```

### Step 3 — Build the C Binaries
Navigate to the server directory and compile: /proxy_server/server/
```bash
cd server
make
```

This produces two executables:
- `proxy` — the HTTP/HTTPS proxy server (port `3128`)
- `server` — the REST API server for managing the blocklist (port `8080`)

---

## Running the Project

All three components must be running simultaneously, each in its own terminal.

### Terminal 1 — API Server
```bash
cd proxy_server/server/
./server
```

On first run, the API server will:
1. Create the SQLite database at `database/managed_sites.db`
2. Create the `sites` table
3. Seed the following pre-blocked sites:
   - `login.wsu.edu`
   - `my.wsu.edu`
   - `canvas.wsu.edu`

### Terminal 2 — Proxy Server
```bash
cd proxy_server/server/
./proxy
```

The proxy will start listening on port `3128`.

### Terminal 3 — Frontend
```bash
cd proxy_server/src/
npm run dev
```

Vite will start and output something like:
```
  VITE v8.0.8  ready in 261 ms

  ➜  Local:   http://localhost:5173/
```

Open `http://localhost:5173/` in your browser to access the dashboard. From here you can view, add, and remove blocked sites. Changes take effect immediately — the proxy reads from the same database in real time.

---

## Testing the Proxy

With all three components running, open a fourth terminal and use `curl` to test the proxy.

### Test a Non-Blocked Site
```bash
curl -x http://localhost:3128 http://example.com
```
Expected: Full HTML content of the page is returned.

### Test the Pre-Blocked Sites
```bash
curl -x http://localhost:3128 http://login.wsu.edu
curl -x http://localhost:3128 http://my.wsu.edu
curl -x http://localhost:3128 http://canvas.wsu.edu
```

Expected output:
```html
<html><body><h1>403 Forbidden</h1>
<p>Access to <b>canvas.wsu.edu</b> is blocked by the proxy.</p>
</body></html>
```

### Test HTTPS Tunneling
```bash
curl -x http://localhost:3128 https://example.com
```
Expected: Page content is returned, confirming HTTPS tunneling works.

---

## Using the Proxy with a Browser (WSL Users)

If you are running on WSL and want to route real browser traffic through the proxy, follow these steps.

### Step 1 — Find Your WSL IP Address
Run this inside your WSL terminal:
```bash
ip addr show eth0 | grep "inet "
```

Example output:
```
inet 172.22.144.50/20 brd 172.22.159.255 scope global eth0
```

Take the IP address before the `/20` — in this example it is `172.22.144.50`.

### Step 2 — Configure Windows Proxy Settings
1. Open **Settings → Network & Internet → Proxy**
2. Under **Manual proxy setup**, click **Set up**
3. Toggle **Use a proxy server** to **On**
4. Set **Proxy IP address** to your WSL IP (e.g. `172.22.144.50`)
5. Set **Port** to `3128`
6. Click **Save**

### Step 3 — Restart Your Browser
Close and reopen your browser. All traffic will now route through the proxy.

- Allowed sites load normally
- Blocked sites (e.g. `login.wsu.edu`) return a `403 Forbidden` page

> **Note:** The WSL IP address changes every time WSL restarts. You will need to repeat Steps 1 and 2 after each restart.

> **Minimum requirement:** Only the `proxy` binary needs to be running for browser traffic to be filtered. The `server` and frontend are only needed if you want to manage the blocklist via the dashboard.

---

## Notes & Limitations

- **IPv4 only** — hostnames are resolved to IPv4 addresses only. IPv6-only networks are not supported.
- **HTTPS is tunneled, not inspected** — the proxy can block HTTPS sites by hostname but cannot read or modify the encrypted content.
- **Local only** — this project is designed to run on a single local machine. It is not intended for deployment as a shared network proxy.
- **WSL IP changes on restart** — if using WSL, your IP address is reassigned each time WSL starts. Update your Windows proxy settings accordingly.
- **No authentication** — the proxy has no password protection. Anyone on the same network who knows your WSL IP and port can route traffic through it.