import http from 'node:http'
import { URL } from 'node:url'
import {
  getDataFilePath,
  loadBlocklist,
  saveBlocklist,
} from './blocklistStore.js'
import { getEnforcementStatus, syncHostsFile } from './hostsEnforcer.js'

const PORT = Number(process.env.PORT || 3001)

function normalizeSiteInput(value) {
  const trimmedValue = value.trim()

  if (!trimmedValue) {
    throw new Error('Enter a site to continue.')
  }

  const candidate = /^https?:\/\//i.test(trimmedValue)
    ? trimmedValue
    : `https://${trimmedValue}`

  let parsed

  try {
    parsed = new URL(candidate)
  } catch {
    throw new Error('Enter a valid hostname or URL.')
  }

  const hostname = parsed.hostname.replace(/^www\./i, '').toLowerCase()

  if (!hostname || hostname.includes(' ')) {
    throw new Error('Enter a valid hostname or URL.')
  }

  return hostname
}

function json(response, statusCode, payload) {
  response.writeHead(statusCode, {
    'Content-Type': 'application/json',
  })
  response.end(JSON.stringify(payload))
}

function notFound(response) {
  json(response, 404, { error: 'Route not found.' })
}

async function readJsonBody(request) {
  const chunks = []

  for await (const chunk of request) {
    chunks.push(chunk)
  }

  if (chunks.length === 0) {
    return {}
  }

  return JSON.parse(Buffer.concat(chunks).toString('utf8'))
}

function createSiteRecord(currentSites, site) {
  const nextId = currentSites.length
    ? Math.max(...currentSites.map((entry) => entry.id)) + 1
    : 1

  return {
    id: nextId,
    site,
    dateAdded: new Date().toISOString(),
  }
}

const server = http.createServer(async (request, response) => {
  const requestUrl = new URL(request.url, `http://${request.headers.host}`)

  try {
    if (request.method === 'GET' && requestUrl.pathname === '/api/health') {
      const state = await loadBlocklist()
      const enforcement = getEnforcementStatus()

      json(response, 200, {
        status: 'ok',
        service: 'cs-guardian-api',
        enforcementMode: enforcement.mode,
        dataFile: getDataFilePath(),
        hostsFile: enforcement.hostsFile,
        enforcement: enforcement.lastSync,
        siteCount: state.sites.length,
      })
      return
    }

    if (request.method === 'GET' && requestUrl.pathname === '/api/sites') {
      const state = await loadBlocklist()
      json(response, 200, { sites: state.sites })
      return
    }

    if (request.method === 'POST' && requestUrl.pathname === '/api/sites') {
      const body = await readJsonBody(request)
      const site = normalizeSiteInput(body.site ?? '')
      const state = await loadBlocklist()

      if (state.sites.some((entry) => entry.site === site)) {
        json(response, 409, { error: 'That site is already blocked.' })
        return
      }

      const nextSite = createSiteRecord(state.sites, site)
      const nextState = {
        ...state,
        sites: [nextSite, ...state.sites],
      }

      await syncHostsFile(nextState.sites)
      await saveBlocklist(nextState)
      json(response, 201, { site: nextSite, sites: nextState.sites })
      return
    }

    if (request.method === 'DELETE' && requestUrl.pathname === '/api/sites') {
      const state = await loadBlocklist()
      const nextState = {
        ...state,
        sites: [],
      }

      await syncHostsFile(nextState.sites)
      await saveBlocklist(nextState)
      json(response, 200, { sites: [] })
      return
    }

    if (request.method === 'DELETE' && requestUrl.pathname.startsWith('/api/sites/')) {
      const encodedSite = requestUrl.pathname.replace('/api/sites/', '')
      const site = normalizeSiteInput(decodeURIComponent(encodedSite))
      const state = await loadBlocklist()

      if (!state.sites.some((entry) => entry.site === site)) {
        json(response, 404, { error: 'That site is not in the block list.' })
        return
      }

      const nextState = {
        ...state,
        sites: state.sites.filter((entry) => entry.site !== site),
      }

      await syncHostsFile(nextState.sites)
      await saveBlocklist(nextState)
      json(response, 200, { sites: nextState.sites })
      return
    }

    notFound(response)
  } catch (error) {
    if (error instanceof SyntaxError) {
      json(response, 400, { error: 'Invalid JSON body.' })
      return
    }

    json(response, 400, { error: error.message || 'Unexpected server error.' })
  }
})

async function startServer() {
  try {
    const state = await loadBlocklist()
    await syncHostsFile(state.sites)
  } catch (error) {
    console.error(`Initial hosts sync failed: ${error.message}`)
  }

  server.listen(PORT, () => {
    console.log(`CS Guardian API listening on http://localhost:${PORT}`)
  })
}

startServer()
