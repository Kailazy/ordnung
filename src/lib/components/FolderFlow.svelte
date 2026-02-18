<script>
  let { srcPath, outPath, onBrowseSrc, onBrowseOut } = $props()

  function truncPath(p, max) {
    if (!p) return 'click to set'
    return p.length > max ? '...' + p.slice(-(max - 3)) : p
  }
</script>

<div class="folder-flow">
  <!-- svelte-ignore a11y_no_static_element_interactions -->
  <div class="folder-node" onclick={onBrowseSrc} title={srcPath || 'click to browse'}
       role="button" tabindex="0" onkeydown={(e) => e.key === 'Enter' && onBrowseSrc()}>
    <svg class="folder-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5">
      <path d="M3 7v10a2 2 0 002 2h14a2 2 0 002-2V9a2 2 0 00-2-2h-6l-2-2H5a2 2 0 00-2 2z"/>
    </svg>
    <span class="folder-node-label">source</span>
    <span class="folder-node-path">{truncPath(srcPath, 38)}</span>
  </div>

  <div class="folder-arrow">
    <svg viewBox="0 0 48 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
      <line x1="2" y1="12" x2="38" y2="12"/>
      <polyline points="32,6 38,12 32,18"/>
    </svg>
  </div>

  <!-- svelte-ignore a11y_no_static_element_interactions -->
  <div class="folder-node folder-node-out" onclick={onBrowseOut} title={outPath || 'click to browse'}
       role="button" tabindex="0" onkeydown={(e) => e.key === 'Enter' && onBrowseOut()}>
    <svg class="folder-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5">
      <path d="M3 7v10a2 2 0 002 2h14a2 2 0 002-2V9a2 2 0 00-2-2h-6l-2-2H5a2 2 0 00-2 2z"/>
    </svg>
    <span class="folder-node-label">aiff output</span>
    <span class="folder-node-path">{truncPath(outPath, 38)}</span>
  </div>
</div>
