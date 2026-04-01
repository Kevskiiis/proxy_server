export function normalizeSiteInput(value) {
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

export function formatDate(value) {
  const date = new Date(value)

  if (Number.isNaN(date.getTime())) {
    return 'Unknown'
  }

  return new Intl.DateTimeFormat('en-US', {
    year: 'numeric',
    month: 'short',
    day: 'numeric',
  }).format(date)
}
