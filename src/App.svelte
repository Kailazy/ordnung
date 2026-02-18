<script>
  import { onMount } from 'svelte'
  import { listen } from '@tauri-apps/api/event'
  import { store } from './lib/stores.svelte.js'
  import Library from './lib/Library.svelte'
  import Downloads from './lib/Downloads.svelte'
  import { listPlaylists } from './lib/invoke.js'

  onMount(async () => {
    // Load initial data
    store.playlists = await listPlaylists()

    // Subscribe to Tauri events (replaces setInterval polling)
    await listen('conversion-update', (event) => {
      const update = event.payload
      // Update matching download in store.downloads
      const idx = store.downloads.findIndex(d => d.conv_id === update.conv_id || d.id === update.download_id)
      if (idx !== -1) {
        store.downloads[idx] = { ...store.downloads[idx], ...update }
      }
    })

    await listen('download-detected', (event) => {
      store.downloads = [event.payload, ...store.downloads]
    })

    await listen('log-line', (event) => {
      store.logLines = [...store.logLines, event.payload]
      if (store.logLines.length > 200) store.logLines = store.logLines.slice(-200)
    })
  })

  function setTab(t) {
    store.tab = t
  }
</script>

<div class="shell">
  <nav class="sidebar">
    <div class="logo">eyebags<br>terminal</div>
    <div class="nav-tabs">
      <button class="nav-btn" class:active={store.tab === 'library'} onclick={() => setTab('library')}>library</button>
      <button class="nav-btn" class:active={store.tab === 'downloads'} onclick={() => setTab('downloads')}>downloads</button>
    </div>
    <div class="sidebar-footer">eyebags v0.1</div>
  </nav>

  <main class="content">
    {#if store.tab === 'library'}
      <Library />
    {:else if store.tab === 'downloads'}
      <Downloads />
    {/if}
  </main>
</div>

{#if store.modalContent}
  <!-- svelte-ignore a11y_click_events_have_key_events -->
  <!-- svelte-ignore a11y_no_static_element_interactions -->
  <div class="modal-overlay visible" onclick={(e) => { if (e.target === e.currentTarget) store.modalContent = null }}>
    <div class="modal">
      {@html store.modalContent}
    </div>
  </div>
{/if}
