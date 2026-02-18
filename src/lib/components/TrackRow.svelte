<script>
  import { store } from '../stores.svelte.js'
  import { updateSongFormat, getSongPlaylists } from '../invoke.js'
  import TrackDetail from './TrackDetail.svelte'

  let { track } = $props()

  const AUDIO_FORMATS = ['mp3','flac','wav','aiff','ogg','m4a','alac','wma','aac']

  const isExpanded = $derived(store.expandedSong === track.song_id)

  function formatBadgeClass(fmt) {
    const classes = { mp3:'badge-mp3', flac:'badge-flac', wav:'badge-wav', aiff:'badge-aiff', aif:'badge-aiff', ogg:'badge-ogg', m4a:'badge-m4a', alac:'badge-alac', wma:'badge-wma', aac:'badge-aac', opus:'badge-ogg' }
    return classes[fmt] || 'badge-other'
  }

  function toggleDetail() {
    if (store.expandedSong === track.song_id) {
      store.expandedSong = null
    } else {
      store.expandedSong = track.song_id
      if (!store.songPlaylistsCache[track.song_id]) {
        getSongPlaylists(track.song_id).then(pls => {
          store.songPlaylistsCache[track.song_id] = pls
        })
      }
    }
  }

  async function onFormatChange(e) {
    const fmt = e.target.value
    await updateSongFormat(track.song_id, fmt)
    // Update local track state
    const t = store.tracks.find(t => t.song_id === track.song_id)
    if (t) t.format = fmt
  }
</script>

<div class="track-row-wrap" class:expanded={isExpanded}>
  <div class="track-row" onclick={toggleDetail} role="button" tabindex="0" onkeydown={(e) => e.key==='Enter'&&toggleDetail()}>
    <span class="track-chevron">{isExpanded ? '▾' : '▸'}</span>
    <div class="track-info">
      <div class="track-main">
        <span class="track-title" title={track.title}>{track.title}</span>
        {#if track.artist}
          <span class="track-artist">&mdash; {track.artist}</span>
        {/if}
        <span class="track-badges">
          {#if track.format !== 'mp3'}
            <span class="status-badge {formatBadgeClass(track.format)}">{track.format}</span>
          {/if}
          {#if track.has_aiff}
            <span class="status-badge badge-aiff">aiff ✓</span>
          {/if}
        </span>
      </div>
      <div class="track-meta">
        {#if track.bpm}<span class="bpm-tag">{parseFloat(track.bpm).toFixed(0)} bpm</span>{/if}
        {#if track.key_sig}<span class="key-tag">{track.key_sig}</span>{/if}
        {#if track.time}<span>{track.time}</span>{/if}
      </div>
    </div>
    <!-- svelte-ignore a11y_no_static_element_interactions -->
    <div class="format-toggle" onclick={(e) => e.stopPropagation()}>
      <select class="format-select" value={track.format} onchange={onFormatChange}>
        {#each AUDIO_FORMATS as f}<option value={f}>{f}</option>{/each}
      </select>
    </div>
  </div>
  {#if isExpanded}
    <TrackDetail {track} />
  {/if}
</div>
