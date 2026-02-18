import { invoke } from '@tauri-apps/api/core'

export const listPlaylists = () => invoke('list_playlists')
export const importPlaylist = (filePath, name) => invoke('import_playlist', { filePath, name })
export const deletePlaylist = (playlistId) => invoke('delete_playlist', { playlistId })
export const getTracks = (playlistId) => invoke('get_tracks', { playlistId })
export const exportTracks = (playlistId, format) => invoke('export_tracks', { playlistId, format })

export const updateSongFormat = (songId, format) => invoke('update_song_format', { songId, format })
export const updateSongAiff = (songId, hasAiff) => invoke('update_song_aiff', { songId, hasAiff })
export const bulkUpdateFormat = (format, ids, playlistId) => invoke('bulk_update_format', { format, ids, playlistId })
export const getSongPlaylists = (songId) => invoke('get_song_playlists', { songId })
export const updateSongPlaylists = (songId, add, remove) => invoke('update_song_playlists', { songId, add, remove })

export const getWatchConfig = () => invoke('get_watch_config')
export const setWatchConfig = (path, outputFolder, autoConvert) => invoke('set_watch_config', { path, outputFolder, autoConvert })
export const listDownloads = () => invoke('list_downloads')
export const deleteDownload = (downloadId) => invoke('delete_download', { downloadId })
export const scanFolder = () => invoke('scan_folder')
export const browseFolder = () => invoke('browse_folder')

export const listConversions = () => invoke('list_conversions')
export const conversionStats = () => invoke('conversion_stats')
export const retryConversion = (conversionId) => invoke('retry_conversion', { conversionId })
export const convertSingle = (downloadId) => invoke('convert_single', { downloadId })
