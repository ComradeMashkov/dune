#include "web_assets.hpp"

namespace dune::notebook {

std::string_view notebook_app_html() {
    return R"dune_notebook(<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>Dune Notebook</title>
  <style>
    :root {
      color-scheme: light;
      --page: #ffffff;
      --chrome: #ffffff;
      --surface: #ffffff;
      --surface-muted: #f7f7f7;
      --surface-hover: #f1f3f5;
      --border: #d5d8dc;
      --border-strong: #aeb4ba;
      --text: #24292f;
      --muted: #66707a;
      --faint: #8c959f;
      --accent: #2f7dbd;
      --accent-soft: #e8f2fb;
      --edit: #2d8a46;
      --danger: #c43c35;
      --danger-soft: #fff1f0;
      --orange: #f37726;
      --code: #f7f7f7;
      --code-text: #1f2328;
      --syntax-keyword: #8250df;
      --syntax-type: #953800;
      --syntax-literal: #0550ae;
      --syntax-number: #0550ae;
      --syntax-string: #0a7f3f;
      --syntax-comment: #6e7781;
      --syntax-function: #6639ba;
      --syntax-declaration: #8250df;
      --syntax-namespace: #0969da;
      --syntax-constant: #0550ae;
      --syntax-operator: #cf222e;
      --syntax-invalid: #cf222e;
      --code-selection: rgba(47, 125, 189, .28);
      --output: #ffffff;
      --shadow: 0 4px 18px rgba(31, 35, 40, .12);
      --header-height: 132px;
    }
    :root[data-theme="dark"] {
      color-scheme: dark;
      --page: #1e1f22;
      --chrome: #25262a;
      --surface: #282a2e;
      --surface-muted: #202226;
      --surface-hover: #34363b;
      --border: #41444a;
      --border-strong: #62666e;
      --text: #e6e8eb;
      --muted: #a6abb3;
      --faint: #7f858e;
      --accent: #67a9df;
      --accent-soft: #20384c;
      --edit: #59b875;
      --danger: #ef7770;
      --danger-soft: #442a2a;
      --orange: #ff8b42;
      --code: #202226;
      --code-text: #e6e8eb;
      --syntax-keyword: #c792ea;
      --syntax-type: #ffcb6b;
      --syntax-literal: #f78c6c;
      --syntax-number: #f78c6c;
      --syntax-string: #c3e88d;
      --syntax-comment: #7f848e;
      --syntax-function: #82aaff;
      --syntax-declaration: #c792ea;
      --syntax-namespace: #89ddff;
      --syntax-constant: #f78c6c;
      --syntax-operator: #89ddff;
      --syntax-invalid: #ff5370;
      --code-selection: rgba(103, 169, 223, .32);
      --output: #25272b;
      --shadow: 0 7px 24px rgba(0, 0, 0, .34);
    }
    @keyframes cell-ring-travel { to { stroke-dashoffset: -100; } }
    * { box-sizing: border-box; }
    [hidden] { display: none !important; }
    html { min-height: 100%; background: var(--page); }
    body {
      min-height: 100vh;
      margin: 0;
      background: var(--page);
      color: var(--text);
      font: 13px/1.45 -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
    }
    button, input, select, textarea { font: inherit; }
    button, select {
      min-height: 30px;
      border: 1px solid var(--border);
      border-radius: 3px;
      background: var(--surface);
      color: var(--text);
    }
    button { padding: 4px 10px; cursor: pointer; }
    button:hover, select:hover { border-color: var(--border-strong); background: var(--surface-hover); }
    button:focus-visible, select:focus-visible, input:focus-visible, textarea:focus-visible {
      outline: 2px solid var(--accent);
      outline-offset: 1px;
    }
    button.primary { border-color: var(--accent); background: var(--accent); color: #fff; }
    button.primary:hover { filter: brightness(.94); background: var(--accent); }
    button.danger { color: var(--danger); }
    .notebook-header {
      position: sticky;
      z-index: 20;
      top: 0;
      border-bottom: 1px solid var(--border);
      background: var(--chrome);
      box-shadow: 0 1px 2px rgba(0, 0, 0, .05);
    }
    .topbar, .menubar, .toolbar {
      display: flex;
      align-items: center;
      width: min(1180px, 100%);
      margin: 0 auto;
      padding-inline: 18px;
    }
    .topbar { min-height: 52px; gap: 10px; }
    .brand {
      display: inline-flex;
      align-items: center;
      gap: 8px;
      margin-right: 2px;
      white-space: nowrap;
      font-size: 16px;
      font-weight: 600;
    }
    .brand-mark {
      display: grid;
      width: 25px;
      height: 25px;
      place-items: center;
      border: 2px solid var(--orange);
      border-radius: 50%;
      color: var(--orange);
      font-size: 12px;
      font-weight: 750;
    }
    #files-toggle { padding-inline: 9px; }
    #title {
      min-width: 180px;
      flex: 1;
      border: 1px solid transparent;
      border-radius: 3px;
      background: transparent;
      color: var(--text);
      padding: 5px 7px;
      font-size: 16px;
      font-weight: 500;
    }
    #title:hover, #title:focus { border-color: var(--border); background: var(--surface-muted); outline: none; }
    .status { color: var(--muted); white-space: nowrap; }
    .status.error { color: var(--danger); }
    .kernel-pill {
      display: inline-flex;
      align-items: center;
      gap: 6px;
      border: 1px solid var(--border);
      border-radius: 999px;
      padding: 4px 9px;
      color: var(--muted);
      white-space: nowrap;
    }
    .kernel-dot { width: 7px; height: 7px; border-radius: 50%; background: var(--edit); }
    .kernel-pill.busy .kernel-dot { background: var(--orange); animation: pulse 1s infinite alternate; }
    @keyframes pulse { to { opacity: .35; } }
    .menubar { min-height: 34px; gap: 1px; border-top: 1px solid var(--border); }
    details.menu { position: relative; }
    details.menu summary {
      list-style: none;
      border-radius: 3px;
      padding: 5px 9px;
      cursor: pointer;
      user-select: none;
    }
    details.menu summary::-webkit-details-marker { display: none; }
    details.menu summary:hover, details.menu[open] summary { background: var(--surface-hover); }
    .menu-content {
      position: absolute;
      z-index: 40;
      top: calc(100% + 2px);
      left: 0;
      min-width: 210px;
      border: 1px solid var(--border);
      border-radius: 4px;
      background: var(--surface);
      box-shadow: var(--shadow);
      padding: 5px;
    }
    .menu-content button {
      display: flex;
      width: 100%;
      align-items: center;
      justify-content: space-between;
      border: 0;
      background: transparent;
      padding: 6px 9px;
      text-align: left;
    }
    .menu-content button:hover { background: var(--surface-hover); }
    .shortcut { margin-left: 24px; color: var(--faint); font-size: 11px; }
    .menu-separator { height: 1px; margin: 4px 6px; background: var(--border); }
    .toolbar {
      min-height: 45px;
      gap: 6px;
      border-top: 1px solid var(--border);
      overflow-x: auto;
    }
    .toolbar button { white-space: nowrap; }
    .toolbar-separator { width: 1px; height: 24px; margin-inline: 2px; background: var(--border); }
    #cell-type { min-width: 125px; padding: 4px 28px 4px 8px; }
    .selection-label { margin-left: auto; color: var(--faint); white-space: nowrap; }
    .file-panel {
      position: fixed;
      z-index: 50;
      top: 48px;
      left: max(14px, calc((100vw - 1180px) / 2 + 18px));
      width: min(390px, calc(100vw - 28px));
      max-height: min(560px, calc(100vh - 72px));
      overflow: auto;
      border: 1px solid var(--border);
      border-radius: 5px;
      background: var(--surface);
      box-shadow: var(--shadow);
    }
    .file-panel-header {
      position: sticky;
      top: 0;
      display: flex;
      align-items: center;
      gap: 8px;
      border-bottom: 1px solid var(--border);
      background: var(--surface);
      padding: 10px 12px;
    }
    .file-panel-header strong { flex: 1; }
    .files { padding: 6px; }
    .file {
      display: flex;
      width: 100%;
      align-items: center;
      gap: 9px;
      border: 0;
      background: transparent;
      padding: 7px 9px;
      text-align: left;
    }
    .file::before { content: "□"; color: var(--faint); font-size: 15px; }
    .file.active { background: var(--accent-soft); color: var(--accent); font-weight: 600; }
    .notebook-main { width: min(1180px, 100%); margin: 0 auto; padding: 18px 18px 100px; }
    .notebook-context {
      display: flex;
      align-items: center;
      gap: 8px;
      min-height: 31px;
      margin-bottom: 13px;
      color: var(--muted);
    }
    .context-path { overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
    .context-spacer { flex: 1; }
    .workspace { max-width: 1080px; margin: 0 auto; }
    .welcome, .empty-notebook {
      border: 1px dashed var(--border-strong);
      border-radius: 4px;
      background: var(--surface-muted);
      padding: 44px 24px;
      text-align: center;
      color: var(--muted);
    }
    .cell {
      position: relative;
      display: grid;
      grid-template-columns: 88px minmax(0, 1fr);
      gap: 0;
      margin: 7px 0;
      padding: 6px;
      border: 0;
      border-radius: 4px;
    }
    .cell.selected {
      --cell-ring-color: var(--accent);
    }
    .cell.selected.edit-mode { --cell-ring-color: var(--edit); }
    .cell-selection-ring {
      position: absolute;
      z-index: 1;
      inset: 0;
      display: none;
      width: 100%;
      height: 100%;
      overflow: visible;
      pointer-events: none;
    }
    .cell.selected > .cell-selection-ring { display: block; }
    .cell-selection-ring rect {
      x: 1px;
      y: 1px;
      width: calc(100% - 2px);
      height: calc(100% - 2px);
      rx: 3px;
      fill: none;
      vector-effect: non-scaling-stroke;
    }
    .cell-selection-track { stroke: var(--border-strong); stroke-width: 1; }
    .cell-selection-progress {
      stroke: var(--cell-ring-color);
      stroke-width: 2;
      stroke-linecap: round;
      stroke-dasharray: 18 82;
      animation: cell-ring-travel 6s linear infinite;
    }
    .prompt {
      padding: 7px 10px 0 5px;
      color: var(--accent);
      text-align: right;
      white-space: nowrap;
      font: 12px/1.5 "SFMono-Regular", Consolas, "Liberation Mono", monospace;
      user-select: none;
    }
    .cell.selected.edit-mode .prompt { color: var(--edit); }
    .cell-content { min-width: 0; }
    textarea {
      display: block;
      width: 100%;
      resize: vertical;
      outline: none;
    }
    .code-editor {
      position: relative;
      min-height: 56px;
      border: 1px solid var(--border);
      border-radius: 2px;
      background: var(--code);
    }
    .code-editor:focus-within { border-color: var(--border-strong); }
    .code-source, .code-highlight {
      margin: 0;
      border: 0;
      padding: 8px 10px;
      tab-size: 4;
      white-space: pre-wrap;
      overflow-wrap: break-word;
      font: 13px/1.55 "SFMono-Regular", Consolas, "Liberation Mono", monospace;
      letter-spacing: normal;
    }
    .code-highlight {
      position: absolute;
      z-index: 0;
      inset: 0;
      overflow: hidden;
      color: var(--code-text);
      pointer-events: none;
    }
    .code-source {
      position: relative;
      z-index: 1;
      min-height: 56px;
      overflow: hidden;
      resize: vertical;
      background: transparent;
      color: transparent;
      caret-color: var(--code-text);
      -webkit-text-fill-color: transparent;
    }
    .code-source:focus, .code-source:focus-visible {
      outline: 0;
      box-shadow: none;
    }
    .code-source::selection { background: var(--code-selection); }
    .syntax-keyword { color: var(--syntax-keyword); font-weight: 600; }
    .syntax-type { color: var(--syntax-type); }
    .syntax-literal { color: var(--syntax-literal); }
    .syntax-number { color: var(--syntax-number); }
    .syntax-string { color: var(--syntax-string); }
    .syntax-comment { color: var(--syntax-comment); font-style: italic; }
    .syntax-function { color: var(--syntax-function); }
    .syntax-declaration { color: var(--syntax-declaration); font-weight: 600; }
    .syntax-namespace { color: var(--syntax-namespace); }
    .syntax-constant { color: var(--syntax-constant); }
    .syntax-operator { color: var(--syntax-operator); }
    .syntax-invalid {
      color: var(--syntax-invalid);
      text-decoration: underline wavy var(--syntax-invalid);
      text-underline-offset: 2px;
    }
    .markdown-source {
      min-height: 90px;
      overflow: hidden;
      border: 1px solid var(--border);
      border-radius: 2px;
      background: var(--surface);
      color: var(--text);
      padding: 10px 12px;
      font: 13px/1.55 "SFMono-Regular", Consolas, "Liberation Mono", monospace;
    }
    .markdown-source:focus, .markdown-source:focus-visible {
      border-color: var(--border-strong);
      outline: 0;
      box-shadow: none;
    }
    .markdown-preview { min-height: 38px; padding: 4px 12px 8px; font-size: 14px; }
    .markdown-preview h1, .markdown-preview h2, .markdown-preview h3 {
      margin: .65em 0 .35em;
      border-bottom: 1px solid var(--border);
      padding-bottom: .18em;
      line-height: 1.22;
    }
    .markdown-preview h1 { font-size: 1.85em; }
    .markdown-preview h2 { font-size: 1.45em; }
    .markdown-preview h3 { border-bottom: 0; font-size: 1.2em; }
    .markdown-preview p { margin: .5em 0; }
    .markdown-preview code { border-radius: 2px; background: var(--surface-muted); padding: .1em .3em; }
    .markdown-preview pre {
      overflow: auto;
      border: 1px solid var(--border);
      border-radius: 2px;
      background: var(--code);
      color: var(--code-text);
      padding: 10px;
    }
    .output {
      margin: 5px 0 2px;
      border: 0;
      background: var(--output);
      color: var(--text);
      padding: 7px 10px;
      white-space: pre-wrap;
      overflow-wrap: anywhere;
      font: 13px/1.5 "SFMono-Regular", Consolas, "Liberation Mono", monospace;
    }
    .output.error {
      border-left: 3px solid var(--danger);
      background: var(--danger-soft);
      color: var(--danger);
    }
    .rich-output {
      margin: 8px 0 2px;
      overflow: auto;
      background: var(--surface);
      padding: 8px;
    }
    .rich-output img { display: block; max-width: 100%; height: auto; margin: 0 auto; }
    .cell-insert {
      position: absolute;
      z-index: 2;
      right: 10px;
      bottom: -14px;
      display: none;
      min-height: 24px;
      border-radius: 12px;
      padding: 1px 8px;
      color: var(--muted);
      font-size: 11px;
    }
    .cell:hover .cell-insert, .cell.selected .cell-insert { display: block; }
    .footer-status {
      position: fixed;
      z-index: 10;
      right: 12px;
      bottom: 10px;
      border: 1px solid var(--border);
      border-radius: 3px;
      background: var(--surface);
      box-shadow: var(--shadow);
      padding: 4px 8px;
      color: var(--muted);
      font-size: 11px;
    }
    @media (max-width: 760px) {
      :root { --header-height: 180px; }
      .topbar { flex-wrap: wrap; padding-block: 7px; }
      .brand { order: -2; }
      #files-toggle { order: -3; }
      #title { order: 2; min-width: 100%; }
      .status { margin-left: auto; }
      .menubar, .toolbar { overflow-x: auto; padding-inline: 8px; }
      .selection-label { display: none; }
      .notebook-main { padding: 12px 6px 80px; }
      .notebook-context { padding-inline: 6px; }
      .cell { grid-template-columns: 58px minmax(0, 1fr); }
      .prompt { padding-right: 5px; font-size: 10px; }
    }
    @media (prefers-reduced-motion: reduce) {
      .cell-selection-progress { stroke-dasharray: 100 0; animation: none; }
    }
  </style>
</head>
<body>
  <header class="notebook-header">
    <div class="topbar">
      <button id="files-toggle" title="Open notebook browser">Files</button>
      <div class="brand"><span class="brand-mark">D</span><span>Dune Notebook</span></div>
      <input id="title" value="Untitled notebook" aria-label="Notebook title">
      <span id="status" class="status">Ready</span>
      <span id="kernel-state" class="kernel-pill"><span class="kernel-dot"></span>Dune VM</span>
      <button id="theme-toggle" title="Switch color theme">Dark</button>
    </div>
    <nav class="menubar" aria-label="Notebook menu">
      <details class="menu">
        <summary>File</summary>
        <div class="menu-content">
          <button id="menu-new">New notebook</button>
          <button id="menu-files">Open notebook</button>
          <div class="menu-separator"></div>
          <button id="menu-save"><span>Save</span><span class="shortcut">⌘S</span></button>
          <button id="menu-export">Export HTML</button>
        </div>
      </details>
      <details class="menu">
        <summary>Edit</summary>
        <div class="menu-content">
          <button id="menu-move-up">Move cell up</button>
          <button id="menu-move-down">Move cell down</button>
          <button id="menu-delete" class="danger">Delete cell</button>
        </div>
      </details>
      <details class="menu">
        <summary>Insert</summary>
        <div class="menu-content">
          <button id="menu-code-above"><span>Code above</span><span class="shortcut">A</span></button>
          <button id="menu-code-below"><span>Code below</span><span class="shortcut">B</span></button>
          <button id="menu-markdown-below">Markdown below</button>
        </div>
      </details>
      <details class="menu">
        <summary>Cell</summary>
        <div class="menu-content">
          <button id="menu-run"><span>Run selected cell</span><span class="shortcut">⇧↵</span></button>
          <button id="menu-run-all">Run all cells</button>
          <div class="menu-separator"></div>
          <button id="menu-code-type"><span>Cell type: Code</span><span class="shortcut">Y</span></button>
          <button id="menu-markdown-type"><span>Cell type: Markdown</span><span class="shortcut">M</span></button>
        </div>
      </details>
      <details class="menu">
        <summary>Kernel</summary>
        <div class="menu-content">
          <button id="menu-restart">Restart kernel</button>
        </div>
      </details>
      <details class="menu">
        <summary>View</summary>
        <div class="menu-content">
          <button id="menu-theme">Toggle light/dark theme</button>
        </div>
      </details>
      <details class="menu">
        <summary>Help</summary>
        <div class="menu-content">
          <button id="menu-shortcuts">Keyboard shortcuts</button>
        </div>
      </details>
    </nav>
    <div class="toolbar" aria-label="Notebook toolbar">
      <button id="save" title="Save notebook (Cmd/Ctrl+S)">Save</button>
      <button id="add-code" title="Insert code cell below">+ Code</button>
      <button id="add-markdown" title="Insert Markdown cell below">+ Markdown</button>
      <div class="toolbar-separator"></div>
      <button id="run" class="primary" title="Run selected cell (Shift+Enter runs and selects the next cell)">▶ Run</button>
      <button id="run-all" title="Run every code cell">Run all</button>
      <button id="restart" title="Restart the Dune VM kernel">Restart</button>
      <div class="toolbar-separator"></div>
      <button id="move-up" title="Move selected cell up">↑</button>
      <button id="move-down" title="Move selected cell down">↓</button>
      <button id="delete-cell" class="danger" title="Delete selected cell">Delete</button>
      <select id="cell-type" aria-label="Selected cell type">
        <option value="code">Code</option>
        <option value="markdown">Markdown</option>
      </select>
      <button id="export" title="Download a standalone HTML export">Export</button>
      <span id="selection-label" class="selection-label">No cell selected</span>
    </div>
  </header>

  <section id="file-panel" class="file-panel" hidden aria-label="Notebook browser">
    <div class="file-panel-header">
      <strong>Notebooks</strong>
      <button id="new-file">New</button>
      <button id="close-files" aria-label="Close notebook browser">×</button>
    </div>
    <div id="files" class="files"></div>
  </section>

  <main class="notebook-main">
    <div class="notebook-context">
      <span>Dune VM</span><span>/</span><span id="current-path" class="context-path">No notebook open</span>
      <span class="context-spacer"></span><span>Shift+Enter: run and select next</span>
    </div>
    <div id="workspace" class="workspace">
      <div class="welcome">Open a <code>.dnb</code> notebook from <strong>Files</strong>, or create a new one.</div>
    </div>
  </main>
  <div class="footer-status">Notebook · Dune bytecode VM</div>

  <script>
    const params = new URLSearchParams(location.search);
    const token = params.get("token") || "";
    let currentPath = params.get("path") || "";
    let sessionId = "";
    let notebook = null;
    let dirty = false;
    let focusedCell = -1;

    const filesElement = document.querySelector("#files");
    const filePanel = document.querySelector("#file-panel");
    const workspace = document.querySelector("#workspace");
    const titleInput = document.querySelector("#title");
    const statusElement = document.querySelector("#status");
    const kernelElement = document.querySelector("#kernel-state");
    const themeButton = document.querySelector("#theme-toggle");
    const cellTypeSelect = document.querySelector("#cell-type");
    const selectionLabel = document.querySelector("#selection-label");
    const currentPathElement = document.querySelector("#current-path");

    function storedTheme() {
      try { return localStorage.getItem("dune-notebook-theme"); } catch (_) { return null; }
    }

    function applyTheme(theme, persist = true) {
      const normalized = theme === "dark" ? "dark" : "light";
      document.documentElement.dataset.theme = normalized;
      themeButton.textContent = normalized === "dark" ? "Light" : "Dark";
      themeButton.title = `Switch to ${normalized === "dark" ? "light" : "dark"} theme`;
      if (persist) {
        try { localStorage.setItem("dune-notebook-theme", normalized); } catch (_) {}
      }
    }

    const initialTheme = storedTheme() || (matchMedia("(prefers-color-scheme: dark)").matches ? "dark" : "light");
    applyTheme(initialTheme, false);

    function toggleTheme() {
      applyTheme(document.documentElement.dataset.theme === "dark" ? "light" : "dark");
    }

    function setStatus(message, error = false) {
      statusElement.textContent = message;
      statusElement.classList.toggle("error", error);
    }

    function setKernelBusy(busy) {
      kernelElement.classList.toggle("busy", busy);
      kernelElement.lastChild.textContent = busy ? " Running" : " Dune VM";
    }

    async function api(path, options = {}) {
      const headers = new Headers(options.headers || {});
      headers.set("X-Dune-Token", token);
      const response = await fetch(path, {...options, headers});
      if (!response.ok) {
        let message = `${response.status} ${response.statusText}`;
        try { message = (await response.json()).error || message; } catch (_) {}
        throw new Error(message);
      }
      return response;
    }

    function escapeHtml(value) {
      return value.replaceAll("&", "&amp;").replaceAll("<", "&lt;").replaceAll(">", "&gt;")
        .replaceAll('"', "&quot;");
    }

    const duneKeywords = new Set([
      "as", "break", "choice", "const", "continue", "contract", "derive", "else", "export", "fn",
      "for", "foreknown", "foreign", "from", "if", "import", "in", "is", "method", "module", "record",
      "return", "static", "test", "to", "type", "when", "while", "with"
    ]);
    const duneTypes = new Set([
      "bool", "glyph", "i8", "i16", "i32", "i64", "int", "isize", "real", "real32", "real64", "text",
      "u8", "u16", "u32", "u64", "uint8", "uint16", "uint32", "uint64", "unit", "usize"
    ]);
    const duneLiterals = new Set(["false", "true"]);
    const duneDeclarationKeywords = new Set(["choice", "contract", "fn", "method", "module", "record", "type"]);
    const duneNamespaceKeywords = new Set(["as", "from", "import"]);
    const duneIntegerSuffixes = new Set(["i8", "i16", "i32", "i64", "isize", "u8", "u16", "u32", "u64", "usize"]);

    function isAsciiAlpha(value) {
      return typeof value === "string" && value.length > 0 && /[A-Za-z]/.test(value[0]);
    }

    function isAsciiDigit(value) {
      return typeof value === "string" && value.length > 0 && value[0] >= "0" && value[0] <= "9";
    }

    function isIdentifierPart(value) {
      return isAsciiAlpha(value) || isAsciiDigit(value) || value === "_";
    }

    function lexDune(source) {
      const tokens = [];
      let position = 0;
      let previousSignificant = "";
      const append = (kind, start, end) => {
        const text = source.slice(start, end);
        tokens.push({kind, text});
        if (kind && kind !== "comment" && kind !== "invalid") previousSignificant = text;
      };
      const consumeQuoted = (start, quote, raw = false) => {
        let cursor = start + (raw ? 2 : 1);
        let invalid = false;
        const validEscapes = quote === "\"" ? "nrt0\"\\" : "nrt0'\\";
        while (cursor < source.length) {
          if (source[cursor] === quote) return {end: cursor + 1, invalid};
          if (source[cursor] === "\n" || source[cursor] === "\r") return {end: cursor, invalid: true};
          if (!raw && source[cursor] === "\\") {
            if (cursor + 1 >= source.length || !validEscapes.includes(source[cursor + 1])) invalid = true;
            cursor += Math.min(2, source.length - cursor);
          } else {
            const codePoint = source.codePointAt(cursor);
            cursor += codePoint > 0xffff ? 2 : 1;
          }
        }
        return {end: cursor, invalid: true};
      };
      const consumeGlyph = start => {
        let cursor = start + 1;
        let invalid = false;
        if (cursor >= source.length || source[cursor] === "\n" || source[cursor] === "\r") {
          return {end: cursor, invalid: true};
        }
        if (source[cursor] === "\\") {
          cursor += 1;
          if (cursor >= source.length || !"nrt0'\\".includes(source[cursor])) invalid = true;
          if (cursor < source.length) cursor += 1;
        } else if (source[cursor] === "'") {
          cursor += 1;
          return {end: cursor, invalid: true};
        } else {
          const codePoint = source.codePointAt(cursor);
          cursor += codePoint > 0xffff ? 2 : 1;
        }
        if (source[cursor] === "'") return {end: cursor + 1, invalid};
        invalid = true;
        while (cursor < source.length && source[cursor] !== "'" && source[cursor] !== "\n" &&
               source[cursor] !== "\r") cursor += 1;
        if (source[cursor] === "'") cursor += 1;
        return {end: cursor, invalid};
      };
      const consumeNumber = start => {
        let cursor = start;
        let invalid = false;
        const consumeDigits = (predicate, alreadySawDigit = false) => {
          let sawDigit = alreadySawDigit;
          let previousSeparator = false;
          while (cursor < source.length && (predicate(source[cursor]) || source[cursor] === "_")) {
            if (source[cursor] === "_") {
              if (!sawDigit || previousSeparator) invalid = true;
              previousSeparator = true;
            } else {
              sawDigit = true;
              previousSeparator = false;
            }
            cursor += 1;
          }
          if (!sawDigit || previousSeparator) invalid = true;
        };
        const consumeSuffix = () => {
          if (!isAsciiAlpha(source[cursor])) return;
          const suffixStart = cursor;
          while (isAsciiAlpha(source[cursor]) || isAsciiDigit(source[cursor])) cursor += 1;
          if (!duneIntegerSuffixes.has(source.slice(suffixStart, cursor))) invalid = true;
        };

        cursor += 1;
        if (source[start] === "0" && (source[cursor] === "x" || source[cursor] === "X")) {
          cursor += 1;
          consumeDigits(value => isAsciiDigit(value) || (value.toLowerCase() >= "a" && value.toLowerCase() <= "f"));
          consumeSuffix();
          return {end: cursor, invalid};
        }
        if (source[start] === "0" && (source[cursor] === "b" || source[cursor] === "B")) {
          cursor += 1;
          consumeDigits(value => value === "0" || value === "1");
          if (isAsciiDigit(source[cursor])) {
            invalid = true;
            while (isIdentifierPart(source[cursor])) cursor += 1;
          } else {
            consumeSuffix();
          }
          return {end: cursor, invalid};
        }

        consumeDigits(isAsciiDigit, true);
        if (source[cursor] === "." && isAsciiDigit(source[cursor + 1])) {
          cursor += 1;
          consumeDigits(isAsciiDigit, true);
          if (isAsciiAlpha(source[cursor])) {
            invalid = true;
            while (isAsciiAlpha(source[cursor]) || isAsciiDigit(source[cursor])) cursor += 1;
          }
          return {end: cursor, invalid};
        }
        consumeSuffix();
        return {end: cursor, invalid};
      };
      const identifierKind = (identifier, end) => {
        if (duneKeywords.has(identifier)) return "keyword";
        if (duneTypes.has(identifier)) return "type";
        if (duneLiterals.has(identifier)) return "literal";
        if (duneDeclarationKeywords.has(previousSignificant)) return "declaration";
        if (duneNamespaceKeywords.has(previousSignificant)) return "namespace";
        if (/^[A-Z][A-Z0-9_]*$/.test(identifier)) return "constant";
        if (/^[A-Z][A-Za-z0-9_]*$/.test(identifier)) return "type";
        let next = end;
        while (source[next] === " " || source[next] === "\t" || source[next] === "\r" || source[next] === "\n") next += 1;
        return source[next] === "(" ? "function" : "identifier";
      };

      while (position < source.length) {
        const start = position;
        const current = source[position];
        if (/\s/.test(current)) {
          while (position < source.length && /\s/.test(source[position])) position += 1;
          append("", start, position);
          continue;
        }
        if (current === "/" && source[position + 1] === "/") {
          position += 2;
          while (position < source.length && source[position] !== "\n") position += 1;
          append("comment", start, position);
          continue;
        }
        if (current === "/" && source[position + 1] === "*") {
          const closing = source.indexOf("*/", position + 2);
          position = closing < 0 ? source.length : closing + 2;
          append(closing < 0 ? "invalid" : "comment", start, position);
          continue;
        }
        if (current === "r" && source[position + 1] === "\"") {
          const quoted = consumeQuoted(start, "\"", true);
          position = quoted.end;
          append(quoted.invalid ? "invalid" : "string", start, position);
          continue;
        }
        if (current === "\"") {
          const quoted = consumeQuoted(start, current);
          position = quoted.end;
          append(quoted.invalid ? "invalid" : "string", start, position);
          continue;
        }
        if (current === "'") {
          const glyph = consumeGlyph(start);
          position = glyph.end;
          append(glyph.invalid ? "invalid" : "string", start, position);
          continue;
        }
        if (isAsciiDigit(current)) {
          const number = consumeNumber(start);
          position = number.end;
          append(number.invalid ? "invalid" : "number", start, position);
          continue;
        }
        if (isAsciiAlpha(current) || current === "_") {
          position += 1;
          while (isIdentifierPart(source[position])) position += 1;
          const identifier = source.slice(start, position);
          append(identifierKind(identifier, position), start, position);
          continue;
        }
        const pair = source.slice(position, position + 2);
        if (["->", "=>", "==", "!=", "&&", "||", ">=", "<=", ".."].includes(pair)) {
          position += 2;
          append("operator", start, position);
          continue;
        }
        if ("+-*/%=!><".includes(current)) {
          position += 1;
          append("operator", start, position);
          continue;
        }
        if (":,.?;(){}[]".includes(current)) {
          position += 1;
          append("punctuation", start, position);
          continue;
        }
        position += 1;
        append("invalid", start, position);
      }
      return tokens;
    }

    function highlightDune(source) {
      let html = lexDune(source).map(token => {
        const escaped = escapeHtml(token.text);
        return token.kind && token.kind !== "identifier" && token.kind !== "punctuation"
          ? `<span class="syntax-${token.kind}">${escaped}</span>`
          : escaped;
      }).join("");
      if (!html || source.endsWith("\n")) html += " ";
      return html;
    }

    function syncCodeHighlight(source, highlight) {
      highlight.innerHTML = highlightDune(source.value);
      highlight.scrollTop = source.scrollTop;
      highlight.scrollLeft = source.scrollLeft;
    }

    function renderMarkdown(source) {
      const lines = source.split("\n");
      let html = "";
      let list = false;
      let code = false;
      for (const raw of lines) {
        if (raw.startsWith("```")) {
          if (list) { html += "</ul>"; list = false; }
          html += code ? "</code></pre>" : "<pre><code>";
          code = !code;
          continue;
        }
        if (code) { html += `${escapeHtml(raw)}\n`; continue; }
        if (raw.startsWith("- ")) {
          if (!list) { html += "<ul>"; list = true; }
          html += `<li>${inlineMarkdown(raw.slice(2))}</li>`;
          continue;
        }
        if (list) { html += "</ul>"; list = false; }
        const heading = raw.match(/^(#{1,6})\s+(.*)$/);
        if (heading) {
          const level = heading[1].length;
          html += `<h${level}>${inlineMarkdown(heading[2])}</h${level}>`;
        } else if (raw.trim()) {
          html += `<p>${inlineMarkdown(raw)}</p>`;
        }
      }
      if (list) html += "</ul>";
      if (code) html += "</code></pre>";
      return html;
    }

    function inlineMarkdown(value) {
      return escapeHtml(value)
        .replace(/`([^`]+)`/g, "<code>$1</code>")
        .replace(/\*\*([^*]+)\*\*/g, "<strong>$1</strong>")
        .replace(/\*([^*]+)\*/g, "<em>$1</em>");
    }

    function nextId() {
      if (crypto.randomUUID) return crypto.randomUUID();
      return `cell-${Date.now()}-${Math.random().toString(16).slice(2)}`;
    }

    function newCodeCell() {
      return {id: nextId(), cell_type: "code", source: "", execution_count: null, outputs: []};
    }

    function newMarkdownCell() {
      return {id: nextId(), cell_type: "markdown", source: "Write **Markdown** here."};
    }

    function makeSelectionRing() {
      const namespace = "http://www.w3.org/2000/svg";
      const ring = document.createElementNS(namespace, "svg");
      ring.classList.add("cell-selection-ring");
      ring.setAttribute("aria-hidden", "true");
      for (const className of ["cell-selection-track", "cell-selection-progress"]) {
        const border = document.createElementNS(namespace, "rect");
        border.classList.add(className);
        border.setAttribute("pathLength", "100");
        ring.append(border);
      }
      return ring;
    }

    function markDirty() {
      dirty = true;
      setStatus("Unsaved changes");
    }

    function autoSize(textarea) {
      textarea.style.height = "auto";
      const minimum = textarea.classList.contains("markdown-source") ? 90 : 56;
      textarea.style.height = `${Math.max(minimum, textarea.scrollHeight)}px`;
    }

    function outputText(cell, name) {
      return (cell.outputs || []).filter(output => output.output_type === "stream" && output.name === name)
        .map(output => output.text || "").join("");
    }

    function selectedIndex() {
      if (!notebook || notebook.cells.length === 0) return -1;
      if (focusedCell < 0 || focusedCell >= notebook.cells.length) focusedCell = 0;
      return focusedCell;
    }

    function updateSelection() {
      document.querySelectorAll(".cell").forEach((cell, index) => {
        cell.classList.toggle("selected", index === focusedCell);
      });
      const index = selectedIndex();
      const hasSelection = index >= 0;
      cellTypeSelect.disabled = !hasSelection;
      if (hasSelection) {
        cellTypeSelect.value = notebook.cells[index].cell_type;
        selectionLabel.textContent = `Cell ${index + 1} of ${notebook.cells.length}`;
      } else {
        selectionLabel.textContent = "No cell selected";
      }
    }

    function selectCell(index, focusEditor = false) {
      if (!notebook || index < 0 || index >= notebook.cells.length) return;
      focusedCell = index;
      updateSelection();
      if (focusEditor) {
        const container = document.querySelector(`.cell[data-index="${index}"]`);
        const editor = container?.querySelector("textarea");
        if (notebook.cells[index].cell_type === "markdown") {
          container?.classList.add("edit-mode");
          const preview = container?.querySelector(".markdown-preview");
          if (preview) preview.hidden = true;
          if (editor) editor.hidden = false;
          if (editor) autoSize(editor);
        }
        editor?.focus();
      }
    }

    function selectNextCell(index) {
      let next = index + 1;
      if (next >= notebook.cells.length) {
        notebook.cells.push(newCodeCell());
        dirty = true;
        next = notebook.cells.length - 1;
      }
      focusedCell = next;
      renderNotebook();
      selectCell(next, true);
    }

    function moveCell(index, delta) {
      if (!notebook) return;
      const target = index + delta;
      if (target < 0 || target >= notebook.cells.length) return;
      [notebook.cells[index], notebook.cells[target]] = [notebook.cells[target], notebook.cells[index]];
      focusedCell = target;
      markDirty();
      renderNotebook();
    }

    function moveSelected(delta) {
      const index = selectedIndex();
      if (index >= 0) moveCell(index, delta);
    }

    function removeSelected() {
      const index = selectedIndex();
      if (index < 0 || !confirm(`Delete cell ${index + 1}?`)) return;
      notebook.cells.splice(index, 1);
      focusedCell = Math.min(index, notebook.cells.length - 1);
      markDirty();
      renderNotebook();
    }

    function insertCell(kind, position = "below") {
      if (!notebook) return;
      const selected = selectedIndex();
      const index = selected < 0 ? notebook.cells.length : selected + (position === "below" ? 1 : 0);
      notebook.cells.splice(index, 0, kind === "code" ? newCodeCell() : newMarkdownCell());
      focusedCell = index;
      markDirty();
      renderNotebook();
      selectCell(index, true);
    }

    function setSelectedCellType(kind) {
      const index = selectedIndex();
      if (index < 0 || notebook.cells[index].cell_type === kind) return;
      const cell = notebook.cells[index];
      cell.cell_type = kind;
      if (kind === "code") {
        cell.execution_count = null;
        cell.outputs = [];
      } else {
        delete cell.execution_count;
        delete cell.outputs;
      }
      markDirty();
      renderNotebook();
      selectCell(index, kind === "markdown");
    }

    function makeOutput(text, error = false) {
      const trimmed = text.trim();
      const isSvg = trimmed.startsWith("<svg") && (trimmed[4] === ">" || /\s/.test(trimmed[4])) &&
        trimmed.endsWith("</svg>");
      if (!error && isSvg) {
        const figure = document.createElement("figure");
        figure.className = "rich-output";
        const image = document.createElement("img");
        image.alt = "Chart output";
        image.src = `data:image/svg+xml;charset=utf-8,${encodeURIComponent(trimmed)}`;
        figure.append(image);
        return figure;
      }
      const output = document.createElement("pre");
      output.className = `output ${error ? "error" : ""}`;
      output.textContent = text;
      return output;
    }

    function renderNotebook() {
      workspace.replaceChildren();
      currentPathElement.textContent = currentPath || "No notebook open";
      if (!notebook) {
        workspace.innerHTML = '<div class="welcome">Open a <code>.dnb</code> notebook from <strong>Files</strong>, or create a new one.</div>';
        updateSelection();
        return;
      }
      titleInput.value = notebook.metadata?.title || "";
      if (notebook.cells.length === 0) {
        const empty = document.createElement("div");
        empty.className = "empty-notebook";
        empty.innerHTML = "This notebook has no cells.<br>";
        const add = document.createElement("button");
        add.textContent = "Add code cell";
        add.onclick = () => insertCell("code");
        empty.append(add);
        workspace.append(empty);
        updateSelection();
        return;
      }
      if (focusedCell < 0 || focusedCell >= notebook.cells.length) focusedCell = 0;

      notebook.cells.forEach((cell, index) => {
        const container = document.createElement("section");
        container.className = `cell ${focusedCell === index ? "selected" : ""}`;
        container.dataset.index = index;
        container.onclick = () => selectCell(index);

        const prompt = document.createElement("div");
        prompt.className = "prompt";
        prompt.textContent = cell.cell_type === "code" ? `In [${cell.execution_count ?? " "}]:` : "";
        const content = document.createElement("div");
        content.className = "cell-content";

        if (cell.cell_type === "markdown") {
          const preview = document.createElement("div");
          preview.className = "markdown-preview";
          preview.innerHTML = renderMarkdown(cell.source || "");
          const editor = document.createElement("textarea");
          editor.className = "markdown-source";
          editor.value = cell.source || "";
          editor.hidden = focusedCell !== index || !container.classList.contains("edit-mode");
          preview.hidden = !editor.hidden;
          preview.ondblclick = event => {
            event.stopPropagation();
            document.querySelectorAll(".cell.edit-mode").forEach(item => item.classList.remove("edit-mode"));
            container.classList.add("edit-mode");
            focusedCell = index;
            preview.hidden = true;
            editor.hidden = false;
            autoSize(editor);
            editor.focus();
            updateSelection();
          };
          editor.oninput = () => { cell.source = editor.value; autoSize(editor); markDirty(); };
          editor.onkeydown = event => {
            if (event.key === "Enter" && event.shiftKey) {
              event.preventDefault();
              runCell(index, true);
            } else if (event.key === "Enter" && (event.metaKey || event.ctrlKey)) {
              event.preventDefault();
              runCell(index);
            }
          };
          editor.onblur = () => {
            container.classList.remove("edit-mode");
            editor.hidden = true;
            preview.hidden = false;
            preview.innerHTML = renderMarkdown(editor.value);
          };
          content.append(preview, editor);
        } else {
          const editor = document.createElement("div");
          editor.className = "code-editor";
          const highlight = document.createElement("pre");
          highlight.className = "code-highlight";
          highlight.setAttribute("aria-hidden", "true");
          const source = document.createElement("textarea");
          source.className = "code-source";
          source.setAttribute("aria-label", "Dune code");
          source.setAttribute("autocomplete", "off");
          source.setAttribute("autocapitalize", "off");
          source.setAttribute("spellcheck", "false");
          source.value = cell.source || "";
          source.onfocus = () => { focusedCell = index; container.classList.add("edit-mode"); updateSelection(); };
          source.onblur = () => container.classList.remove("edit-mode");
          source.oninput = () => {
            cell.source = source.value;
            autoSize(source);
            syncCodeHighlight(source, highlight);
            markDirty();
          };
          source.onscroll = () => syncCodeHighlight(source, highlight);
          source.onkeydown = event => {
            if (event.key === "Tab") {
              event.preventDefault();
              source.setRangeText("    ", source.selectionStart, source.selectionEnd, "end");
              cell.source = source.value;
              markDirty();
              autoSize(source);
              syncCodeHighlight(source, highlight);
            } else if (event.key === "Enter" && event.shiftKey) {
              event.preventDefault();
              runCell(index, true);
            } else if (event.key === "Enter" && (event.metaKey || event.ctrlKey)) {
              event.preventDefault();
              runCell(index);
            }
          };
          setTimeout(() => {
            autoSize(source);
            syncCodeHighlight(source, highlight);
          }, 0);
          editor.append(highlight, source);
          content.append(editor);
          const stdout = outputText(cell, "stdout");
          const stderr = outputText(cell, "stderr");
          if (stdout) content.append(makeOutput(stdout));
          if (stderr) content.append(makeOutput(stderr, true));
        }

        const insert = document.createElement("button");
        insert.className = "cell-insert";
        insert.textContent = "+ Code";
        insert.title = "Insert code cell below";
        insert.onclick = event => { event.stopPropagation(); focusedCell = index; insertCell("code", "below"); };
        container.append(prompt, content, insert, makeSelectionRing());
        workspace.append(container);
      });
      updateSelection();
    }

    function setFilePanel(open) {
      filePanel.hidden = !open;
      document.querySelector("#files-toggle").setAttribute("aria-expanded", String(open));
    }

    async function refreshFiles() {
      const response = await api("/api/files");
      const data = await response.json();
      filesElement.replaceChildren();
      if (data.files.length === 0) {
        const empty = document.createElement("div");
        empty.className = "welcome";
        empty.textContent = "No .dnb notebooks in this workspace.";
        filesElement.append(empty);
        return;
      }
      for (const path of data.files) {
        const button = document.createElement("button");
        button.className = `file ${path === currentPath ? "active" : ""}`;
        button.textContent = path;
        button.title = path;
        button.onclick = () => openNotebook(path).catch(error => setStatus(error.message, true));
        filesElement.append(button);
      }
    }

    async function createSession() {
      if (sessionId) {
        try { await api(`/api/sessions/${encodeURIComponent(sessionId)}`, {method: "DELETE"}); } catch (_) {}
      }
      const response = await api("/api/sessions", {method: "POST", body: currentPath});
      sessionId = (await response.json()).id;
    }

    async function openNotebook(path) {
      if (dirty && !confirm("Discard unsaved changes?")) return;
      setStatus("Loading…");
      const response = await api(`/api/notebook?path=${encodeURIComponent(path)}`);
      notebook = await response.json();
      currentPath = path;
      dirty = false;
      focusedCell = notebook.cells.length > 0 ? 0 : -1;
      history.replaceState(null, "", `/?token=${encodeURIComponent(token)}&path=${encodeURIComponent(path)}`);
      await createSession();
      await refreshFiles();
      renderNotebook();
      setFilePanel(false);
      setStatus("Ready");
    }

    async function saveNotebook() {
      if (!notebook || !currentPath) return;
      notebook.metadata = notebook.metadata || {};
      notebook.metadata.title = titleInput.value;
      setStatus("Saving…");
      await api(`/api/notebook?path=${encodeURIComponent(currentPath)}`, {
        method: "PUT",
        headers: {"Content-Type": "application/json"},
        body: JSON.stringify(notebook, null, 2) + "\n"
      });
      dirty = false;
      await refreshFiles();
      setStatus("Saved");
    }

    async function runCell(index = selectedIndex(), advance = false) {
      if (!notebook || !sessionId || index < 0) return;
      if (notebook.cells[index].cell_type === "markdown") {
        if (advance) selectNextCell(index);
        else {
          focusedCell = index;
          renderNotebook();
        }
        setStatus(`Cell ${index + 1} rendered`);
        return;
      }
      setStatus(`Running cell ${index + 1}…`);
      setKernelBusy(true);
      try {
        const response = await api(`/api/sessions/${encodeURIComponent(sessionId)}/execute?cell=${index}`, {
          method: "POST",
          headers: {"Content-Type": "application/json"},
          body: JSON.stringify(notebook)
        });
        const data = await response.json();
        notebook = data.document;
        dirty = true;
        if (advance) selectNextCell(index);
        else {
          focusedCell = index;
          renderNotebook();
          selectCell(index, true);
        }
        setStatus(data.success ? `Cell ${index + 1} complete` : `Cell ${data.failed_cell + 1} failed`, !data.success);
      } catch (error) {
        setStatus(error.message, true);
      } finally {
        setKernelBusy(false);
      }
    }

    async function runAll() {
      if (!notebook || !sessionId) return;
      setStatus("Running all cells…");
      setKernelBusy(true);
      try {
        const response = await api(`/api/sessions/${encodeURIComponent(sessionId)}/execute?cell=all`, {
          method: "POST",
          headers: {"Content-Type": "application/json"},
          body: JSON.stringify(notebook)
        });
        const data = await response.json();
        notebook = data.document;
        dirty = true;
        renderNotebook();
        setStatus(data.success ? "All cells complete" : `Cell ${data.failed_cell + 1} failed`, !data.success);
      } catch (error) {
        setStatus(error.message, true);
      } finally {
        setKernelBusy(false);
      }
    }

    async function restartKernel() {
      if (!sessionId) return;
      setKernelBusy(true);
      try {
        await api(`/api/sessions/${encodeURIComponent(sessionId)}/reset`, {method: "POST"});
        setStatus("Kernel restarted");
      } finally {
        setKernelBusy(false);
      }
    }

    async function exportHtml() {
      if (!notebook) return;
      const response = await api("/api/export", {
        method: "POST",
        headers: {"Content-Type": "application/json"},
        body: JSON.stringify(notebook)
      });
      const blob = await response.blob();
      const url = URL.createObjectURL(blob);
      const download = document.createElement("a");
      download.href = url;
      download.download = `${(currentPath.split("/").pop() || "notebook").replace(/\.dnb$/, "")}.html`;
      download.click();
      setTimeout(() => URL.revokeObjectURL(url), 1000);
      setStatus("HTML exported");
    }

    async function newNotebook() {
      if (dirty && !confirm("Discard unsaved changes?")) return;
      let path = prompt("Notebook path", "notebooks/untitled.dnb");
      if (!path) return;
      if (!path.endsWith(".dnb")) path += ".dnb";
      const title = prompt("Notebook title", "Untitled notebook") || "Untitled notebook";
      const document = {
        dune_notebook: 1,
        metadata: {title},
        cells: [
          {id: nextId(), cell_type: "markdown", source: `# ${title}\n\nWrite your notes here.`},
          newCodeCell()
        ]
      };
      await api(`/api/notebook?path=${encodeURIComponent(path)}`, {
        method: "PUT",
        headers: {"Content-Type": "application/json", "If-None-Match": "*"},
        body: JSON.stringify(document, null, 2) + "\n"
      });
      dirty = false;
      await refreshFiles();
      await openNotebook(path);
    }

    function closeMenus() {
      document.querySelectorAll("details.menu[open]").forEach(menu => menu.removeAttribute("open"));
    }

    function handleFailure(action) {
      return () => {
        closeMenus();
        Promise.resolve().then(action).catch(error => setStatus(error.message, true));
      };
    }

    function bind(id, action) {
      document.querySelector(`#${id}`).onclick = handleFailure(action);
    }

    titleInput.oninput = () => {
      if (!notebook) return;
      notebook.metadata = notebook.metadata || {};
      notebook.metadata.title = titleInput.value;
      markDirty();
    };
    bind("files-toggle", () => setFilePanel(filePanel.hidden));
    bind("close-files", () => setFilePanel(false));
    bind("new-file", newNotebook);
    bind("save", saveNotebook);
    bind("add-code", () => insertCell("code"));
    bind("add-markdown", () => insertCell("markdown"));
    bind("run", () => runCell());
    bind("run-all", runAll);
    bind("restart", restartKernel);
    bind("move-up", () => moveSelected(-1));
    bind("move-down", () => moveSelected(1));
    bind("delete-cell", removeSelected);
    bind("export", exportHtml);
    bind("theme-toggle", toggleTheme);
    bind("menu-new", newNotebook);
    bind("menu-files", () => setFilePanel(true));
    bind("menu-save", saveNotebook);
    bind("menu-export", exportHtml);
    bind("menu-move-up", () => moveSelected(-1));
    bind("menu-move-down", () => moveSelected(1));
    bind("menu-delete", removeSelected);
    bind("menu-code-above", () => insertCell("code", "above"));
    bind("menu-code-below", () => insertCell("code", "below"));
    bind("menu-markdown-below", () => insertCell("markdown", "below"));
    bind("menu-run", () => runCell());
    bind("menu-run-all", runAll);
    bind("menu-code-type", () => setSelectedCellType("code"));
    bind("menu-markdown-type", () => setSelectedCellType("markdown"));
    bind("menu-restart", restartKernel);
    bind("menu-theme", toggleTheme);
    bind("menu-shortcuts", () => alert("Shift+Enter  Run and select next cell (creates one at the end)\nCmd/Ctrl+Enter  Run and stay in cell\nCmd/Ctrl+S  Save\nA  Insert code above\nB  Insert code below\nY  Change to Code\nM  Change to Markdown"));
    cellTypeSelect.onchange = () => setSelectedCellType(cellTypeSelect.value);

    document.addEventListener("click", event => {
      document.querySelectorAll("details.menu[open]").forEach(menu => {
        if (!menu.contains(event.target)) menu.removeAttribute("open");
      });
      if (!filePanel.hidden && !filePanel.contains(event.target) && !document.querySelector("#files-toggle").contains(event.target)) {
        setFilePanel(false);
      }
    });

    document.addEventListener("keydown", event => {
      if ((event.metaKey || event.ctrlKey) && event.key.toLowerCase() === "s") {
        event.preventDefault();
        saveNotebook().catch(error => setStatus(error.message, true));
        return;
      }
      if (["INPUT", "TEXTAREA", "SELECT"].includes(event.target.tagName)) return;
      const key = event.key.toLowerCase();
      if (event.key === "Enter" && event.shiftKey) {
        event.preventDefault();
        runCell(selectedIndex(), true);
      } else if (event.key === "Enter" && (event.metaKey || event.ctrlKey)) {
        event.preventDefault();
        runCell();
      } else if (key === "a") insertCell("code", "above");
      else if (key === "b") insertCell("code", "below");
      else if (key === "y") setSelectedCellType("code");
      else if (key === "m") setSelectedCellType("markdown");
    });

    addEventListener("beforeunload", event => {
      if (dirty) { event.preventDefault(); event.returnValue = ""; }
    });

    (async () => {
      try {
        await refreshFiles();
        if (currentPath) await openNotebook(currentPath);
      } catch (error) {
        setStatus(error.message, true);
      }
    })();
  </script>
</body>
</html>
)dune_notebook";
}

} // namespace dune::notebook
