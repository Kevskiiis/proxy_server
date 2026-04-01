import { mkdir, readFile, writeFile } from 'node:fs/promises'
import path from 'node:path'
import { fileURLToPath } from 'node:url'

const __dirname = path.dirname(fileURLToPath(import.meta.url))
const dataDir = path.join(__dirname, 'data')
const dataFile = path.join(dataDir, 'blocklist.json')

const defaultState = {
  version: 1,
  sites: [],
}

async function ensureStore() {
  await mkdir(dataDir, { recursive: true })

  try {
    await readFile(dataFile, 'utf8')
  } catch {
    await writeFile(dataFile, JSON.stringify(defaultState, null, 2))
  }
}

export async function loadBlocklist() {
  await ensureStore()

  const raw = await readFile(dataFile, 'utf8')
  const parsed = JSON.parse(raw)

  if (!parsed || !Array.isArray(parsed.sites)) {
    return { ...defaultState }
  }

  return parsed
}

export async function saveBlocklist(state) {
  await ensureStore()
  await writeFile(dataFile, JSON.stringify(state, null, 2))
}

export function getDataFilePath() {
  return dataFile
}
