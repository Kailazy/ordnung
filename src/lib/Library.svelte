<script>
  import { onMount } from 'svelte'
  import { store } from './stores.svelte.js'
  import { listPlaylists, getTracks, importPlaylist, deletePlaylist } from './invoke.js'
  import PlaylistSidebar from './components/PlaylistSidebar.svelte'
  import TrackList from './components/TrackList.svelte'

  async function loadTracks(playlistId) {
    store.activePlaylist = playlistId
    store.search = ''
    store.genreFilter = null
    store.expandedSong = null
    store.tracks = await getTracks(playlistId)
  }

  async function handleImport(files) {
    const txtFiles = Array.from(files).filter(f => f.name.endsWith('.txt'))
    if (txtFiles.length === 0) return

    let lastId = null
    for (const file of txtFiles) {
      // Pass the file path - in Tauri we need to use the file path
      // The file object from drag/drop has a path property in Tauri's webview
      const filePath = file.path || file.name
      const name = file.name.replace(/\.txt$/, '')
      try {
        const result = await importPlaylist(filePath, name)
        if (result && result.id) lastId = result.id
      } catch (e) {
        console.error('Import failed:', e)
      }
    }
    store.playlists = await listPlaylists()
    if (lastId) await loadTracks(lastId)
  }

  async function handleDeletePlaylist(id) {
    if (!confirm('Delete this playlist?')) return
    await deletePlaylist(id)
    if (store.activePlaylist === id) {
      store.activePlaylist = null
      store.tracks = []
    }
    store.playlists = await listPlaylists()
  }

  onMount(async () => {
    store.playlists = await listPlaylists()
  })

  // Drag/drop setup
  let dragging = $state(false)
  let fileInputEl

  function onDrop(e) {
    e.preventDefault()
    dragging = false
    if (e.dataTransfer.files.length) handleImport(e.dataTransfer.files)
  }
</script>

<div class="ft-layout" style="height:100%;display:flex;">
  <div class="ft-sidebar">
    <div class="ft-sidebar-head">
      <h2>playlists</h2>
      <!-- Upload area -->
      <!-- svelte-ignore a11y_no_static_element_interactions -->
      <div
        class="upload-area"
        class:dragging
        role="button"
        tabindex="0"
        onclick={() => fileInputEl.click()}
        onkeydown={(e) => e.key === 'Enter' && fileInputEl.click()}
        ondragover={(e) => { e.preventDefault(); dragging = true }}
        ondragleave={() => dragging = false}
        ondrop={onDrop}
      >
        <input bind:this={fileInputEl} type="file" accept=".txt" multiple style="display:none"
               onchange={(e) => handleImport(e.target.files)} />
        drop or click to import .txt
      </div>
    </div>
    <PlaylistSidebar
      playlists={store.playlists}
      activePlaylist={store.activePlaylist}
      onSelect={loadTracks}
      onDelete={handleDeletePlaylist}
    />
  </div>

  <div class="ft-main">
    {#if store.activePlaylist && store.tracks.length > 0}
      <TrackList />
    {:else if store.activePlaylist}
      <div class="empty">loading...</div>
    {:else}
      <div class="empty">
        select or import a playlist
        <div class="hint">import a rekordbox .txt export to get started</div>
      </div>
    {/if}
  </div>
</div>
