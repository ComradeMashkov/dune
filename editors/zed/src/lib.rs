use zed_extension_api::{self as zed, LanguageServerId, Result, Worktree};

struct DuneExtension;

impl DuneExtension {
    fn local_binary(worktree: &Worktree) -> String {
        let root = worktree.root_path();
        let separator = if root.ends_with('/') || root.ends_with('\\') { "" } else { "/" };
        let (os, _) = zed::current_platform();
        let binary = if os == zed::Os::Windows {
            "dune.exe"
        } else {
            "dune"
        };

        format!("{root}{separator}build/{binary}")
    }
}

impl zed::Extension for DuneExtension {
    fn new() -> Self {
        Self
    }

    fn language_server_command(
        &mut self,
        _language_server_id: &LanguageServerId,
        worktree: &Worktree,
    ) -> Result<zed::Command> {
        let command = worktree
            .which("dune")
            .unwrap_or_else(|| Self::local_binary(worktree));
        let root = worktree.root_path();

        Ok(zed::Command {
            command,
            args: vec!["lsp".to_string()],
            env: vec![("DUNE_STDLIB_PATH".to_string(), format!("{root}/stdlib"))],
        })
    }
}

zed::register_extension!(DuneExtension);
