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
      --ink: #25231f;
      --muted: #746d62;
      --paper: #f4f1ea;
      --panel: #fffdfa;
      --line: #ddd5c8;
      --accent: #9a6b2f;
      --accent-dark: #68451d;
      --error: #a63d35;
      --code: #262521;
      --code-ink: #f8f1e5;
      --shadow: 0 12px 36px rgba(54, 45, 33, .08);
    }
    * { box-sizing: border-box; }
    [hidden] { display: none !important; }
    body {
      margin: 0;
      background: var(--paper);
      color: var(--ink);
      font: 15px/1.5 Inter, ui-sans-serif, system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
    }
    button, input, textarea { font: inherit; }
    button {
      border: 1px solid var(--line);
      border-radius: 7px;
      background: var(--panel);
      color: var(--ink);
      padding: 7px 11px;
      cursor: pointer;
    }
    button:hover { border-color: #b9aa93; background: #fff; }
    button.primary { background: var(--accent); border-color: var(--accent); color: white; }
    button.primary:hover { background: var(--accent-dark); }
    button.danger { color: var(--error); }
    .shell { min-height: 100vh; display: grid; grid-template-columns: 250px minmax(0, 1fr); }
    aside {
      position: sticky;
      top: 0;
      height: 100vh;
      border-right: 1px solid var(--line);
      background: #ebe6dc;
      padding: 20px 14px;
      overflow: auto;
    }
    .brand { padding: 0 8px 18px; }
    .brand strong { display: block; font-size: 18px; letter-spacing: -.02em; }
    .brand span { color: var(--muted); font-size: 12px; text-transform: uppercase; letter-spacing: .12em; }
    .files-title { display: flex; align-items: center; justify-content: space-between; padding: 10px 8px; }
    .files { display: grid; gap: 3px; }
    .file {
      display: block;
      width: 100%;
      border: 0;
      background: transparent;
      text-align: left;
      overflow: hidden;
      text-overflow: ellipsis;
      white-space: nowrap;
    }
    .file.active { background: #fff9ed; color: var(--accent-dark); font-weight: 650; }
    main { min-width: 0; }
    header {
      position: sticky;
      z-index: 10;
      top: 0;
      min-height: 64px;
      display: flex;
      align-items: center;
      gap: 9px;
      padding: 10px 24px;
      border-bottom: 1px solid var(--line);
      background: rgba(244, 241, 234, .94);
      backdrop-filter: blur(10px);
    }
    #title {
      min-width: 160px;
      flex: 1;
      border: 0;
      background: transparent;
      color: var(--ink);
      font-size: 18px;
      font-weight: 650;
      outline: none;
    }
    .status { color: var(--muted); font-size: 12px; white-space: nowrap; }
    .workspace { max-width: 980px; margin: 0 auto; padding: 34px 28px 120px; }
    .welcome {
      background: var(--panel);
      border: 1px solid var(--line);
      border-radius: 14px;
      box-shadow: var(--shadow);
      padding: 36px;
      text-align: center;
      color: var(--muted);
    }
    .cell {
      position: relative;
      margin: 18px 0;
      border: 1px solid transparent;
      border-radius: 11px;
      transition: border-color .12s ease, box-shadow .12s ease;
    }
    .cell:hover, .cell.focused { border-color: var(--line); box-shadow: var(--shadow); }
    .cell-toolbar {
      min-height: 36px;
      display: flex;
      align-items: center;
      gap: 6px;
      padding: 5px 7px;
      opacity: .35;
      transition: opacity .12s ease;
    }
    .cell:hover .cell-toolbar, .cell.focused .cell-toolbar { opacity: 1; }
    .cell-toolbar button { padding: 3px 8px; font-size: 12px; }
    .cell-kind { margin-right: auto; color: var(--muted); font: 12px ui-monospace, monospace; }
    .count { width: 58px; color: var(--muted); text-align: right; font: 12px ui-monospace, monospace; }
    textarea {
      display: block;
      width: 100%;
      resize: vertical;
      border: 0;
      outline: none;
      overflow: hidden;
    }
    .code-source {
      min-height: 68px;
      border-radius: 8px;
      background: var(--code);
      color: var(--code-ink);
      padding: 16px 18px;
      tab-size: 4;
      font: 14px/1.55 "SFMono-Regular", Consolas, "Liberation Mono", monospace;
    }
    .markdown-source {
      min-height: 90px;
      border-radius: 8px;
      background: #fff;
      padding: 16px 18px;
      color: var(--ink);
      font: 14px/1.55 "SFMono-Regular", Consolas, monospace;
    }
    .markdown-preview { padding: 8px 20px 18px; min-height: 44px; }
    .markdown-preview h1, .markdown-preview h2, .markdown-preview h3 { line-height: 1.2; }
    .markdown-preview code { background: #e9e3d8; border-radius: 4px; padding: .1em .3em; }
    .markdown-preview pre { overflow: auto; background: var(--code); color: var(--code-ink); padding: 14px; border-radius: 8px; }
    .output {
      margin: 8px 0 0 58px;
      border-left: 3px solid var(--accent);
      background: var(--panel);
      padding: 11px 14px;
      white-space: pre-wrap;
      overflow-wrap: anywhere;
      font: 13px/1.5 "SFMono-Regular", Consolas, monospace;
    }
    .output.error { border-left-color: var(--error); color: #812d27; background: #fff5f3; }
    .add-row { display: flex; justify-content: center; gap: 8px; margin: 20px 0; opacity: .55; }
    .add-row:hover { opacity: 1; }
    @media (max-width: 760px) {
      .shell { display: block; }
      aside { position: static; height: auto; border-right: 0; border-bottom: 1px solid var(--line); }
      .files { grid-template-columns: repeat(auto-fill, minmax(170px, 1fr)); }
      header { flex-wrap: wrap; padding: 10px 12px; }
      .workspace { padding: 20px 12px 90px; }
      .output { margin-left: 0; }
    }
  </style>
</head>
<body>
  <div class="shell">
    <aside>
      <div class="brand"><strong>Dune Notebook</strong><span>bytecode VM workspace</span></div>
      <div class="files-title"><strong>Notebooks</strong><button id="new-file">New</button></div>
      <div id="files" class="files"></div>
    </aside>
    <main>
      <header>
        <input id="title" value="Untitled notebook" aria-label="Notebook title">
        <span id="status" class="status">Ready</span>
        <button id="restart">Restart kernel</button>
        <button id="run-all">Run all</button>
        <button id="export">Export HTML</button>
        <button id="save" class="primary">Save</button>
      </header>
      <div id="workspace" class="workspace">
        <div class="welcome">Choose a notebook or create a new <code>.dnb</code> document.</div>
      </div>
    </main>
  </div>
  <script>
    const params = new URLSearchParams(location.search);
    const token = params.get("token") || "";
    let currentPath = params.get("path") || "";
    let sessionId = "";
    let notebook = null;
    let dirty = false;
    let focusedCell = -1;

    const filesElement = document.querySelector("#files");
    const workspace = document.querySelector("#workspace");
    const titleInput = document.querySelector("#title");
    const statusElement = document.querySelector("#status");

    function setStatus(message, error = false) {
      statusElement.textContent = message;
      statusElement.style.color = error ? "var(--error)" : "";
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

    function markDirty() {
      dirty = true;
      setStatus("Unsaved changes");
    }

    function autoSize(textarea) {
      textarea.style.height = "auto";
      textarea.style.height = `${Math.max(68, textarea.scrollHeight)}px`;
    }

    function outputText(cell, name) {
      return (cell.outputs || []).filter(output => output.output_type === "stream" && output.name === name)
        .map(output => output.text || "").join("");
    }

    function moveCell(index, delta) {
      const target = index + delta;
      if (target < 0 || target >= notebook.cells.length) return;
      [notebook.cells[index], notebook.cells[target]] = [notebook.cells[target], notebook.cells[index]];
      focusedCell = target;
      markDirty();
      renderNotebook();
    }

    function removeCell(index) {
      notebook.cells.splice(index, 1);
      focusedCell = Math.min(index, notebook.cells.length - 1);
      markDirty();
      renderNotebook();
    }

    function addCell(index, kind) {
      notebook.cells.splice(index, 0, kind === "code" ? newCodeCell() : newMarkdownCell());
      focusedCell = index;
      markDirty();
      renderNotebook();
    }

    function toolbar(cell, index) {
      const bar = document.createElement("div");
      bar.className = "cell-toolbar";
      bar.onclick = event => event.stopPropagation();
      const kind = document.createElement("span");
      kind.className = "cell-kind";
      kind.textContent = cell.cell_type;
      bar.append(kind);

      if (cell.cell_type === "code") {
        const run = document.createElement("button");
        run.textContent = "Run";
        run.onclick = () => runCell(index);
        bar.append(run);
      }
      for (const [label, action] of [["↑", () => moveCell(index, -1)], ["↓", () => moveCell(index, 1)]]) {
        const button = document.createElement("button");
        button.textContent = label;
        button.onclick = action;
        bar.append(button);
      }
      const remove = document.createElement("button");
      remove.textContent = "Delete";
      remove.className = "danger";
      remove.onclick = () => removeCell(index);
      bar.append(remove);
      return bar;
    }

    function addRow(index) {
      const row = document.createElement("div");
      row.className = "add-row";
      const code = document.createElement("button");
      code.textContent = "+ Code";
      code.onclick = () => addCell(index, "code");
      const markdown = document.createElement("button");
      markdown.textContent = "+ Markdown";
      markdown.onclick = () => addCell(index, "markdown");
      row.append(code, markdown);
      return row;
    }

    function renderNotebook() {
      workspace.replaceChildren();
      if (!notebook) {
        workspace.innerHTML = '<div class="welcome">Choose a notebook or create a new <code>.dnb</code> document.</div>';
        return;
      }
      titleInput.value = notebook.metadata?.title || "";
      workspace.append(addRow(0));
      notebook.cells.forEach((cell, index) => {
        const container = document.createElement("section");
        container.className = `cell ${focusedCell === index ? "focused" : ""}`;
        container.onclick = () => { focusedCell = index; };
        container.append(toolbar(cell, index));

        if (cell.cell_type === "markdown") {
          const preview = document.createElement("div");
          preview.className = "markdown-preview";
          preview.innerHTML = renderMarkdown(cell.source || "");
          const editor = document.createElement("textarea");
          editor.className = "markdown-source";
          editor.value = cell.source || "";
          editor.hidden = focusedCell !== index;
          preview.hidden = !editor.hidden;
          preview.ondblclick = () => { focusedCell = index; renderNotebook(); };
          editor.oninput = () => { cell.source = editor.value; autoSize(editor); markDirty(); };
          editor.onblur = () => {
            focusedCell = -1;
            editor.hidden = true;
            preview.hidden = false;
            preview.innerHTML = renderMarkdown(editor.value);
            container.classList.remove("focused");
          };
          container.append(preview, editor);
          if (!editor.hidden) setTimeout(() => { autoSize(editor); editor.focus(); }, 0);
        } else {
          const source = document.createElement("textarea");
          source.className = "code-source";
          source.value = cell.source || "";
          source.oninput = () => { cell.source = source.value; autoSize(source); markDirty(); };
          source.onkeydown = event => {
            if (event.key === "Enter" && event.shiftKey) {
              event.preventDefault();
              runCell(index);
            }
          };
          setTimeout(() => autoSize(source), 0);
          const count = document.createElement("div");
          count.className = "count";
          count.textContent = `In [${cell.execution_count ?? " "}]`;
          container.append(count, source);
          const stdout = outputText(cell, "stdout");
          const stderr = outputText(cell, "stderr");
          if (stdout) {
            const output = document.createElement("pre");
            output.className = "output";
            output.textContent = stdout;
            container.append(output);
          }
          if (stderr) {
            const output = document.createElement("pre");
            output.className = "output error";
            output.textContent = stderr;
            container.append(output);
          }
        }
        workspace.append(container, addRow(index + 1));
      });
    }

    async function refreshFiles() {
      const response = await api("/api/files");
      const data = await response.json();
      filesElement.replaceChildren();
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
      focusedCell = -1;
      history.replaceState(null, "", `/?token=${encodeURIComponent(token)}&path=${encodeURIComponent(path)}`);
      await createSession();
      await refreshFiles();
      renderNotebook();
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

    async function runCell(index) {
      if (!notebook || !sessionId) return;
      setStatus(`Running cell ${index + 1}…`);
      try {
        const response = await api(`/api/sessions/${encodeURIComponent(sessionId)}/execute?cell=${index}`, {
          method: "POST",
          headers: {"Content-Type": "application/json"},
          body: JSON.stringify(notebook)
        });
        const data = await response.json();
        notebook = data.document;
        dirty = true;
        renderNotebook();
        setStatus(data.success ? `Cell ${index + 1} complete` : `Cell ${data.failed_cell + 1} failed`, !data.success);
      } catch (error) {
        setStatus(error.message, true);
      }
    }

    async function runAll() {
      if (!notebook || !sessionId) return;
      setStatus("Running all cells…");
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
      }
    }

    async function restartKernel() {
      if (!sessionId) return;
      await api(`/api/sessions/${encodeURIComponent(sessionId)}/reset`, {method: "POST"});
      setStatus("Kernel restarted");
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

    titleInput.oninput = () => {
      if (!notebook) return;
      notebook.metadata = notebook.metadata || {};
      notebook.metadata.title = titleInput.value;
      markDirty();
    };
    const handleFailure = action => () => action().catch(error => setStatus(error.message, true));
    document.querySelector("#new-file").onclick = handleFailure(newNotebook);
    document.querySelector("#save").onclick = handleFailure(saveNotebook);
    document.querySelector("#run-all").onclick = runAll;
    document.querySelector("#restart").onclick = handleFailure(restartKernel);
    document.querySelector("#export").onclick = handleFailure(exportHtml);
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
