import http from 'node:http'
import { spawn } from 'node:child_process'
import { access } from 'node:fs/promises'
import path from 'node:path'
import { fileURLToPath } from 'node:url'
import { startJsBackend } from './jsBackend.js'

const __dirname = path.dirname(fileURLToPath(import.meta.url))
const PORT = Number(process.env.PORT || 3001)
const C_BACKEND_PORT = Number(process.env.C_BACKEND_PORT || 3101)
const argv = new Map(
  process.argv.slice(2)
    .filter((value) => value.startsWith('--'))
    .map((value) => {
      const [key, rawValue = 'true'] = value.slice(2).split('=')
      return [key, rawValue]
    }),
)
const backendMode = argv.get('backend') || process.env.BACKEND_IMPL || 'auto'

function canHaveBody(method) {
  return !['GET', 'HEAD'].includes(method || 'GET')
}

async function fileExists(target) {
  try {
    await access(target)
    return true
  } catch {
    return false
  }
}

async function findCExecutable() {
  const candidates = [
    process.env.C_BACKEND_BIN,
    path.join(__dirname, 'bin', 'cs_guardian_c_api.exe'),
    path.join(__dirname, 'bin', 'cs_guardian_c_api'),
  ].filter(Boolean)

  for (const candidate of candidates) {
    if (await fileExists(candidate)) {
      return candidate
    }
  }

  return null
}

async function waitForBackend(url, timeoutMs = 5000) {
  const start = Date.now()

  while (Date.now() - start < timeoutMs) {
    try {
      const response = await fetch(url)

      if (response.ok) {
        return
      }
    } catch {
      await new Promise((resolve) => setTimeout(resolve, 150))
    }
  }

  throw new Error(`Timed out waiting for ${url}`)
}

async function readBody(request) {
  const chunks = []

  for await (const chunk of request) {
    chunks.push(chunk)
  }

  return Buffer.concat(chunks)
}

function copyHeaders(sourceHeaders, response) {
  for (const [key, value] of sourceHeaders.entries()) {
    if (key.toLowerCase() === 'transfer-encoding') {
      continue
    }

    response.setHeader(key, value)
  }
}

function createProxyServer(targetPort, childProcess) {
  const server = http.createServer(async (request, response) => {
    if (childProcess.exitCode !== null) {
      response.writeHead(502, { 'Content-Type': 'application/json' })
      response.end(JSON.stringify({ error: 'The C backend is not running.' }))
      return
    }

    try {
      const body = await readBody(request)
      const upstream = await fetch(`http://127.0.0.1:${targetPort}${request.url}`, {
        method: request.method,
        headers: {
          'content-type': request.headers['content-type'] || 'application/json',
        },
        body: canHaveBody(request.method) ? body : undefined,
      })

      response.statusCode = upstream.status
      copyHeaders(upstream.headers, response)
      const payload = Buffer.from(await upstream.arrayBuffer())
      response.end(payload)
    } catch (error) {
      response.writeHead(502, { 'Content-Type': 'application/json' })
      response.end(JSON.stringify({
        error: `Failed to reach the C backend. ${error.message}`,
      }))
    }
  })

  server.listen(PORT, () => {
    console.log(`CS Guardian API proxy listening on http://localhost:${PORT}`)
  })

  return server
}

async function startCBackendOrThrow() {
  const executable = await findCExecutable()

  if (!executable) {
    throw new Error(
      'No compiled C backend was found. Build server/bin/cs_guardian_c_api first or set C_BACKEND_BIN.',
    )
  }

  const child = spawn(executable, [], {
    cwd: __dirname,
    env: {
      ...process.env,
      CS_GUARDIAN_C_PORT: String(C_BACKEND_PORT),
      CS_GUARDIAN_C_DATA_FILE: process.env.C_BACKEND_DATA_FILE || path.join(__dirname, 'data', 'blocklist.txt'),
      HOSTS_FILE_PATH: process.env.HOSTS_FILE_PATH || path.join(__dirname, 'data', 'hosts_test.txt'),
    },
    stdio: 'inherit',
  })

  const cleanup = () => {
    if (child.exitCode === null) {
      child.kill()
    }
  }

  child.on('error', (error) => {
    console.error(`Failed to start the C backend: ${error.message}`)
  })

  process.on('exit', cleanup)
  process.on('SIGINT', () => {
    cleanup()
    process.exit(0)
  })
  process.on('SIGTERM', () => {
    cleanup()
    process.exit(0)
  })

  await waitForBackend(`http://127.0.0.1:${C_BACKEND_PORT}/api/health`)
  createProxyServer(C_BACKEND_PORT, child)
  console.log(`Using C backend executable: ${executable}`)
}

async function start() {
  if (backendMode === 'js') {
    startJsBackend({ port: PORT })
    return
  }

  try {
    await startCBackendOrThrow()
  } catch (error) {
    if (backendMode === 'c') {
      console.error(error.message)
      process.exit(1)
    }

    console.warn(`C backend unavailable, falling back to JS backend. ${error.message}`)
    startJsBackend({ port: PORT })
  }
}

start()
