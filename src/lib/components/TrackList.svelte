<script>
  import { store } from '../stores.svelte.js'
  import { listPlaylists, bulkUpdateFormat, exportTracks } from '../invoke.js'
  import GenreBar from './GenreBar.svelte'
  import TrackRow from './TrackRow.svelte'

  const AUDIO_FORMATS = ['mp3','flac','wav','aiff','ogg','m4a','alac','wma','aac']

  let bulkFormatSel = $state('mp3')

  const filteredTracks = $derived.by(() => store.tracks.filter(t => {
    if (store.search) {
      const q = store.search.toLowerCase()
      const hay = `${t.title} ${t.artist} ${t.album} ${t.genre} ${t.key_sig}`.toLowerCase()
      if (!hay.includes(q)) return false
    }
    if (store.genreFilter) {
      if (!t.genre) return false
      const parts = t.genre.toLowerCase().split(/[;,\/]/).map(s => s.trim())
      if (!parts.some(p => p.includes(store.genreFilter))) return false
    }
    return true
  }))

  const formatStats = $derived.by(() => {
    const counts = {}
    store.tracks.forEach(t => {
      const fmt = t.format || 'mp3'
      counts[fmt] = (counts[fmt] || 0) + 1
    })
    return counts
  })

  async function doBulkFormat(ids) {
    const affected = ids
      ? store.tracks.filter(t => ids.includes(t.song_id))
      : store.tracks
    store.formatUndo = affected.map(t => ({ id: t.song_id, format: t.format }))

    if (ids) {
      await bulkUpdateFormat(bulkFormatSel, ids, null)
      store.tracks.forEach(t => { if (ids.includes(t.song_id)) t.format = bulkFormatSel })
    } else {
      await bulkUpdateFormat(bulkFormatSel, null, store.activePlaylist)
      store.tracks.forEach(t => { t.format = bulkFormatSel })
    }
    store.playlists = await listPlaylists()
  }

  async function undoBulk() {
    if (!store.formatUndo) return
    const snapshot = store.formatUndo
    store.formatUndo = null

    const byFormat = {}
    for (const s of snapshot) {
      if (!byFormat[s.format]) byFormat[s.format] = []
      byFormat[s.format].push(s.id)
    }
    for (const [fmt, ids] of Object.entries(byFormat)) {
      await bulkUpdateFormat(fmt, ids, null)
      store.tracks.forEach(t => { if (ids.includes(t.song_id)) t.format = fmt })
    }
    store.playlists = await listPlaylists()
  }

  async function showExport() {
    const result = await exportTracks(store.activePlaylist, null)
    store.modalContent = `
      <h2>export track list</h2>
      <pre id="export-pre">${result.tracks.join('\n')}</pre>
      <div class="btn-row">
        <button class="btn" onclick="navigator.clipboard.writeText(document.getElementById('export-pre').textContent)">copy</button>
        <button class="btn" onclick="window.__closeModal()">close</button>
      </div>
    `
    window.__closeModal = () => { store.modalContent = null }
  }
</script>

<div class="ft-toolbar">
  <input type="text" class="ft-search" placeholder="search tracks, artists, albums, genres..."
         bind:value={store.search} autocomplete="off" />
  <div class="ft-stats">
    <span>{store.tracks.length} tracks</span>
    {#each Object.entries(formatStats).sort((a,b) => b[1]-a[1]) as [fmt, count]}
      <span class="fmt-tag fmt-{fmt}">{count} {fmt}</span>
    {/each}
  </div>
  <div class="bulk-controls">
    <select class="format-select" bind:value={bulkFormatSel}>
      {#each AUDIO_FORMATS as f}<option value={f}>{f}</option>{/each}
    </select>
    <button class="btn" onclick={() => doBulkFormat(null)}>set all</button>
    <button class="btn" onclick={() => doBulkFormat(filteredTracks.map(t => t.song_id))}>set visible</button>
  </div>
  {#if store.formatUndo}
    <button class="btn btn-undo" onclick={undoBulk}>undo</button>
  {/if}
  <button class="btn btn-accent" onclick={showExport}>export</button>
</div>

<GenreBar />

<div class="ft-tracks">
  {#if filteredTracks.length === 0}
    <div class="empty">no tracks match</div>
  {:else}
    {#each filteredTracks as t (t.song_id)}
      <TrackRow track={t} />
    {/each}
  {/if}
</div>
