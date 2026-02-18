// Svelte 5 runes — single reactive state object so consumers can mutate properties freely
export const store = $state({
  tab: 'library',
  playlists: [],
  activePlaylist: null,
  tracks: [],
  search: '',
  genreFilter: null,
  formatUndo: null,
  expandedSong: null,
  songPlaylistsCache: {},
  downloads: [],
  watchConfig: { path: '', output_folder: '', auto_convert: true, converter_running: false, queue_size: 0 },
  modalContent: null,
  logLines: [],
})
