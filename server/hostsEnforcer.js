import { execFile } from 'node:child_process'
import { promisify } from 'node:util'
import { readFile, writeFile } from 'node:fs/promises'

const HOSTS_BLOCK_START = '# CS_GUARDIAN_START'
const HOSTS_BLOCK_END = '# CS_GUARDIAN_END'
const execFileAsync = promisify(execFile)

function defaultHostsPath() {
  return process.env.HOSTS_FILE_PATH
    || 'C:\\Windows\\System32\\drivers\\etc\\hosts'
}

function uniqueHostnames(sites) {
  return [...new Set(
    sites.flatMap((entry) => (
      entry.site.startsWith('www.')
        ? [entry.site]
        : [entry.site, `www.${entry.site}`]
    )),
  )]
}

function buildManagedBlock(sites) {
  const hostnames = uniqueHostnames(sites)

  if (hostnames.length === 0) {
    return ''
  }

  const entries = hostnames.flatMap((hostname) => ([
    `127.0.0.1 ${hostname}`,
    `::1 ${hostname}`,
  ]))

  return [
    HOSTS_BLOCK_START,
    '# Managed by CS Guardian',
    ...entries,
    HOSTS_BLOCK_END,
  ].join('\n')
}

function stripManagedBlock(content) {
  const managedBlockPattern =
    /\r?\n?# CS_GUARDIAN_START[\s\S]*?# CS_GUARDIAN_END\r?\n?/g

  return content.replace(managedBlockPattern, '\n').trimEnd()
}

let lastSync = {
  ok: true,
  message: 'Hosts file has not been synced yet.',
  syncedAt: null,
  dnsFlushed: false,
}

export function getEnforcementStatus() {
  return {
    mode: 'hosts-file',
    hostsFile: defaultHostsPath(),
    lastSync,
  }
}

async function flushDnsCache() {
  try {
    await execFileAsync('ipconfig', ['/flushdns'])

    return {
      ok: true,
      message: 'Windows DNS cache flushed.',
    }
  } catch (error) {
    return {
      ok: false,
      message: `Hosts file updated, but DNS flush failed: ${error.message}`,
    }
  }
}

export async function syncHostsFile(sites) {
  const hostsFile = defaultHostsPath()

  try {
    let current = ''

    try {
      current = await readFile(hostsFile, 'utf8')
    } catch (error) {
      if (error.code !== 'ENOENT') {
        throw error
      }
    }

    const cleaned = stripManagedBlock(current)
    const managedBlock = buildManagedBlock(sites)
    const nextContent = [cleaned, managedBlock]
      .filter(Boolean)
      .join('\n\n')
      .concat('\n')

    await writeFile(hostsFile, nextContent, 'utf8')
    const dnsFlush = await flushDnsCache()

    lastSync = {
      ok: dnsFlush.ok,
      message: dnsFlush.ok
        ? `Synced ${sites.length} blocked site(s) to hosts file and flushed DNS cache.`
        : dnsFlush.message,
      syncedAt: new Date().toISOString(),
      dnsFlushed: dnsFlush.ok,
    }

    if (!dnsFlush.ok) {
      throw new Error(dnsFlush.message)
    }
  } catch (error) {
    const message = error.code === 'EACCES' || error.code === 'EPERM'
      ? 'Permission denied while writing the hosts file. Run the server as Administrator.'
      : error.message

    lastSync = {
      ok: false,
      message,
      syncedAt: new Date().toISOString(),
      dnsFlushed: false,
    }

    throw new Error(message)
  }
}
