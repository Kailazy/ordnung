<script>
  import { store } from '../stores.svelte.js'

  const topGenres = $derived.by(() => {
    const freq = {}
    store.tracks.forEach(t => {
      if (t.genre) {
        t.genre.split(/[;,\/]/).forEach(g => {
          const trimmed = g.trim().toLowerCase()
          if (trimmed && trimmed.length > 1) freq[trimmed] = (freq[trimmed] || 0) + 1
        })
      }
    })
    return Object.entries(freq).sort((a,b) => b[1]-a[1]).slice(0,15).map(e => e[0])
  })

  function toggleGenre(g) {
    store.genreFilter = store.genreFilter === g ? null : g
  }
</script>

<div class="ft-genres">
  {#each topGenres as g}
    <button class="genre-tag" class:active={store.genreFilter === g} onclick={() => toggleGenre(g)}>{g}</button>
  {/each}
</div>
