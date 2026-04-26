use std::{path::Path, sync::mpsc};

use anyhow::{Context, Result};
use notify::{RecommendedWatcher, RecursiveMode, Watcher};

/// A handle to a file system watcher for a content pack directory.
/// Drop this handle to stop watching.
pub struct WatcherHandle {
    _watcher: RecommendedWatcher,
    pub rx: mpsc::Receiver<notify::Result<notify::Event>>,
}

impl WatcherHandle {
    /// Start watching `path` recursively for changes.
    pub fn watch(path: &Path) -> Result<Self> {
        let (tx, rx) = mpsc::channel();
        let mut watcher = notify::recommended_watcher(move |event| {
            let _ = tx.send(event);
        })
        .context("creating file watcher")?;

        watcher
            .watch(path, RecursiveMode::Recursive)
            .with_context(|| format!("watching {}", path.display()))?;

        Ok(Self {
            _watcher: watcher,
            rx,
        })
    }
}
