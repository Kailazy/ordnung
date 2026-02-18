<script>
  import { store } from '../stores.svelte.js'
  import { updateSongAiff, updateSongPlaylists } from '../invoke.js'

  let { track } = $props()

  function stars(n) { return n ? '★'.repeat(parseInt(n)) : '' }

  async function toggleAiff() {
    const newVal = !track.has_aiff
    await updateSongAiff(track.song_id, !!newVal)
    const t = store.tracks.find(t => t.song_id === track.song_id)
    if (t) t.has_aiff = newVal ? 1 : 0
  }

  async function togglePlaylist(songId, playlistId, add) {
    const addList = add ? [playlistId] : []
    const removeList = add ? [] : [playlistId]
    await updateSongPlaylists(songId, addList, removeList)
    const cached = store.songPlaylistsCache[songId]
    if (cached) {
      const entry = cached.find(p => p.id === playlistId)
      if (entry) entry.member = add
    }
  }

  const playlists = $derived(store.songPlaylistsCache[track.song_id])
</script>

<div class="track-detail">
  <div class="detail-section">
    <div class="detail-grid">
      {#if track.album}<div class="detail-kv"><span class="detail-k">album</span><span class="detail-v">{track.album}</span></div>{/if}
      {#if track.genre}<div class="detail-kv"><span class="detail-k">genre</span><span class="detail-v">{track.genre}</span></div>{/if}
      {#if track.bpm}<div class="detail-kv"><span class="detail-k">bpm</span><span class="detail-v">{parseFloat(track.bpm).toFixed(0)}</span></div>{/if}
      {#if track.key_sig}<div class="detail-kv"><span class="detail-k">key</span><span class="detail-v">{track.key_sig}</span></div>{/if}
      {#if track.time}<div class="detail-kv"><span class="detail-k">time</span><span class="detail-v">{track.time}</span></div>{/if}
      {#if track.rating}<div class="detail-kv"><span class="detail-k">rating</span><span class="detail-v">{stars(track.rating)}</span></div>{/if}
      {#if track.date_added}<div class="detail-kv"><span class="detail-k">added</span><span class="detail-v">{track.date_added}</span></div>{/if}
    </div>
  </div>
  <div class="detail-section">
    <div class="detail-badges">
      <span class="detail-k">status</span>
      <span class="status-badge badge-{track.format || 'mp3'}">{track.format || 'mp3'}</span>
      {#if track.has_aiff}<span class="status-badge badge-aiff">aiff</span>{/if}
      <button class="btn btn-sm" class:badge-aiff-active={track.has_aiff} onclick={toggleAiff}>
        {track.has_aiff ? 'has aiff' : 'no aiff'}
      </button>
    </div>
  </div>
  <div class="detail-section">
    <span class="detail-k">playlists</span>
    <div class="detail-playlists">
      {#if !playlists}
        <span class="detail-loading">loading...</span>
      {:else}
        {#each playlists as p (p.id)}
          <label class="pl-check" class:current-pl={p.id === store.activePlaylist}>
            <input type="checkbox" checked={p.member}
                   onchange={(e) => togglePlaylist(track.song_id, p.id, e.target.checked)} />
            <span>{p.name}</span>
          </label>
        {/each}
      {/if}
    </div>
  </div>
</div>
