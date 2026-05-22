const BASE = '/api'

async function get(path) {
  const res = await fetch(BASE + path)
  if (!res.ok) {
    const body = await res.json().catch(() => ({}))
    throw new Error(body.reason ?? `HTTP ${res.status}`)
  }
  return res.json()
}

export const fetchAreas     = ()                       => get(`/v1/areas`)
export const fetchProviders = (area)                   => get(`/v1/${area}/providers`)
export const fetchRaw       = (area, date)             => get(`/v1/${area}/raw/${date}`)
export const fetchProvider  = (area, provider, date)   => get(`/v1/${area}/${provider}/${date}`)
