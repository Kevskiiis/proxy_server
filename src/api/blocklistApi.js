async function parseResponse(response) {
  const payload = await response.json()
  
  if (!response.ok) {
    throw new Error(payload.error || 'Request failed.')
  }

  return payload
}

export async function fetchSites() {
  const response = await fetch('/api/sites')
  return parseResponse(response)
}

export async function addSite(site) {
  const response = await fetch('/api/sites', {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
    },
    body: JSON.stringify({ site }),
  })

  return parseResponse(response)
}

export async function removeSite(site) {
  const response = await fetch(`/api/sites/${encodeURIComponent(site)}`, {
    method: 'DELETE',
  })

  return parseResponse(response)
}

export async function clearSites() {
  const response = await fetch('/api/sites', {
    method: 'DELETE',
  })

  return parseResponse(response)
}

export async function fetchServiceHealth() {
  const response = await fetch('/api/health')
  return parseResponse(response)
}
