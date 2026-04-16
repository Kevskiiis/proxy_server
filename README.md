# CS Guardian

CS Guardian is a local website-blocking dashboard. The React UI manages a blocklist, and the backend enforces that list by writing a managed section to a hosts file.

## Runtime Modes

- `npm run dev`
  Starts the Vite frontend and the backend launcher. The launcher prefers the compiled C backend when it exists, and falls back to the JavaScript backend when it does not.
- `npm run dev:c`
  Requires a compiled C backend and fails fast if it is missing.
- `npm run dev:server:js`
  Forces the JavaScript backend.

## C Backend

The C backend is part of the runtime path instead of being a disconnected prototype. The launcher in `server/index.js` can spawn it and proxy the React app's API traffic to it on the usual `/api/*` routes.

Expected compiled binary locations:

- `server/bin/cs_guardian_c_api.exe`
- `server/bin/cs_guardian_c_api`

The included `server/Makefile` builds:

- `server/bin/cs_guardian_c_api`
- `server/bin/cs_guardian_c_proxy`

Example build on Linux or WSL:

```bash
cd server
make
```

## Useful Environment Variables

- `PORT`
  Public API port used by the React app. Default: `3001`
- `C_BACKEND_PORT`
  Internal port used by the spawned C backend. Default: `3101`
- `C_BACKEND_BIN`
  Explicit path to the compiled C backend executable
- `HOSTS_FILE_PATH`
  Hosts file to sync. For safe local testing you can point this at `server/data/hosts_test.txt`

## Notes

- The C backend now returns the same API shape the React app expects, including site objects and health details.
- The standalone forwarding proxy still exists in `server/proxy.c`, but it is separate from the blocklist API.
