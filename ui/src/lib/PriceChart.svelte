<script>
  /** @type {{ start: string, end: string, value: number, sell?: number }[]} */
  let { slots, day = 'today' } = $props()

  // ── Layout constants ──────────────────────────────────────────────────────
  const W     = 960
  const H     = 180
  const PAD   = { t: 8, r: 8, b: 26, l: 50 }
  const plotW = W - PAD.l - PAD.r
  const plotH = H - PAD.t - PAD.b

  // ── Derived geometry ──────────────────────────────────────────────────────
  const computed = $derived.by(() => {
    if (!slots.length) return null

    const values = slots.map(s => s.value)
    const yMin   = Math.min(0, ...values)
    const yMax   = Math.max(0.001, ...values)   // never zero-height range
    const yRange = yMax - yMin
    const barW   = plotW / slots.length
    const y0     = PAD.t + (yMax / yRange) * plotH  // y-coordinate of price = 0

    // Scale helpers
    const scaleY = v  => PAD.t + (1 - (v - yMin) / yRange) * plotH
    const barH   = v  => Math.max(1, Math.abs(v) / yRange * plotH)
    const barY   = v  => v >= 0 ? y0 - barH(v) : y0
    const barX   = i  => PAD.l + i * barW

    // Y-axis: 5 evenly-spaced ticks, formatted as €
    const yTicks = Array.from({ length: 5 }, (_, i) => {
      const v = yMin + (i / 4) * yRange
      return { v, y: scaleY(v), label: v.toFixed(3) }
    })

    // X-axis: one label every 4 hours (every 16 slots)
    const xTicks = slots
      .filter((_, i) => i % 16 === 0)
      .map((s, j) => ({
        x: barX(j * 16) + barW / 2,
        label: localTime(s.start)
      }))

    return { yMin, barW, y0, barH, barY, barX, yTicks, xTicks }
  })

  // ── Helpers ────────────────────────────────────────────────────────────────
  function localTime(iso) {
    return new Date(iso).toLocaleTimeString('nl-NL', {
      timeZone: 'Europe/Amsterdam',
      hour: '2-digit', minute: '2-digit', hour12: false
    })
  }

  function isCurrent(s) {
    if (day !== 'today') return false
    const now = new Date()
    return new Date(s.start) <= now && now < new Date(s.end)
  }

  function fmt(v) {
    return `€ ${v.toFixed(4)}`
  }

  // ── Hover state ────────────────────────────────────────────────────────────
  let hovered = $state(null)
</script>

{#if computed}
  {@const c = computed}
  <div class="chart-wrap">
    <svg viewBox="0 0 {W} {H}" role="img" aria-label="Prijsgrafiek">

      <!-- Grid lines + Y-axis ticks -->
      {#each c.yTicks as t}
        <line x1={PAD.l - 4} y1={t.y} x2={PAD.l}      y2={t.y} class="tick" />
        <line x1={PAD.l}     y1={t.y} x2={W - PAD.r}   y2={t.y} class="grid" />
        <text x={PAD.l - 8} y={t.y} text-anchor="end" dominant-baseline="middle" class="axis-lbl">
          {t.label}
        </text>
      {/each}

      <!-- Zero line (only when negative prices exist) -->
      {#if c.yMin < 0}
        <line x1={PAD.l} y1={c.y0} x2={W - PAD.r} y2={c.y0} class="zero-line" />
      {/if}

      <!-- Y-axis spine -->
      <line x1={PAD.l} y1={PAD.t} x2={PAD.l} y2={PAD.t + plotH} class="tick" />

      <!-- Bars -->
      {#each slots as s, i}
        <rect
          x={c.barX(i) + 0.5}
          y={c.barY(s.value)}
          width={Math.max(1, c.barW - 1)}
          height={c.barH(s.value)}
          role="button"
          aria-label="{localTime(s.start)}: {fmt(s.value)}/kWh"
          tabindex="0"
          class="bar"
          class:bar--current={isCurrent(s)}
          class:bar--negative={s.value < 0}
          class:bar--hover={hovered === i}
          onmouseenter={() => hovered = i}
          onmouseleave={() => hovered = null}
          onfocus={() => hovered = i}
          onblur={() => hovered = null}
        />
      {/each}

      <!-- X-axis labels -->
      {#each c.xTicks as t}
        <text x={t.x} y={H - 4} text-anchor="middle" class="axis-lbl">{t.label}</text>
      {/each}

    </svg>

    <!-- Info strip — fixed height so the layout doesn't jump -->
    <p class="slot-info" aria-live="polite">
      {#if hovered !== null}
        {@const s = slots[hovered]}
        <strong>{localTime(s.start)}–{localTime(s.end)}</strong>
        &nbsp;{fmt(s.value)}/kWh
        {#if s.sell !== undefined}
          &nbsp;&nbsp;teruglevering: {fmt(s.sell)}/kWh
        {/if}
        {#if isCurrent(s)}
          &nbsp;<mark>nu</mark>
        {/if}
      {:else}
        &nbsp;
      {/if}
    </p>
  </div>
{/if}

<style>
  .chart-wrap { width: 100%; }

  svg {
    width: 100%;
    height: auto;
    display: block;
  }

  /* Bars */
  .bar          { fill: var(--pico-primary); transition: opacity 0.08s; }
  .bar--hover   { opacity: 0.75; }
  .bar--current { fill: #f59e0b; }
  .bar--negative { fill: #dc2626; }

  /* Axes */
  .tick      { stroke: currentColor; stroke-width: 1; opacity: 0.25; }
  .grid      { stroke: currentColor; stroke-width: 1; opacity: 0.07; }
  .zero-line { stroke: currentColor; stroke-width: 1; opacity: 0.35; stroke-dasharray: 4 3; }

  .axis-lbl {
    font-size: 11px;
    fill: currentColor;
    opacity: 0.55;
    font-family: inherit;
  }

  /* Hover info */
  .slot-info {
    min-height: 1.6rem;
    font-size: 0.8125rem;
    margin: 0.25rem 0 0;
    color: var(--pico-muted-color);
  }

  .slot-info mark {
    font-size: 0.7rem;
    padding: 0.1em 0.4em;
    border-radius: 3px;
    vertical-align: middle;
  }
</style>
