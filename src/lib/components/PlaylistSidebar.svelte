<script>
  let { playlists, activePlaylist, onSelect, onDelete } = $props()

  function formatCounts(counts) {
    if (!counts || Object.keys(counts).length === 0) return ''
    return Object.entries(counts)
      .sort((a, b) => b[1] - a[1])
      .map(([fmt, count]) => `${count} ${fmt}`)
      .join(' · ')
  }
</script>

<div class="ft-playlists">
  {#if playlists.length === 0}
    <div class="empty" style="padding:20px;font-size:11px;">no playlists yet</div>
  {:else}
    {#each playlists as p (p.id)}
      <div
        class="ft-playlist-item"
        class:active={p.id === activePlaylist}
        role="button"
        tabindex="0"
        onclick={() => onSelect(p.id)}
        onkeydown={(e) => e.key === 'Enter' && onSelect(p.id)}
      >
        <div style="display:flex;align-items:center;gap:6px;">
          <span class="ft-playlist-name">{p.name}</span>
          <button class="ft-playlist-delete"
                  onclick={(e) => { e.stopPropagation(); onDelete(p.id) }}
                  title="delete">&times;</button>
        </div>
        <div class="ft-playlist-meta">
          {p.total} tracks{formatCounts(p.format_counts) ? ' · ' + formatCounts(p.format_counts) : ''}
        </div>
      </div>
    {/each}
  {/if}
</div>
