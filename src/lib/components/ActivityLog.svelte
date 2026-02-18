<script>
  import { store } from '../stores.svelte.js'
  import { afterUpdate } from 'svelte'

  let logEl = $state(null)
  let pulsing = $state(false)
  let prevLen = 0

  afterUpdate(() => {
    if (store.logLines.length !== prevLen) {
      prevLen = store.logLines.length
      pulsing = true
      setTimeout(() => { pulsing = false }, 1200)
      if (logEl) logEl.scrollTop = logEl.scrollHeight
    }
  })
</script>

<div class="dl-log-section">
  <div class="dl-log-header">
    <span class="dl-log-title">activity log</span>
    <span class="dl-log-dot" class:pulse={pulsing}></span>
  </div>
  <div class="dl-log" bind:this={logEl}>
    {#each store.logLines as line}
      <div class="dl-log-line"
           class:log-error={line.includes('ERROR')}
           class:log-warn={line.includes('WARNING') || line.includes('WARN')}>
        {line}
      </div>
    {/each}
  </div>
</div>
