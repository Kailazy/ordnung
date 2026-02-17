// ── state ────────────────────────────────────────────────────────────────────

const AUDIO_FORMATS = ["mp3", "flac", "wav", "aiff", "ogg", "m4a", "alac", "wma", "aac"];

const state = {
  tab: "library",
  playlists: [],
  activePlaylist: null,
  tracks: [],
  search: "",
  genreFilter: null,
  formatUndo: null,
  expandedSong: null,
  songPlaylistsCache: {},
  downloads: [],
  watchConfig: { path: "", output_folder: "", converter_running: false, queue_size: 0 },
};

// ── tab switching ────────────────────────────────────────────────────────────

document.querySelectorAll(".nav-btn").forEach((btn) => {
  btn.addEventListener("click", () => {
    document.querySelectorAll(".nav-btn").forEach((b) => b.classList.remove("active"));
    btn.classList.add("active");
    const tab = btn.dataset.tab;
    document.querySelectorAll(".tab-panel").forEach((p) => p.classList.remove("active"));
    document.getElementById("tab-" + tab).classList.add("active");
    state.tab = tab;
    if (tab === "library") loadPlaylists();
    if (tab === "downloads") loadDownloads();
  });
});

// ── modal ────────────────────────────────────────────────────────────────────

function showModal(html) {
  document.getElementById("modal").innerHTML = html;
  document.getElementById("modal-overlay").classList.add("visible");
}

function hideModal() {
  document.getElementById("modal-overlay").classList.remove("visible");
}

document.getElementById("modal-overlay").addEventListener("click", (e) => {
  if (e.target === e.currentTarget) hideModal();
});

// ── helpers ──────────────────────────────────────────────────────────────────

function stars(n) {
  if (!n) return "";
  return "\u2605".repeat(parseInt(n));
}

function esc(s) {
  const d = document.createElement("div");
  d.textContent = s;
  return d.innerHTML;
}

function truncate(s, n) {
  if (!s) return "";
  return s.length > n ? s.slice(0, n) + "..." : s;
}

function formatBadgeClass(fmt) {
  const classes = {
    mp3: "badge-mp3", flac: "badge-flac", wav: "badge-wav",
    aiff: "badge-aiff", aif: "badge-aiff", ogg: "badge-ogg",
    m4a: "badge-m4a", alac: "badge-alac", wma: "badge-wma",
    aac: "badge-aac", opus: "badge-ogg",
  };
  return classes[fmt] || "badge-other";
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║  LIBRARY TAB                                                            ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

async function loadPlaylists() {
  const res = await fetch("/api/playlists");
  state.playlists = await res.json();
  renderLibrary();
}

async function loadTracks(playlistId) {
  state.activePlaylist = playlistId;
  state.search = "";
  state.genreFilter = null;
  state.expandedSong = null;
  const res = await fetch(`/api/playlists/${playlistId}/tracks`);
  state.tracks = await res.json();
  renderLibrary();
}

async function importPlaylist(file, name) {
  const form = new FormData();
  form.append("file", file);
  if (name) form.append("name", name);
  const res = await fetch("/api/playlists/import", { method: "POST", body: form });
  const data = await res.json();
  if (data.error) {
    alert(data.error);
    return null;
  }
  return data;
}

async function importFiles(files) {
  const fileList = Array.from(files).filter((f) => f.name.endsWith(".txt"));
  if (fileList.length === 0) return;

  let lastId = null;
  for (const file of fileList) {
    const data = await importPlaylist(file);
    if (data) lastId = data.id;
  }
  await loadPlaylists();
  if (lastId) await loadTracks(lastId);
}

async function deletePlaylist(id) {
  if (!confirm("Delete this playlist and all its tracks?")) return;
  await fetch(`/api/playlists/${id}`, { method: "DELETE" });
  if (state.activePlaylist === id) {
    state.activePlaylist = null;
    state.tracks = [];
  }
  await loadPlaylists();
}

async function setSongFormat(songId, fmt) {
  await fetch(`/api/songs/${songId}/format`, {
    method: "PUT",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ format: fmt }),
  });
  state.tracks.forEach((t) => { if (t.song_id === songId) t.format = fmt; });
  renderTrackList();
  renderPlaylistSidebar();
}

async function toggleSongAiff(songId) {
  const t = state.tracks.find((t) => t.song_id === songId);
  if (!t) return;
  const newVal = !t.has_aiff;
  await fetch(`/api/songs/${songId}/has-aiff`, {
    method: "PUT",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ has_aiff: newVal }),
  });
  state.tracks.forEach((t) => { if (t.song_id === songId) t.has_aiff = newVal ? 1 : 0; });
  renderTrackList();
}

async function bulkFormat(fmt, ids) {
  const affected = ids
    ? state.tracks.filter((t) => ids.includes(t.song_id))
    : state.tracks;
  state.formatUndo = affected.map((t) => ({ id: t.song_id, format: t.format }));

  const body = { format: fmt };
  if (ids) {
    body.ids = ids;
  } else {
    body.playlist_id = state.activePlaylist;
  }
  await fetch("/api/songs/bulk-format", {
    method: "PUT",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body),
  });
  if (ids) {
    state.tracks.forEach((t) => { if (ids.includes(t.song_id)) t.format = fmt; });
  } else {
    state.tracks.forEach((t) => (t.format = fmt));
  }
  renderTrackMain();
  renderPlaylistSidebar();
  loadPlaylists();
}

async function undoBulkFormat() {
  if (!state.formatUndo) return;
  const snapshot = state.formatUndo;
  state.formatUndo = null;

  const byFormat = {};
  for (const s of snapshot) {
    if (!byFormat[s.format]) byFormat[s.format] = [];
    byFormat[s.format].push(s.id);
  }

  const requests = [];
  for (const [fmt, ids] of Object.entries(byFormat)) {
    requests.push(fetch("/api/songs/bulk-format", {
      method: "PUT",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ format: fmt, ids }),
    }));
  }
  await Promise.all(requests);

  const lookup = Object.fromEntries(snapshot.map((s) => [s.id, s.format]));
  state.tracks.forEach((t) => { if (lookup[t.song_id] !== undefined) t.format = lookup[t.song_id]; });

  renderTrackMain();
  renderPlaylistSidebar();
  loadPlaylists();
}

async function showExport() {
  if (!state.activePlaylist) return;
  const formats = [...new Set(state.tracks.map((t) => t.format))].sort();
  const formatOptions = formats.map((f) => `<option value="${f}">${f}</option>`).join("");

  showModal(`
    <h2>export track list</h2>
    <div style="margin-bottom:12px;display:flex;align-items:center;gap:10px;">
      <label style="color:var(--text3);font-size:14px;">filter by format:</label>
      <select id="export-format" class="format-select" onchange="updateExportPreview()">
        <option value="">all formats</option>
        ${formatOptions}
      </select>
    </div>
    <pre id="export-pre"></pre>
    <div class="btn-row">
      <button class="btn" onclick="copyExport()">copy</button>
      <button class="btn" onclick="downloadExport()">download .txt</button>
      <button class="btn" onclick="hideModal()">close</button>
    </div>
  `);
  updateExportPreview();
}

async function updateExportPreview() {
  const fmt = document.getElementById("export-format").value;
  const url = `/api/playlists/${state.activePlaylist}/export` + (fmt ? `?format=${fmt}` : "");
  const res = await fetch(url);
  const data = await res.json();
  const content = data.count === 0 ? "no tracks match" : data.tracks.join("\n");
  document.getElementById("export-pre").textContent = content;
}

function copyExport() {
  const text = document.getElementById("export-pre").textContent;
  navigator.clipboard.writeText(text);
}

function downloadExport() {
  const text = document.getElementById("export-pre").textContent;
  const fmt = document.getElementById("export-format").value || "all";
  const blob = new Blob([text], { type: "text/plain" });
  const a = document.createElement("a");
  a.href = URL.createObjectURL(blob);
  a.download = `${fmt}_tracks.txt`;
  a.click();
}

// ── expandable track detail ──────────────────────────────────────────────────

function toggleDetail(songId) {
  if (state.expandedSong === songId) {
    state.expandedSong = null;
  } else {
    state.expandedSong = songId;
  }
  renderTrackList();
  if (state.expandedSong && !state.songPlaylistsCache[songId]) {
    loadSongPlaylists(songId);
  }
}

async function loadSongPlaylists(songId) {
  const res = await fetch(`/api/songs/${songId}/playlists`);
  const playlists = await res.json();
  state.songPlaylistsCache[songId] = playlists;
  renderPlaylistChecklist(songId);
}

function renderPlaylistChecklistHtml(songId) {
  const playlists = state.songPlaylistsCache[songId];
  if (!playlists) return '<span class="detail-loading">loading...</span>';
  return playlists
    .map((p) => {
      const checked = p.member ? "checked" : "";
      const current = p.id === state.activePlaylist ? " current-pl" : "";
      return `
        <label class="pl-check${current}">
          <input type="checkbox" ${checked}
                 onchange="toggleSongPlaylist(${songId}, ${p.id}, this.checked)">
          <span>${esc(p.name)}</span>
        </label>`;
    })
    .join("");
}

function renderPlaylistChecklist(songId) {
  const container = document.getElementById(`detail-playlists-${songId}`);
  if (!container) return;
  container.innerHTML = renderPlaylistChecklistHtml(songId);
}

async function toggleSongPlaylist(songId, playlistId, add) {
  const body = add ? { add: [playlistId] } : { remove: [playlistId] };
  await fetch(`/api/songs/${songId}/playlists`, {
    method: "PUT",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body),
  });
  const cached = state.songPlaylistsCache[songId];
  if (cached) {
    const entry = cached.find((p) => p.id === playlistId);
    if (entry) entry.member = add;
  }
  loadPlaylists();
}

function renderDetailPanel(t) {
  const fmt = t.format || "mp3";
  const badges = [];
  badges.push(`<span class="status-badge ${formatBadgeClass(fmt)}">${esc(fmt)}</span>`);
  if (t.has_aiff) badges.push('<span class="status-badge badge-aiff">aiff</span>');

  return `
    <div class="track-detail" id="detail-${t.song_id}">
      <div class="detail-section">
        <div class="detail-grid">
          ${t.album ? `<div class="detail-kv"><span class="detail-k">album</span><span class="detail-v">${esc(t.album)}</span></div>` : ""}
          ${t.genre ? `<div class="detail-kv"><span class="detail-k">genre</span><span class="detail-v">${esc(t.genre)}</span></div>` : ""}
          ${t.bpm ? `<div class="detail-kv"><span class="detail-k">bpm</span><span class="detail-v">${parseFloat(t.bpm).toFixed(0)}</span></div>` : ""}
          ${t.key_sig ? `<div class="detail-kv"><span class="detail-k">key</span><span class="detail-v">${esc(t.key_sig)}</span></div>` : ""}
          ${t.time ? `<div class="detail-kv"><span class="detail-k">time</span><span class="detail-v">${esc(t.time)}</span></div>` : ""}
          ${t.rating ? `<div class="detail-kv"><span class="detail-k">rating</span><span class="detail-v">${stars(t.rating)}</span></div>` : ""}
          ${t.date_added ? `<div class="detail-kv"><span class="detail-k">added</span><span class="detail-v">${esc(t.date_added)}</span></div>` : ""}
        </div>
      </div>
      <div class="detail-section">
        <div class="detail-badges">
          <span class="detail-k">status</span>
          ${badges.join("")}
          <button class="btn btn-sm ${t.has_aiff ? "badge-aiff-active" : ""}" onclick="event.stopPropagation();toggleSongAiff(${t.song_id})">
            ${t.has_aiff ? "has aiff" : "no aiff"}
          </button>
        </div>
      </div>
      <div class="detail-section">
        <span class="detail-k">playlists</span>
        <div class="detail-playlists" id="detail-playlists-${t.song_id}">
          ${renderPlaylistChecklistHtml(t.song_id)}
        </div>
      </div>
    </div>`;
}

// ── library rendering ────────────────────────────────────────────────────────

function formatCountsHtml(counts) {
  if (!counts || Object.keys(counts).length === 0) return "";
  return Object.entries(counts)
    .sort((a, b) => b[1] - a[1])
    .map(([fmt, count]) => `<span class="fmt-tag fmt-${fmt}">${count} ${fmt}</span>`)
    .join(" &middot; ");
}

function formatStatsHtml() {
  const counts = {};
  state.tracks.forEach((t) => {
    const fmt = t.format || "mp3";
    counts[fmt] = (counts[fmt] || 0) + 1;
  });
  const parts = [`<span>${state.tracks.length} tracks</span>`];
  for (const [fmt, count] of Object.entries(counts).sort((a, b) => b[1] - a[1])) {
    parts.push(`<span class="fmt-tag fmt-${fmt}">${count} ${fmt}</span>`);
  }
  return parts.join("");
}

function renderLibrary() {
  const panel = document.getElementById("tab-library");
  panel.innerHTML = `
    <div class="ft-layout">
      <div class="ft-sidebar">
        <div class="ft-sidebar-head">
          <h2>playlists</h2>
          <div class="upload-area" id="upload-area">
            <input type="file" id="file-input" accept=".txt" multiple>
            drop or click to import .txt
          </div>
        </div>
        <div class="ft-playlists" id="playlist-list"></div>
      </div>
      <div class="ft-main" id="ft-main"></div>
    </div>
  `;

  const uploadArea = document.getElementById("upload-area");
  const fileInput = document.getElementById("file-input");

  uploadArea.addEventListener("click", () => fileInput.click());
  uploadArea.addEventListener("dragover", (e) => { e.preventDefault(); uploadArea.style.borderColor = "#444"; });
  uploadArea.addEventListener("dragleave", () => { uploadArea.style.borderColor = ""; });
  uploadArea.addEventListener("drop", (e) => {
    e.preventDefault();
    uploadArea.style.borderColor = "";
    if (e.dataTransfer.files.length) importFiles(e.dataTransfer.files);
  });
  fileInput.addEventListener("change", () => {
    if (fileInput.files.length) importFiles(fileInput.files);
  });

  renderPlaylistSidebar();

  if (state.activePlaylist && state.tracks.length) {
    renderTrackMain();
  } else if (state.activePlaylist) {
    loadTracks(state.activePlaylist);
  } else {
    document.getElementById("ft-main").innerHTML =
      '<div class="empty">select or import a playlist<div class="hint">import a rekordbox .txt export to get started</div></div>';
  }
}

function renderPlaylistSidebar() {
  const list = document.getElementById("playlist-list");
  if (!list) return;

  if (state.playlists.length === 0) {
    list.innerHTML = '<div class="empty" style="padding:20px;font-size:11px;">no playlists yet</div>';
    return;
  }

  list.innerHTML = state.playlists
    .map((p) => {
      const active = p.id === state.activePlaylist ? "active" : "";
      const fmtHtml = formatCountsHtml(p.format_counts);
      return `
        <div class="ft-playlist-item ${active}" data-id="${p.id}">
          <div style="display:flex;align-items:center;gap:6px;">
            <span class="ft-playlist-name">${esc(p.name)}</span>
            <button class="ft-playlist-delete" onclick="event.stopPropagation();deletePlaylist(${p.id})" title="delete">&times;</button>
          </div>
          <div class="ft-playlist-meta">
            ${p.total} tracks${fmtHtml ? " &middot; " + fmtHtml : ""}
          </div>
        </div>`;
    })
    .join("");

  list.querySelectorAll(".ft-playlist-item").forEach((el) => {
    el.addEventListener("click", () => loadTracks(parseInt(el.dataset.id)));
  });
}

function renderTrackMain() {
  const main = document.getElementById("ft-main");
  if (!main) return;

  main.innerHTML = `
    <div class="ft-toolbar">
      <input type="text" class="ft-search" id="ft-search" placeholder="search tracks, artists, albums, genres..."
             value="${esc(state.search)}" autocomplete="off">
      <div class="ft-stats" id="ft-stats">${formatStatsHtml()}</div>
      <div class="bulk-controls">
        <select class="format-select" id="bulk-format-sel">
          ${AUDIO_FORMATS.map((f) => `<option value="${f}">${f}</option>`).join("")}
        </select>
        <button class="btn" onclick="bulkFormat(document.getElementById('bulk-format-sel').value)">set all</button>
        <button class="btn" onclick="bulkVisibleFormat()">set visible</button>
      </div>
      ${state.formatUndo ? '<button class="btn btn-undo" onclick="undoBulkFormat()">undo</button>' : ""}
      <button class="btn btn-accent" onclick="showExport()">export</button>
    </div>
    <div class="ft-genres" id="ft-genres"></div>
    <div class="ft-tracks" id="ft-tracks"></div>
  `;

  document.getElementById("ft-search").addEventListener("input", (e) => {
    state.search = e.target.value;
    renderTrackList();
  });

  renderGenreBar();
  renderTrackList();
}

function getTopGenres() {
  const freq = {};
  state.tracks.forEach((t) => {
    if (t.genre) {
      t.genre.split(/[;,\/]/).forEach((g) => {
        const trimmed = g.trim().toLowerCase();
        if (trimmed && trimmed.length > 1) freq[trimmed] = (freq[trimmed] || 0) + 1;
      });
    }
  });
  return Object.entries(freq)
    .sort((a, b) => b[1] - a[1])
    .slice(0, 15)
    .map((e) => e[0]);
}

function renderGenreBar() {
  const bar = document.getElementById("ft-genres");
  if (!bar) return;
  const genres = getTopGenres();
  bar.innerHTML = genres
    .map(
      (g) =>
        `<button class="genre-tag ${state.genreFilter === g ? "active" : ""}"
                 onclick="toggleGenre('${g.replace(/'/g, "\\'")}')">${g}</button>`
    )
    .join("");
}

function toggleGenre(g) {
  state.genreFilter = state.genreFilter === g ? null : g;
  renderGenreBar();
  renderTrackList();
}

function getFilteredTracks() {
  return state.tracks.filter((t) => {
    if (state.search) {
      const q = state.search.toLowerCase();
      const hay = `${t.title} ${t.artist} ${t.album} ${t.genre} ${t.key_sig}`.toLowerCase();
      if (!hay.includes(q)) return false;
    }
    if (state.genreFilter) {
      if (!t.genre) return false;
      const parts = t.genre.toLowerCase().split(/[;,\/]/).map((s) => s.trim());
      if (!parts.some((p) => p.includes(state.genreFilter))) return false;
    }
    return true;
  });
}

function renderTrackList() {
  const container = document.getElementById("ft-tracks");
  if (!container) return;

  const filtered = getFilteredTracks();

  const statsEl = document.getElementById("ft-stats");
  if (statsEl) {
    statsEl.innerHTML = formatStatsHtml();
  }

  if (filtered.length === 0) {
    container.innerHTML = '<div class="empty">no tracks match</div>';
    return;
  }

  container.innerHTML = filtered
    .map((t) => {
      const isExpanded = state.expandedSong === t.song_id;
      const chevron = isExpanded ? "&#9662;" : "&#9656;";
      const expandedClass = isExpanded ? " expanded" : "";

      const fmt = t.format || "mp3";
      const inlineBadges = [];
      if (fmt !== "mp3") inlineBadges.push(`<span class="status-badge ${formatBadgeClass(fmt)}">${esc(fmt)}</span>`);
      if (t.has_aiff) inlineBadges.push('<span class="status-badge badge-aiff">aiff \u2713</span>');

      const formatOptions = AUDIO_FORMATS.map((f) =>
        `<option value="${f}" ${fmt === f ? "selected" : ""}>${f}</option>`
      ).join("");

      return `
    <div class="track-row-wrap${expandedClass}">
      <div class="track-row" onclick="toggleDetail(${t.song_id})">
        <span class="track-chevron">${chevron}</span>
        <div class="track-info">
          <div class="track-main">
            <span class="track-title" title="${esc(t.title)}">${esc(t.title)}</span>
            ${t.artist ? `<span class="track-artist">&mdash; ${esc(t.artist)}</span>` : ""}
            ${inlineBadges.length ? `<span class="track-badges">${inlineBadges.join("")}</span>` : ""}
          </div>
          <div class="track-meta">
            ${t.bpm ? `<span class="bpm-tag">${parseFloat(t.bpm).toFixed(0)} bpm</span>` : ""}
            ${t.key_sig ? `<span class="key-tag">${esc(t.key_sig)}</span>` : ""}
            <span>${esc(t.time)}</span>
          </div>
        </div>
        <div class="format-toggle" onclick="event.stopPropagation()">
          <select class="format-select" onchange="setSongFormat(${t.song_id}, this.value)">
            ${formatOptions}
          </select>
        </div>
      </div>
      ${isExpanded ? renderDetailPanel(t) : ""}
    </div>`;
    })
    .join("");
}

function bulkVisibleFormat() {
  const filtered = getFilteredTracks();
  const ids = filtered.map((t) => t.song_id);
  const fmt = document.getElementById("bulk-format-sel").value;
  bulkFormat(fmt, ids);
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║  DOWNLOADS TAB                                                          ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

async function loadDownloads() {
  const [configRes, dlRes] = await Promise.all([
    fetch("/api/downloads/config"),
    fetch("/api/downloads"),
  ]);
  state.watchConfig = await configRes.json();
  state.downloads = await dlRes.json();
  renderDownloads();
}

async function saveDownloadsConfig() {
  const pathEl = document.getElementById("dl-path");
  const outputEl = document.getElementById("dl-output");
  if (!pathEl || !outputEl) return;

  const saveBtn = document.querySelector('[onclick="saveDownloadsConfig()"]');
  if (saveBtn) { saveBtn.disabled = true; saveBtn.textContent = "saving..."; saveBtn.classList.add("btn-busy"); }

  const path = pathEl.value.trim();
  const outputFolder = outputEl.value.trim();
  const res = await fetch("/api/downloads/config", {
    method: "PUT",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ path, output_folder: outputFolder, auto_convert: false }),
  });
  const data = await res.json();

  if (saveBtn) { saveBtn.disabled = false; saveBtn.textContent = "save"; saveBtn.classList.remove("btn-busy"); }

  if (data.error) {
    addLogLine("ERROR: " + data.error);
    alert(data.error);
    return;
  }
  state.watchConfig = {
    path: data.path || "",
    output_folder: data.output_folder || "",
    converter_running: data.converter_running,
    queue_size: state.watchConfig.queue_size,
  };
  addLogLine("Config saved");
  renderConvBadge();
}

async function scanFolder() {
  const btn = document.querySelector('[onclick="scanFolder()"]');
  if (btn) { btn.disabled = true; btn.textContent = "scanning..."; btn.classList.add("btn-busy"); }
  addLogLine("Saving config and starting scan...");
  await saveDownloadsConfig();
  const res = await fetch("/api/downloads/scan", { method: "POST" });
  const data = await res.json();
  if (btn) { btn.disabled = false; btn.textContent = "scan"; btn.classList.remove("btn-busy"); }
  if (data.error) {
    addLogLine("ERROR: " + data.error);
    alert(data.error);
    return;
  }
  addLogLine(`Scan complete: ${data.scanned} files found, ${data.added} new`);
  await loadDownloads();
  refreshLogPanel();
}

async function browseFolder(inputId) {
  const res = await fetch("/api/browse-folder", { method: "POST" });
  const data = await res.json();
  if (data.path) {
    document.getElementById(inputId).value = data.path;
    const label = document.getElementById(inputId + "-label");
    if (label) {
      label.textContent = data.path.length > 38 ? "..." + data.path.slice(-35) : data.path;
      label.closest(".folder-node").title = data.path;
    }
  }
}

async function deleteDownload(id) {
  await fetch(`/api/downloads/${id}`, { method: "DELETE" });
  state.downloads = state.downloads.filter((d) => d.id !== id);
  renderDownloadList();
}

async function convertSingle(downloadId) {
  const res = await fetch(`/api/conversions/convert/${downloadId}`, { method: "POST" });
  const data = await res.json();
  if (data.error) { alert(data.error); return; }
  const d = state.downloads.find((x) => x.id === downloadId);
  if (d) { d.conv_status = "pending"; d.conv_id = data.id; }
  renderDownloadList();
}

async function convertAll() {
  const unconverted = state.downloads.filter((d) => !d.conv_status || d.conv_status === "failed");
  if (unconverted.length === 0) { addLogLine("Nothing to convert"); return; }

  const btn = document.querySelector('[onclick="convertAll()"]');
  if (btn) { btn.disabled = true; btn.textContent = "queuing..."; btn.classList.add("btn-busy"); }
  addLogLine(`Queuing ${unconverted.length} files for conversion...`);

  for (const d of unconverted) {
    await fetch(`/api/conversions/convert/${d.id}`, { method: "POST" });
    d.conv_status = "pending";
  }
  renderDownloadList();

  if (btn) { btn.disabled = false; btn.textContent = "convert all"; btn.classList.remove("btn-busy"); }
  addLogLine("All files queued for conversion");
}

async function retryConversion(convId) {
  const res = await fetch(`/api/conversions/retry/${convId}`, { method: "POST" });
  const data = await res.json();
  if (data.error) { alert(data.error); return; }
  await loadDownloads();
}

function truncPath(p, max) {
  if (!p) return "click to set";
  return p.length > max ? "..." + p.slice(-(max - 3)) : p;
}

function extClass(ext) {
  const classes = {
    flac: "ext-flac", mp3: "ext-mp3",
    wav: "ext-wav", aiff: "ext-wav", aif: "ext-wav",
    ogg: "ext-ogg", m4a: "ext-m4a", alac: "ext-alac",
    wma: "ext-wma", aac: "ext-aac", opus: "ext-ogg",
  };
  return classes[ext] || "ext-other";
}

function renderDownloads() {
  const panel = document.getElementById("tab-downloads");
  const srcPath = state.watchConfig.path || "";
  const outPath = state.watchConfig.output_folder || "";

  panel.innerHTML = `
    <div class="dl-layout">
      <div class="dl-config-section">
        <div class="folder-flow">
          <div class="folder-node" onclick="browseFolder('dl-path')" title="${esc(srcPath) || 'click to browse'}">
            <input type="hidden" id="dl-path" value="${esc(srcPath)}">
            <svg class="folder-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5">
              <path d="M3 7v10a2 2 0 002 2h14a2 2 0 002-2V9a2 2 0 00-2-2h-6l-2-2H5a2 2 0 00-2 2z"/>
            </svg>
            <span class="folder-node-label">source</span>
            <span class="folder-node-path" id="dl-path-label">${esc(truncPath(srcPath, 38))}</span>
          </div>
          <div class="folder-arrow">
            <svg viewBox="0 0 48 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
              <line x1="2" y1="12" x2="38" y2="12"/>
              <polyline points="32,6 38,12 32,18"/>
            </svg>
          </div>
          <div class="folder-node folder-node-out" onclick="browseFolder('dl-output')" title="${esc(outPath) || 'click to browse'}">
            <input type="hidden" id="dl-output" value="${esc(outPath)}">
            <svg class="folder-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5">
              <path d="M3 7v10a2 2 0 002 2h14a2 2 0 002-2V9a2 2 0 00-2-2h-6l-2-2H5a2 2 0 00-2 2z"/>
            </svg>
            <span class="folder-node-label">aiff output</span>
            <span class="folder-node-path" id="dl-output-label">${esc(truncPath(outPath, 38))}</span>
          </div>
        </div>
        <div class="dl-config dl-actions">
          <button class="btn" onclick="saveDownloadsConfig()">save</button>
          <button class="btn" onclick="scanFolder()">scan</button>
          <button class="btn btn-accent" onclick="convertAll()">convert all</button>
          <span id="dl-conv-badge"></span>
        </div>
      </div>
      <div class="dl-table" id="dl-table"></div>
      <div class="dl-log-section">
        <div class="dl-log-header">
          <span class="dl-log-title">activity log</span>
          <span class="dl-log-dot" id="dl-log-dot"></span>
        </div>
        <div class="dl-log" id="dl-log"></div>
      </div>
    </div>
  `;
  renderConvBadge();
  renderDownloadList();
  refreshLogPanel();
}

function renderConvBadge() {
  const badge = document.getElementById("dl-conv-badge");
  if (!badge) return;
  if (state.watchConfig.converter_running) {
    const q = state.watchConfig.queue_size;
    badge.className = "dl-status active";
    badge.textContent = q > 0 ? `converting (${q} queued)` : "converter ready";
  } else {
    badge.className = "";
    badge.textContent = "";
  }
}

function convStatusHtml(d) {
  const s = d.conv_status;
  if (s === "done") return `<span class="conv-badge conv-done">converted</span>`;
  if (s === "converting") return `<span class="conv-badge conv-active">converting...</span>`;
  if (s === "pending") return `<span class="conv-badge conv-pending">queued</span>`;
  if (s === "failed") return `<span class="conv-badge conv-failed" title="${esc(d.conv_error || "")}">failed</span> <button class="btn btn-sm" onclick="retryConversion(${d.conv_id})">retry</button>`;
  return `<button class="btn btn-sm" onclick="convertSingle(${d.id})">convert</button>`;
}

function renderDownloadList() {
  const table = document.getElementById("dl-table");
  if (!table) return;

  if (state.downloads.length === 0) {
    table.innerHTML = `
      <div class="empty">
        no files detected yet
        <div class="hint">set a source folder and output folder, then click scan</div>
      </div>`;
    return;
  }

  table.innerHTML = state.downloads
    .map(
      (d) => `
    <div class="dl-row">
      <span class="fname" title="${esc(d.filepath)}">${esc(d.filename)}</span>
      <span class="ext ${extClass(d.extension)}">${esc(d.extension)}</span>
      <span class="size">${d.size_mb} MB</span>
      <span class="conv-cell">${convStatusHtml(d)}</span>
      <button class="del-btn" onclick="deleteDownload(${d.id})" title="remove">&times;</button>
    </div>`
    )
    .join("");
}

// ── activity log ─────────────────────────────────────────────────────────────

let _lastLogCount = 0;

function addLogLine(msg) {
  const el = document.getElementById("dl-log");
  if (!el) return;
  const time = new Date().toLocaleTimeString();
  const line = document.createElement("div");
  line.className = "dl-log-line" + (msg.startsWith("ERROR") ? " log-error" : "");
  line.textContent = `${time}  ${msg}`;
  el.appendChild(line);
  el.scrollTop = el.scrollHeight;
  flashLogDot();
}

function flashLogDot() {
  const dot = document.getElementById("dl-log-dot");
  if (!dot) return;
  dot.classList.add("pulse");
  setTimeout(() => dot.classList.remove("pulse"), 1200);
}

async function refreshLogPanel() {
  try {
    const res = await fetch("/api/logs/recent?n=80");
    const data = await res.json();
    const el = document.getElementById("dl-log");
    if (!el) return;

    if (data.lines.length !== _lastLogCount) {
      _lastLogCount = data.lines.length;
      el.innerHTML = data.lines
        .map((l) => {
          const cls = l.includes("[ERROR]") ? "dl-log-line log-error" :
                      l.includes("[WARNING]") ? "dl-log-line log-warn" :
                      "dl-log-line";
          return `<div class="${cls}">${esc(l)}</div>`;
        })
        .join("");
      el.scrollTop = el.scrollHeight;
      flashLogDot();
    }
  } catch (e) {}
}

// ── auto-refresh while on downloads tab ──────────────────────────────────────

setInterval(() => {
  if (state.tab === "downloads") {
    Promise.all([
      fetch("/api/downloads/config").then((r) => r.json()),
      fetch("/api/downloads").then((r) => r.json()),
    ]).then(([config, downloads]) => {
      state.watchConfig = config;

      const changed =
        downloads.length !== state.downloads.length ||
        downloads.some((d, i) => d.conv_status !== (state.downloads[i] && state.downloads[i].conv_status));

      if (changed) {
        state.downloads = downloads;
        renderDownloadList();
      }
      renderConvBadge();
    });
    refreshLogPanel();
  }
}, 3000);

// ── init ─────────────────────────────────────────────────────────────────────

loadPlaylists();
