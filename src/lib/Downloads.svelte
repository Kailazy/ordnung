<script>
  import { onMount } from 'svelte'
  import { store } from './stores.svelte.js'
  import { getWatchConfig, setWatchConfig, listDownloads, scanFolder, browseFolder, convertSingle, retryConversion } from './invoke.js'
  import FolderFlow from './components/FolderFlow.svelte'
  import DownloadList from './components/DownloadList.svelte'
  import ActivityLog from './components/ActivityLog.svelte'

  let saving = $state(false)
  let scanning = $state(false)
  let convertingAll = $state(false)

  onMount(async () => {
    const [config, downloads] = await Promise.all([getWatchConfig(), listDownloads()])
    store.watchConfig = config
    store.downloads = downloads
  })

  async function handleSave() {
    saving = true
    try {
      const result = await setWatchConfig(
        store.watchConfig.path,
        store.watchConfig.output_folder,
        store.watchConfig.auto_convert ?? true
      )
      store.watchConfig = result
      store.logLines = [...store.logLines, `${new Date().toLocaleTimeString()}  Config saved`]
    } catch (e) {
      store.logLines = [...store.logLines, `${new Date().toLocaleTimeString()}  ERROR: ${e}`]
    } finally {
      saving = false
    }
  }

  async function handleScan() {
    scanning = true
    store.logLines = [...store.logLines, `${new Date().toLocaleTimeString()}  Scanning folder...`]
    try {
      await handleSave()
      const result = await scanFolder()
      store.logLines = [...store.logLines, `${new Date().toLocaleTimeString()}  Scan complete: ${result.scanned} files found, ${result.added} new`]
      const [config, downloads] = await Promise.all([getWatchConfig(), listDownloads()])
      store.watchConfig = config
      store.downloads = downloads
    } catch (e) {
      store.logLines = [...store.logLines, `${new Date().toLocaleTimeString()}  ERROR: ${e}`]
    } finally {
      scanning = false
    }
  }

  async function handleConvertAll() {
    const unconverted = store.downloads.filter(d => !d.conv_status || d.conv_status === 'failed')
    if (unconverted.length === 0) {
      store.logLines = [...store.logLines, `${new Date().toLocaleTimeString()}  Nothing to convert`]
      return
    }
    convertingAll = true
    store.logLines = [...store.logLines, `${new Date().toLocaleTimeString()}  Queuing ${unconverted.length} files for conversion...`]
    for (const d of unconverted) {
      try {
        await convertSingle(d.id)
        d.conv_status = 'pending'
      } catch (e) {}
    }
    convertingAll = false
    store.logLines = [...store.logLines, `${new Date().toLocaleTimeString()}  All files queued`]
  }

  async function handleBrowse(which) {
    const path = await browseFolder()
    if (path) {
      if (which === 'source') store.watchConfig = { ...store.watchConfig, path }
      else store.watchConfig = { ...store.watchConfig, output_folder: path }
    }
  }
</script>

<div class="dl-layout">
  <div class="dl-config-section">
    <FolderFlow
      srcPath={store.watchConfig.path}
      outPath={store.watchConfig.output_folder}
      onBrowseSrc={() => handleBrowse('source')}
      onBrowseOut={() => handleBrowse('output')}
    />
    <div class="dl-config dl-actions">
      <button class="btn" class:btn-busy={saving} onclick={handleSave} disabled={saving}>
        {saving ? 'saving...' : 'save'}
      </button>
      <button class="btn" class:btn-busy={scanning} onclick={handleScan} disabled={scanning}>
        {scanning ? 'scanning...' : 'scan'}
      </button>
      <button class="btn btn-accent" class:btn-busy={convertingAll} onclick={handleConvertAll} disabled={convertingAll}>
        {convertingAll ? 'queuing...' : 'convert all'}
      </button>
      {#if store.watchConfig.converter_running}
        <span class="dl-status active">
          {store.watchConfig.queue_size > 0 ? `converting (${store.watchConfig.queue_size} queued)` : 'converter ready'}
        </span>
      {/if}
    </div>
  </div>

  <DownloadList />

  <ActivityLog />
</div>
