//! Diagnostics formatting and the JSON report.

use std::collections::BTreeMap;
use std::path::Path;

use serde::Serialize;

use crate::config::{Format, Severity};
use crate::finding::{Finding, ParseRecovery};
use crate::guidance::{Common, Entry, Guidance};

pub const STATUS_COMPLETE: &str = "SCAN_COMPLETE";

#[derive(Debug, Clone, Serialize)]
pub struct ReportGuidance {
    pub schema_version: u32,
    pub common: Common,
    pub rules: BTreeMap<String, Entry>,
}

#[derive(Debug, Clone, Serialize)]
pub struct Report {
    pub schema_version: u32,
    pub status: &'static str,
    pub engine: String,
    pub interpretation: &'static str,
    /// Configured scan paths, joined with ", " (a string for ledger compatibility).
    pub source_scope: String,
    pub source_sha256: BTreeMap<String, String>,
    pub files_scanned: usize,
    pub findings: Vec<Finding>,
    pub unsuppressed: usize,
    pub suppressed: usize,
    pub errors: usize,
    pub warnings: usize,
    pub warnings_as_errors: bool,
    pub remediation_guidance: ReportGuidance,
    pub guidance_sha256: String,
    pub by_rule: BTreeMap<String, usize>,
    /// Active findings per file, most affected first.
    pub by_file: serde_json::Map<String, serde_json::Value>,
    pub rules_selected: Vec<String>,
    pub parse_recovery: Vec<ParseRecovery>,
    pub policy_sha256: String,
    /// Identity of the rule engine: binary version plus configuration bytes.
    pub rules_sha256: String,
    pub postprocessor_config_sha256: BTreeMap<String, String>,
    pub config_sha256: String,
    pub config_path: Option<String>,
    pub elapsed_ms: u128,
}

pub fn engine_name() -> String {
    format!(
        "fakematch-lint {} (tree-sitter-cpp {})",
        env!("CARGO_PKG_VERSION"),
        tree_sitter_cpp_version()
    )
}

fn tree_sitter_cpp_version() -> &'static str {
    "0.23"
}

pub fn posix(path: &Path) -> String {
    path.to_string_lossy().replace('\\', "/")
}

fn github_escape(value: &str, property: bool) -> String {
    let mut v = value.replace('%', "%25").replace('\r', "%0D").replace('\n', "%0A");
    if property {
        v = v.replace(':', "%3A").replace(',', "%2C");
    }
    v
}

/// One console line for a finding in the requested style.
pub fn diagnostic(
    row: &Finding,
    root: &Path,
    style: Format,
    guidance: Option<&Guidance>,
    explain_command: &str,
) -> String {
    let mut detail = row.message.clone();
    if let Some(entry) = guidance.and_then(|g| g.rules.get(&row.rule)) {
        detail.push_str(&format!(
            " Review: {} Conditional example ({}): {} => {} Full steps: {} {}",
            entry.hint, entry.example_condition, entry.before, entry.after, explain_command, row.rule
        ));
    }
    let message = format!("{} [{}] {}", row.rule, row.scope, detail);
    let severity = row.severity;
    match style {
        Format::Github => format!(
            "::{severity} file={},line={},col={},title={}::{}",
            github_escape(&row.path, true),
            row.line,
            row.column,
            row.rule,
            github_escape(&message, false)
        ),
        Format::Problems => {
            let one_line: String = format!("[{}] {}", row.scope, detail)
                .split(['\r', '\n'])
                .filter(|s| !s.is_empty())
                .collect::<Vec<_>>()
                .join(" ");
            format!(
                "{}:{}:{}: {severity} {}: {}",
                posix(&root.join(&row.path)),
                row.line,
                row.column,
                row.rule,
                one_line
            )
        }
        Format::Human | Format::Json => {
            format!("{}:{}:{}: {severity}: {message}", row.path, row.line, row.column)
        }
    }
}

/// The FM000 row printed when a scan cannot complete.
pub fn scanner_failure(path: &str, line: usize, message: &str) -> Finding {
    Finding {
        rule: "FM000".into(),
        path: path.to_string(),
        line,
        column: 1,
        scope: "scanner".into(),
        message: format!("Scan incomplete: {message}"),
        confidence: crate::finding::Confidence::Review,
        excerpt: String::new(),
        fingerprint: String::new(),
        suppressed: false,
        severity: Severity::Error,
        review_reason: None,
        suppression_source: None,
        inline_suppression: None,
        guidance_id: Some("FM000".into()),
        details: BTreeMap::new(),
        offset: 0,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn row() -> Finding {
        let mut f = scanner_failure("src/a,b.c", 3, "x");
        f.rule = "FM001".into();
        f.scope = "f".into();
        f.column = 2;
        f.message = "bad%\n::warning::injection".into();
        f
    }

    #[test]
    fn github_annotation_escapes_untrusted_text() {
        let line = diagnostic(&row(), Path::new("."), Format::Github, None, "x --explain");
        assert!(line.contains("file=src/a%2Cb.c"));
        assert!(line.contains("bad%25%0A::warning::injection"));
        assert!(!line.contains('\n'));
    }

    #[test]
    fn problems_line_is_single_line_and_absolute() {
        let line = diagnostic(&row(), Path::new("W:/My Project"), Format::Problems, None, "x --explain");
        assert!(line.starts_with("W:/My Project/src/a,b.c:3:2: error FM001: [f] bad% ::warning::injection"));
        assert!(!line.contains('\n'));
    }
}
