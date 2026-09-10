//! Review-only C/C++ reconstruction-debt scanner; never rewrites source.
//!
//! Source files are tokenised and parsed in-process with tree-sitter's C++
//! grammar. Rules report review candidates; they do not prove fakematching.
//! Macros are scanned in a separate offset-preserving projection, not
//! expanded. Parse recovery is reported, not treated as proof of clean source.
//! No type/CFG analysis or original-source authenticity proof is implied.
//!
//! Exit codes: 0 scan complete; 1 errors with `--fail-on-findings` or warnings
//! with `--warnings-as-errors`; 2 missing input, bad configuration or an
//! invalid suppression comment.

pub mod analysis;
pub mod config;
pub mod finding;
pub mod guidance;
pub mod lexer;
pub mod parse;
pub mod policy;
pub mod postproc;
pub mod report;
pub mod rules;
pub mod scan;
pub mod suppress;
pub mod watch;

use std::io::Write;
use std::path::Path;

use config::Format;
use scan::{ScanError, Scanner};

/// Convenience: scan one text with the given configuration (tests, tooling).
pub fn scan_source(text: &str, path: &str, config: &config::Config) -> Result<scan::FileResult, ScanError> {
    let rules = rules::build(config);
    scan::scan_text(path, text, &rules, config)
}

/// Print the FM000 diagnostic for a failed scan in the requested format,
/// followed by the `UNRESOLVED` line on stderr.
pub fn print_failure(scanner: &Scanner, error: &ScanError, format: Format, out: &mut dyn Write) {
    let (path, line) = match error {
        ScanError::Suppression(e) => (e.path.clone(), e.line),
        ScanError::Other(_) => (
            scanner
                .loaded
                .path
                .as_ref()
                .and_then(|p| p.strip_prefix(&scanner.root).ok())
                .map(report::posix)
                .unwrap_or_else(|| config::DEFAULT_CONFIG_NAME.to_string()),
            1,
        ),
    };
    if format != Format::Human {
        let row = report::scanner_failure(&path, line, &error.to_string());
        let line = report::diagnostic(
            &row,
            &scanner.root,
            if format == Format::Json { Format::Human } else { format },
            Some(&scanner.guidance.guidance),
            &scanner.config().output.explain_command,
        );
        let _ = writeln!(out, "{line}");
    }
    eprintln!("UNRESOLVED: {error}");
}

/// Find the repository root: the nearest ancestor of `start` containing the
/// configuration file, else `start` itself.
pub fn find_root(start: &Path, config_name: &str) -> std::path::PathBuf {
    let mut cur = Some(start);
    while let Some(dir) = cur {
        if dir.join(config_name).is_file() {
            return dir.to_path_buf();
        }
        cur = dir.parent();
    }
    start.to_path_buf()
}
