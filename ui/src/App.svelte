<script>
  import { onMount } from 'svelte'
  import { fetchAreas, fetchProviders, fetchRaw, fetchProvider } from './lib/api.js'
  import PriceChart from './lib/PriceChart.svelte'

  // ── State ──────────────────────────────────────────────────────────────────
  let areas            = $state({})   // { nl: { name, timezone }, ... }
  let area             = $state(null) // selected area ID
  let day              = $state('today')
  let selectedProvider = $state(null) // null → raw EPEX prices
  let providers        = $state({})   // providers for the selected area
  let data             = $state(null)
  let loading          = $state(true)
  let error            = $state(null)

  // ── Bootstrap ──────────────────────────────────────────────────────────────
  onMount(async () => {
    try {
      const resp = await fetchAreas()
      areas = resp.areas
      const first = Object.keys(resp.areas).sort()[0]
      if (first) await selectArea(first)
    } catch (e) {
      error = e.message
      loading = false
    }
  })

  // ── Area selection: load that area's providers, reset provider choice ──────
  async function selectArea(newArea) {
    if (area === newArea) return
    area             = newArea
    selectedProvider = null
    providers        = {}
    try {
      const resp = await fetchProviders(newArea)
      providers = resp.providers
    } catch { /* non-fatal — provider list stays empty */ }
  }

  // ── Fetch prices whenever area, day, or provider changes ──────────────────
  $effect(() => {
    const a = area
    const d = day
    const p = selectedProvider
    if (!a) return

    loading = true
    error   = null
    data    = null

    let cancelled = false

    const req = p === null
      ? fetchRaw(a, d)
      : fetchProvider(a, p, d)

    req
      .then(r  => { if (!cancelled) data    = r         })
      .catch(e => { if (!cancelled) error   = e.message })
      .finally(() => { if (!cancelled) loading = false  })

    return () => { cancelled = true }
  })

  // ── Derived: normalised slot array (always €/kWh) ─────────────────────────
  const slots = $derived.by(() => {
    if (!data?.prices?.length) return []
    if (data.unit === 'MWh') {
      return data.prices.map(s => ({ start: s.start, end: s.end, value: s.price / 1000 }))
    }
    return data.prices.map(s => ({ start: s.start, end: s.end, value: s.buy, sell: s.sell }))
  })

  // ── Derived: current 15-min slot (today only) ─────────────────────────────
  const currentSlot = $derived.by(() => {
    if (day !== 'today' || !slots.length) return null
    const now = new Date()
    return slots.find(s => new Date(s.start) <= now && now < new Date(s.end)) ?? null
  })

  // ── Helpers ────────────────────────────────────────────────────────────────
  function fmt(v) {
    return `€ ${v.toFixed(4)}`
  }

  const unitNote = $derived(
    selectedProvider === null
      ? 'EPEX spotprijs excl. belastingen en leveringskosten'
      : `All-in prijs incl. energiebelasting, BTW en leveringskosten (${providers[selectedProvider]?.name ?? selectedProvider})`
  )

  const multiArea = $derived(Object.keys(areas).length > 1)
</script>

<header>
  <div class="container">
    <hgroup>
      <h1>⚡ EPEX prijzen</h1>
      <p>
        Dag-vooruit elektriciteitsprijzen
        {#if area && !multiArea}· {areas[area]?.name ?? area.toUpperCase()}{/if}
      </p>
    </hgroup>
  </div>
</header>

<main class="container">

  <!-- Area picker (only shown when more than one area is configured) -->
  {#if multiArea}
    <div class="btn-group" role="group" aria-label="Gebied">
      {#each Object.keys(areas).sort() as id}
        <button
          aria-pressed={area === id}
          onclick={() => selectArea(id)}
        >
          <abbr title={areas[id].name}>{id.toUpperCase()}</abbr>
        </button>
      {/each}
    </div>
  {/if}

  <!-- Day picker -->
  <div class="btn-group" role="group" aria-label="Dag">
    <button
      aria-pressed={day === 'today'}
      onclick={() => day = 'today'}
    >Vandaag</button>
    <button
      aria-pressed={day === 'tomorrow'}
      onclick={() => day = 'tomorrow'}
    >Morgen</button>
  </div>

  <!-- Provider picker -->
  <div class="btn-group" role="group" aria-label="Aanbieder">
    <button
      aria-pressed={selectedProvider === null}
      onclick={() => selectedProvider = null}
    >Raw</button>
    {#each Object.entries(providers) as [id, p]}
      <button
        aria-pressed={selectedProvider === id}
        onclick={() => selectedProvider = id}
      >{p.name}</button>
    {/each}
  </div>

  <!-- Current price card (today only, once data is loaded) -->
  {#if day === 'today' && currentSlot}
    <article class="current-price">
      <div>
        <p class="now-label">Nu</p>
        <p class="buy-price">
          {fmt(currentSlot.value)}<small> /kWh</small>
        </p>
      </div>
      {#if currentSlot.sell !== undefined}
        <p class="sell-price">Teruglevering: {fmt(currentSlot.sell)}/kWh</p>
      {/if}
    </article>
  {/if}

  <!-- Chart / loading / error -->
  {#if loading}
    <p class="msg" aria-busy="true">Laden…</p>
  {:else if error}
    <p class="msg">
      {#if day === 'tomorrow'}
        Prijzen voor morgen zijn nog niet gepubliceerd (verschijnen rond 13:00 CET).
      {:else}
        {error}
      {/if}
    </p>
  {:else if slots.length}
    <PriceChart {slots} {day} />
    <p style="font-size: 0.75rem; color: var(--pico-muted-color); margin-top: 0.25rem;">
      {unitNote}
    </p>
  {/if}

</main>
