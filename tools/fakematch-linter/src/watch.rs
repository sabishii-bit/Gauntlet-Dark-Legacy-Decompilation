//! Editor watch mode: rescan on file-system events, reusing unchanged
//! per-file results. The editor task owns the process lifetime.

use std::io::Write;
use std::path::{Path, PathBuf};
use std::sync::mpsc;
use std::time::Duration;

use anyhow::{Context, Result};
use notify::{RecursiveMode, Watcher};

use crate::scan::{Options, ScanError, Scanner};

/// Paths whose changes should trigger a rescan.
pub fn watched_paths(scanner: &Scanner) -> Vec<(PathBuf, RecursiveMode)> {
    let root = &scanner.root;
    let config = scanner.config();
    let mut paths: Vec<(PathBuf, RecursiveMode)> = config
        .scan
        .paths
        .iter()
        .map(|p| (root.join(p), RecursiveMode::Recursive))
        .collect();
    let mut singles: Vec<PathBuf> = Vec::new();
    if let Some(p) = &scanner.loaded.path {
        singles.push(p.clone());
    }
    if let Some(p) = &config.policy.file {
        singles.push(root.join(p));
    }
    if let Some(p) = &config.guidance.path {
        singles.push(root.join(p));
    }
    for e in &config.postprocessors.engines {
        singles.push(root.join(&e.path));
    }
    for p in &config.watch.extra_paths {
        singles.push(root.join(p));
    }
    for s in singles {
        // Watch the parent directory so files that do not exist yet are seen.
        let dir = if s.is_dir() { s.clone() } else { s.parent().map(Path::to_path_buf).unwrap_or(s.clone()) };
        if dir.exists() && !paths.iter().any(|(p, _)| *p == dir) {
            paths.push((dir, RecursiveMode::NonRecursive));
        }
    }
    paths
}

fn run_once(scanner: &Scanner, opts: &Options, out: &mut dyn Write) -> i32 {
    let output = scanner.config().output.clone();
    let _ = writeln!(out, "{}", output.begin_marker);
    let _ = out.flush();
    let status = match scanner.run(opts, out) {
        Ok(outcome) => outcome.exit_code,
        Err(e) => {
            crate::print_failure(scanner, &e, opts.format, out);
            2
        }
    };
    let _ = writeln!(out, "{} status={status}", output.end_marker);
    let _ = out.flush();
    status
}

/// Scan once, then rescan whenever watched files change.
pub fn watch(scanner: &Scanner, opts: &Options, out: &mut dyn Write) -> Result<i32> {
    let (tx, rx) = mpsc::channel();
    let mut watcher = notify::recommended_watcher(move |event: notify::Result<notify::Event>| {
        if let Ok(event) = event {
            let _ = tx.send(event);
        }
    })
    .context("cannot create file watcher")?;
    for (path, mode) in watched_paths(scanner) {
        if path.exists() {
            watcher
                .watch(&path, mode)
                .with_context(|| format!("cannot watch {}", path.display()))?;
        }
    }
    let debounce = Duration::from_millis(scanner.config().watch.debounce_ms.max(1));
    run_once(scanner, opts, out);
    loop {
        let Ok(_first) = rx.recv() else { return Ok(0) };
        // Coalesce bursts of events.
        while rx.recv_timeout(debounce).is_ok() {}
        run_once(scanner, opts, out);
    }
}

#[allow(dead_code)]
fn _assert_error_is_send(_: ScanError) {}
