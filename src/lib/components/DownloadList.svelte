<script>
  import { store } from '../stores.svelte.js'
  import { deleteDownload, convertSingle, retryConversion } from '../invoke.js'

  function extClass(ext) {
    const classes = {
      flac: 'ext-flac', mp3: 'ext-mp3',
      wav: 'ext-wav', aiff: 'ext-wav', aif: 'ext-wav',
      ogg: 'ext-ogg', m4a: 'ext-m4a', alac: 'ext-alac',
      wma: 'ext-wma', aac: 'ext-aac', opus: 'ext-ogg',
    }
    return classes[ext] || 'ext-other'
  }

  async function handleDelete(id) {
    await deleteDownload(id)
    store.downloads = store.downloads.filter(d => d.id !== id)
  }

  async function handleConvert(downloadId) {
    try {
      const result = await convertSingle(downloadId)
      const d = store.downloads.find(x => x.id === downloadId)
      if (d) { d.conv_status = 'pending'; d.conv_id = result.conversion_id }
    } catch (e) { alert(String(e)) }
  }

  async function handleRetry(convId) {
    try {
      await retryConversion(convId)
      const d = store.downloads.find(x => x.conv_id === convId)
      if (d) d.conv_status = 'pending'
    } catch (e) { alert(String(e)) }
  }
</script>

<div class="dl-table">
  {#if store.downloads.length === 0}
    <div class="empty">
      no files detected yet
      <div class="hint">set a source folder and output folder, then click scan</div>
    </div>
  {:else}
    {#each store.downloads as d (d.id)}
      <div class="dl-row">
        <span class="fname" title={d.filepath}>{d.filename}</span>
        <span class="ext {extClass(d.extension)}">{d.extension}</span>
        <span class="size">{d.size_mb} MB</span>
        <span class="conv-cell">
          {#if d.conv_status === 'done'}
            <span class="conv-badge conv-done">converted</span>
          {:else if d.conv_status === 'converting'}
            <span class="conv-badge conv-active">converting...</span>
          {:else if d.conv_status === 'pending'}
            <span class="conv-badge conv-pending">queued</span>
          {:else if d.conv_status === 'failed'}
            <span class="conv-badge conv-failed" title={d.conv_error || ''}>failed</span>
            <button class="btn btn-sm" onclick={() => handleRetry(d.conv_id)}>retry</button>
          {:else}
            <button class="btn btn-sm" onclick={() => handleConvert(d.id)}>convert</button>
          {/if}
        </span>
        <button class="del-btn" onclick={() => handleDelete(d.id)} title="remove">&times;</button>
      </div>
    {/each}
  {/if}
</div>
