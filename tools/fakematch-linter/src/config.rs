//! Linter configuration (`fakematch.toml`).
//!
//! Everything that used to be a hard-coded constant in the old scanner lives
//! here: the scanned scope, per-rule switches and options, the suppression
//! comment grammar, the review policy, the postprocessor inventory, and the
//! output/watch settings.

use std::collections::BTreeSet;
use std::fmt;
use std::path::{Path, PathBuf};

use anyhow::{Context, Result, bail};
use regex::Regex;
use serde::{Deserialize, Serialize};

pub const DEFAULT_CONFIG_NAME: &str = "fakematch.toml";

/// Every source rule identifier, in report order.
pub const RULE_IDS: [&str; 9] = [
    "FM001", "FM002", "FM003", "FM004", "FM005", "FM006", "FM007", "FM008", "FM009",
];

/// Rules that an inline comment may waive. Scanner failures (FM000) and
/// configured postprocessor dependencies (FM008) are never waivable.
pub const INLINE_RULES: [&str; 8] = [
    "FM001", "FM002", "FM003", "FM004", "FM005", "FM006", "FM007", "FM009",
];

pub fn rule_title(id: &str) -> &'static str {
    match id {
        "FM000" => "Scanner did not complete",
        "FM001" => "Raw offset or indexed pointer cast",
        "FM002" => "Nested dereference through pointer casts",
        "FM003" => "Possible stack/allocation scaffolding",
        "FM004" => "Float/address-shaped numeric byte array",
        "FM005" => "Assembly without an explicit reviewed exception",
        "FM006" => "Source-level compilation override",
        "FM007" => "Unnamed hexadecimal expression constant",
        "FM008" => "Configured postprocessor dependency requiring native retirement",
        "FM009" => "Unnamed constant-offset pointer or array access",
        _ => "Unknown rule",
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Serialize, Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum Severity {
    Warning,
    Error,
}

impl fmt::Display for Severity {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(match self {
            Severity::Warning => "warning",
            Severity::Error => "error",
        })
    }
}

#[derive(Debug, Clone, Deserialize)]
#[serde(deny_unknown_fields, default)]
pub struct Config {
    pub schema_version: u32,
    pub scan: ScanConfig,
    pub rules: RulesConfig,
    pub suppressions: SuppressionConfig,
    pub policy: PolicyConfig,
    pub postprocessors: PostprocessorConfig,
    pub output: OutputConfig,
    pub guidance: GuidanceConfig,
    pub watch: WatchConfig,
}

impl Default for Config {
    fn default() -> Self {
        Config {
            schema_version: 1,
            scan: ScanConfig::default(),
            rules: RulesConfig::default(),
            suppressions: SuppressionConfig::default(),
            policy: PolicyConfig::default(),
            postprocessors: PostprocessorConfig::default(),
            output: OutputConfig::default(),
            guidance: GuidanceConfig::default(),
            watch: WatchConfig::default(),
        }
    }
}

#[derive(Debug, Clone, Deserialize)]
#[serde(deny_unknown_fields, default)]
pub struct ScanConfig {
    /// Repository-relative directories (or files) that are in scope. Inputs
    /// outside these paths are skipped even when named explicitly.
    pub paths: Vec<String>,
    /// Lower-case file extensions that count as C/C++ sources.
    pub extensions: Vec<String>,
    /// Glob patterns (repository-relative) that are excluded from scanning.
    pub exclude: Vec<String>,
}

impl Default for ScanConfig {
    fn default() -> Self {
        ScanConfig {
            paths: vec!["src".into()],
            extensions: [".c", ".cpp", ".cc", ".cxx", ".h", ".hpp", ".hh", ".hxx"]
                .iter()
                .map(|s| s.to_string())
                .collect(),
            exclude: Vec::new(),
        }
    }
}

#[derive(Debug, Clone, Deserialize)]
#[serde(deny_unknown_fields, default)]
#[derive(Default)]
pub struct RulesConfig {
    #[serde(rename = "FM001")]
    pub fm001: Fm001Config,
    #[serde(rename = "FM002")]
    pub fm002: Fm002Config,
    #[serde(rename = "FM003")]
    pub fm003: Fm003Config,
    #[serde(rename = "FM004")]
    pub fm004: Fm004Config,
    #[serde(rename = "FM005")]
    pub fm005: Fm005Config,
    #[serde(rename = "FM006")]
    pub fm006: Fm006Config,
    #[serde(rename = "FM007")]
    pub fm007: Fm007Config,
    #[serde(rename = "FM009")]
    pub fm009: Fm009Config,
}


impl RulesConfig {
    pub fn enabled(&self, id: &str) -> bool {
        match id {
            "FM001" => self.fm001.enabled,
            "FM002" => self.fm002.enabled,
            "FM003" => self.fm003.enabled,
            "FM004" => self.fm004.enabled,
            "FM005" => self.fm005.enabled,
            "FM006" => self.fm006.enabled,
            "FM007" => self.fm007.enabled,
            "FM009" => self.fm009.enabled,
            "FM008" => true,
            _ => false,
        }
    }
}

fn yes() -> bool {
    true
}

fn error() -> Severity {
    Severity::Error
}

#[derive(Debug, Clone, Deserialize)]
#[serde(deny_unknown_fields, default)]
pub struct Fm001Config {
    pub enabled: bool,
    pub severity: Severity,
}

impl Default for Fm001Config {
    fn default() -> Self {
        Fm001Config { enabled: yes(), severity: error() }
    }
}

#[derive(Debug, Clone, Deserialize)]
#[serde(deny_unknown_fields, default)]
pub struct Fm002Config {
    pub enabled: bool,
    pub severity: Severity,
    /// Minimum number of cast-dereference layers before reporting.
    pub min_depth: usize,
}

impl Default for Fm002Config {
    fn default() -> Self {
        Fm002Config { enabled: yes(), severity: error(), min_depth: 2 }
    }
}

#[derive(Debug, Clone, Deserialize)]
#[serde(deny_unknown_fields, default)]
pub struct Fm003Config {
    pub enabled: bool,
    pub severity: Severity,
    /// Report local arrays with no lexical use beyond their declaration.
    pub unused_arrays: bool,
    /// Report `volatile` locals.
    pub volatile_locals: bool,
    /// Report locals whose names match `trash_pattern`.
    pub trash_names: bool,
    /// Report locals that are declared and assigned/incremented but never read.
    pub write_only_locals: bool,
    /// Case-insensitive regex, matched against the whole local name.
    pub trash_pattern: String,
}

impl Default for Fm003Config {
    fn default() -> Self {
        Fm003Config {
            enabled: yes(),
            severity: error(),
            unused_arrays: true,
            volatile_locals: true,
            trash_names: true,
            write_only_locals: true,
            trash_pattern: r"(?i)^(?:trash\w*|unused\w*|(?:stack|frame)_?pad\w*|pad(?:ding)?(?:[0-9_]\w*)?)$".into(),
        }
    }
}

#[derive(Debug, Clone, Deserialize)]
#[serde(deny_unknown_fields, default)]
pub struct Fm004Config {
    pub enabled: bool,
    pub severity: Severity,
    /// Element type spellings that count as byte arrays.
    pub byte_types: Vec<String>,
    /// Minimum number of bytes before the shape heuristics run.
    pub min_bytes: usize,
    /// Half-open address range [start, end) for address-shaped words.
    pub address_range: [u64; 2],
    /// Minimum number of address-shaped words.
    pub min_address_words: usize,
    /// Fraction of decoded floats that must look plausible.
    pub float_ratio: f64,
    /// Plausible float magnitude range [min, max].
    pub float_magnitude: [f64; 2],
}

impl Default for Fm004Config {
    fn default() -> Self {
        Fm004Config {
            enabled: yes(),
            severity: error(),
            byte_types: vec!["u8".into(), "uint8_t".into(), "unsigned char".into()],
            min_bytes: 8,
            address_range: [0x8000_0000, 0x8180_0000],
            min_address_words: 3,
            float_ratio: 0.75,
            float_magnitude: [1e-8, 1e8],
        }
    }
}

#[derive(Debug, Clone, Deserialize)]
#[serde(deny_unknown_fields, default)]
pub struct Fm005Config {
    pub enabled: bool,
    pub severity: Severity,
    /// Identifiers that introduce assembly.
    pub keywords: Vec<String>,
}

impl Default for Fm005Config {
    fn default() -> Self {
        Fm005Config {
            enabled: yes(),
            severity: error(),
            keywords: vec!["asm".into(), "__asm".into(), "__asm__".into(), "ASM".into()],
        }
    }
}

#[derive(Debug, Clone, Deserialize)]
#[serde(deny_unknown_fields, default)]
pub struct Fm006Config {
    pub enabled: bool,
    /// Severity for optimisation attributes.
    pub severity: Severity,
    /// Severity for `#pragma` directives.
    pub pragma_severity: Severity,
    /// Regexes matched against the normalised directive text; matches are ignored.
    pub ignored_pragmas: Vec<String>,
    /// Attribute names that count as compilation overrides.
    pub attributes: Vec<String>,
}

impl Default for Fm006Config {
    fn default() -> Self {
        Fm006Config {
            enabled: yes(),
            severity: error(),
            pragma_severity: Severity::Warning,
            ignored_pragmas: vec![r"^#\s*pragma\s+once\s*$".into()],
            attributes: ["optimize", "optnone", "target", "noinline", "always_inline"]
                .iter()
                .map(|s| s.to_string())
                .collect(),
        }
    }
}

#[derive(Debug, Clone, Deserialize)]
#[serde(deny_unknown_fields, default)]
pub struct Fm007Config {
    pub enabled: bool,
    pub severity: Severity,
    /// Skip literals that are a direct operand of `& | ^ ~` or `&= |= ^=`.
    pub ignore_masks: bool,
    /// Skip literals inside `enum` bodies.
    pub ignore_enums: bool,
    /// Skip literals inside `const`/`constexpr` declarations.
    pub ignore_const_declarations: bool,
    /// Skip object-like macro bodies (`#define SIZE 0x20`).
    pub ignore_object_macros: bool,
}

impl Default for Fm007Config {
    fn default() -> Self {
        Fm007Config {
            enabled: yes(),
            severity: error(),
            ignore_masks: true,
            ignore_enums: true,
            ignore_const_declarations: true,
            ignore_object_macros: true,
        }
    }
}

#[derive(Debug, Clone, Deserialize)]
#[serde(deny_unknown_fields, default)]
pub struct Fm009Config {
    pub enabled: bool,
    pub severity: Severity,
    /// Only report when the base identifier has a visible, unambiguous
    /// pointer/array declaration.
    pub require_visible_declaration: bool,
    /// Regex for the numeric offset operand.
    pub offset_pattern: String,
}

impl Default for Fm009Config {
    fn default() -> Self {
        Fm009Config {
            enabled: yes(),
            severity: error(),
            require_visible_declaration: true,
            offset_pattern: r"^\(*\s*(?:0[xX][0-9a-fA-F]+|[0-9]+)[uUlL]*\s*\)*$".into(),
        }
    }
}

#[derive(Debug, Clone, Deserialize)]
#[serde(deny_unknown_fields, default)]
pub struct SuppressionConfig {
    /// Comment prefix; `lint-` recognises `lint-allow-next-line`, `lint-begin`,
    /// `lint-end` and `lint-file`.
    pub prefix: String,
    /// Prefixes that are recognised only to be rejected (retired spellings).
    pub rejected_prefixes: Vec<String>,
}

impl Default for SuppressionConfig {
    fn default() -> Self {
        SuppressionConfig {
            prefix: "lint-".into(),
            rejected_prefixes: vec!["gdl-lint-".into()],
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(deny_unknown_fields)]
pub struct Exception {
    pub fingerprint: String,
    pub reason: String,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(deny_unknown_fields)]
pub struct PragmaApproval {
    pub path: String,
    pub scope: String,
    pub directive: String,
    pub count: u64,
    pub reason: String,
}

#[derive(Debug, Clone, Deserialize, Default)]
#[serde(deny_unknown_fields, default)]
pub struct PolicyConfig {
    /// Optional separate policy file (TOML or JSON) holding `exceptions` and
    /// `pragma_allowlist`. When set, the inline lists must be empty.
    pub file: Option<String>,
    pub exceptions: Vec<Exception>,
    pub pragma_allowlist: Vec<PragmaApproval>,
}

#[derive(Debug, Clone, Deserialize)]
#[serde(deny_unknown_fields, default)]
#[derive(Default)]
pub struct PostprocessorEngine {
    pub name: String,
    pub path: String,
}


#[derive(Debug, Clone, Deserialize)]
#[serde(deny_unknown_fields, default)]
pub struct PostprocessorConfig {
    /// Legacy postprocessor inventories to report as FM008 when present.
    pub engines: Vec<PostprocessorEngine>,
    /// File whose hash is recorded when no inventory exists (the native-only guard).
    pub native_guard: Option<String>,
    /// Only inventory units with this prefix are in scope.
    pub unit_prefix: String,
    /// Prefix that maps an inventory unit onto a source path.
    pub source_prefix: String,
}

impl Default for PostprocessorConfig {
    fn default() -> Self {
        PostprocessorConfig {
            engines: Vec::new(),
            native_guard: None,
            unit_prefix: String::new(),
            source_prefix: "src/".into(),
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Deserialize, clap::ValueEnum)]
#[serde(rename_all = "lowercase")]
pub enum Format {
    Human,
    Github,
    Problems,
    Json,
}

#[derive(Debug, Clone, Deserialize)]
#[serde(deny_unknown_fields, default)]
pub struct OutputConfig {
    pub format: Format,
    /// Console row cap for human/github output; JSON keeps every finding.
    pub limit: usize,
    /// Directory (repository-relative) that `--out` reports must live under.
    pub report_dir: String,
    /// Command shown in diagnostics for full guidance.
    pub explain_command: String,
    /// Watch-mode markers consumed by the editor's problem matcher.
    pub begin_marker: String,
    pub end_marker: String,
}

impl Default for OutputConfig {
    fn default() -> Self {
        OutputConfig {
            format: Format::Human,
            limit: 40,
            report_dir: "build".into(),
            explain_command: "fakematch-lint --explain".into(),
            begin_marker: "GDL_LINT_BEGIN".into(),
            end_marker: "GDL_LINT_END".into(),
        }
    }
}

#[derive(Debug, Clone, Deserialize, Default)]
#[serde(deny_unknown_fields, default)]
pub struct GuidanceConfig {
    /// Repository-relative guidance TOML; the embedded default is used when unset.
    pub path: Option<String>,
}

#[derive(Debug, Clone, Deserialize)]
#[serde(deny_unknown_fields, default)]
pub struct WatchConfig {
    /// Quiet period after the last file event before rescanning.
    pub debounce_ms: u64,
    /// Additional repository-relative paths whose changes trigger a rescan.
    pub extra_paths: Vec<String>,
}

impl Default for WatchConfig {
    fn default() -> Self {
        WatchConfig { debounce_ms: 250, extra_paths: Vec::new() }
    }
}

/// A loaded configuration plus provenance for the report.
#[derive(Debug, Clone)]
pub struct LoadedConfig {
    pub config: Config,
    /// Path the configuration came from, if a file existed.
    pub path: Option<PathBuf>,
    /// Raw bytes of the configuration file (empty when defaults were used).
    pub raw: Vec<u8>,
}

impl Config {
    pub fn from_toml(text: &str) -> Result<Config> {
        let config: Config = toml::from_str(text).context("invalid configuration")?;
        config.validate()?;
        Ok(config)
    }

    /// Load `root/path`; a missing file at the default name yields defaults.
    pub fn load(root: &Path, path: Option<&Path>) -> Result<LoadedConfig> {
        let explicit = path.is_some();
        let path = root.join(path.unwrap_or(Path::new(DEFAULT_CONFIG_NAME)));
        match std::fs::read(&path) {
            Ok(raw) => {
                let text = String::from_utf8(raw.clone()).context("configuration is not UTF-8")?;
                let config = Config::from_toml(&text)
                    .with_context(|| format!("{}", path.display()))?;
                Ok(LoadedConfig { config, path: Some(path), raw })
            }
            Err(e) if e.kind() == std::io::ErrorKind::NotFound && !explicit => {
                Ok(LoadedConfig { config: Config::default(), path: None, raw: Vec::new() })
            }
            Err(e) => Err(e).with_context(|| format!("cannot read {}", path.display())),
        }
    }

    pub fn validate(&self) -> Result<()> {
        if self.schema_version != 1 {
            bail!("configuration requires schema_version = 1");
        }
        if self.scan.paths.is_empty() {
            bail!("scan.paths must name at least one directory");
        }
        for p in &self.scan.paths {
            check_relative(p, "scan.paths")?;
        }
        for e in &self.scan.extensions {
            if !e.starts_with('.') {
                bail!("scan.extensions entries must start with a dot: {e}");
            }
        }
        for g in &self.scan.exclude {
            globset::Glob::new(g).with_context(|| format!("scan.exclude pattern {g}"))?;
        }
        Regex::new(&self.rules.fm003.trash_pattern).context("rules.FM003.trash_pattern")?;
        for p in &self.rules.fm006.ignored_pragmas {
            Regex::new(p).context("rules.FM006.ignored_pragmas")?;
        }
        Regex::new(&self.rules.fm009.offset_pattern).context("rules.FM009.offset_pattern")?;
        if self.rules.fm005.keywords.is_empty() {
            bail!("rules.FM005.keywords must not be empty");
        }
        if self.rules.fm004.address_range[0] >= self.rules.fm004.address_range[1] {
            bail!("rules.FM004.address_range must be [start, end) with start < end");
        }
        if !(0.0..=1.0).contains(&self.rules.fm004.float_ratio) {
            bail!("rules.FM004.float_ratio must be between 0 and 1");
        }
        if self.suppressions.prefix.trim().is_empty() {
            bail!("suppressions.prefix must not be empty");
        }
        if self.policy.file.is_some()
            && (!self.policy.exceptions.is_empty() || !self.policy.pragma_allowlist.is_empty())
        {
            bail!("policy.file and inline policy lists are mutually exclusive");
        }
        if let Some(p) = &self.policy.file {
            check_relative(p, "policy.file")?;
        }
        validate_policy_lists(&self.policy.exceptions, &self.policy.pragma_allowlist)?;
        for engine in &self.postprocessors.engines {
            if engine.name.trim().is_empty() || engine.path.trim().is_empty() {
                bail!("postprocessors.engines entries need a name and a path");
            }
            check_relative(&engine.path, "postprocessors.engines.path")?;
        }
        if let Some(g) = &self.postprocessors.native_guard {
            check_relative(g, "postprocessors.native_guard")?;
        }
        if let Some(g) = &self.guidance.path {
            check_relative(g, "guidance.path")?;
        }
        check_relative(&self.output.report_dir, "output.report_dir")?;
        if self.output.begin_marker.trim().is_empty() || self.output.end_marker.trim().is_empty() {
            bail!("output markers must not be empty");
        }
        Ok(())
    }

    pub fn extension_set(&self) -> BTreeSet<String> {
        self.scan.extensions.iter().map(|e| e.to_ascii_lowercase()).collect()
    }
}

pub fn check_relative(value: &str, field: &str) -> Result<()> {
    let p = Path::new(value);
    if value.trim().is_empty()
        || p.is_absolute()
        || value.contains('\\')
        || value.contains(':')
        || p.components().any(|c| matches!(c, std::path::Component::ParentDir))
    {
        bail!("{field} must be an exact repository-relative POSIX path: {value:?}");
    }
    Ok(())
}

/// Validate exception and pragma-approval lists the way the review policy
/// requires: exact fingerprints, nonempty reasons, no duplicates, no globs.
pub fn validate_policy_lists(exceptions: &[Exception], approvals: &[PragmaApproval]) -> Result<()> {
    let hex = Regex::new("^[0-9a-f]{64}$").unwrap();
    let mut seen = BTreeSet::new();
    for e in exceptions {
        if !hex.is_match(&e.fingerprint) || e.reason.trim().is_empty() {
            bail!("exception requires exact fingerprint and nonempty reason");
        }
        if !seen.insert(e.fingerprint.clone()) {
            bail!("duplicate exception fingerprint");
        }
    }
    let mut seen = BTreeSet::new();
    for a in approvals {
        if [&a.path, &a.scope, &a.directive, &a.reason]
            .iter()
            .any(|s| s.trim().is_empty())
            || a.count < 1
        {
            bail!("pragma approval requires exact path/scope/directive, positive count, reason");
        }
        if a.path.contains(['*', '?', '\\', ':'])
            || Path::new(&a.path).is_absolute()
            || Path::new(&a.path)
                .components()
                .any(|c| matches!(c, std::path::Component::ParentDir))
        {
            bail!("pragma path must be exact repository-relative POSIX path");
        }
        if !seen.insert((a.path.clone(), a.scope.clone(), a.directive.clone())) {
            bail!("duplicate pragma approval");
        }
    }
    Ok(())
}

/// The review policy actually applied to a scan.
#[derive(Debug, Clone, Default)]
pub struct Policy {
    pub exceptions: Vec<Exception>,
    pub pragma_allowlist: Vec<PragmaApproval>,
    /// Raw bytes the policy was loaded from (config file or policy file).
    pub raw: Vec<u8>,
}

#[derive(Debug, Deserialize)]
#[serde(deny_unknown_fields)]
struct PolicyFile {
    schema_version: u32,
    exceptions: Vec<Exception>,
    pragma_allowlist: Vec<PragmaApproval>,
    /// Retired selector accepted for old policy files; ignored.
    #[serde(default)]
    warning_pragmas: Option<Vec<String>>,
}

impl Policy {
    pub fn load(root: &Path, loaded: &LoadedConfig) -> Result<Policy> {
        match &loaded.config.policy.file {
            Some(rel) => {
                let path = root.join(rel);
                let raw = std::fs::read(&path)
                    .with_context(|| format!("cannot read policy {}", path.display()))?;
                let text = String::from_utf8(raw.clone()).context("policy is not UTF-8")?;
                let file: PolicyFile = if path.extension().is_some_and(|e| e == "json") {
                    serde_json::from_str(&text).context("invalid JSON policy")?
                } else {
                    toml::from_str(&text).context("invalid TOML policy")?
                };
                if file.schema_version != 1 {
                    bail!("policy requires schema_version=1, exceptions and pragma_allowlist");
                }
                if let Some(w) = &file.warning_pragmas {
                    let mut seen = BTreeSet::new();
                    for d in w {
                        if !matches!(d.as_str(), "#pragma dont_inline on" | "#pragma dont_inline off")
                            || !seen.insert(d)
                        {
                            bail!("warning_pragmas must contain distinct approved dont_inline directives");
                        }
                    }
                }
                validate_policy_lists(&file.exceptions, &file.pragma_allowlist)?;
                Ok(Policy { exceptions: file.exceptions, pragma_allowlist: file.pragma_allowlist, raw })
            }
            None => Ok(Policy {
                exceptions: loaded.config.policy.exceptions.clone(),
                pragma_allowlist: loaded.config.policy.pragma_allowlist.clone(),
                raw: loaded.raw.clone(),
            }),
        }
    }
}

/// A documented default configuration, written by `--init`.
pub const EXAMPLE_CONFIG: &str = include_str!("../fakematch.example.toml");

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn example_config_parses_and_validates() {
        let config = Config::from_toml(EXAMPLE_CONFIG).unwrap();
        assert_eq!(config.schema_version, 1);
        assert!(config.rules.fm001.enabled);
    }

    #[test]
    fn defaults_validate() {
        Config::default().validate().unwrap();
    }

    #[test]
    fn unknown_keys_and_bad_patterns_refuse() {
        assert!(Config::from_toml("schema_version = 1\n[rules.FM001]\nbogus = 1\n").is_err());
        assert!(Config::from_toml("schema_version = 1\n[rules.FM003]\ntrash_pattern = '('\n").is_err());
        assert!(Config::from_toml("schema_version = 2\n").is_err());
    }

    #[test]
    fn policy_lists_refuse_globs_and_blank_reasons() {
        let bad = PragmaApproval {
            path: "src/*".into(),
            scope: "f".into(),
            directive: "#pragma optimize off".into(),
            count: 1,
            reason: "review".into(),
        };
        assert!(validate_policy_lists(&[], std::slice::from_ref(&bad)).is_err());
        let blank = PragmaApproval { path: "src/game/a.c".into(), reason: "".into(), ..bad };
        assert!(validate_policy_lists(&[], &[blank]).is_err());
        let ex = Exception { fingerprint: "x".into(), reason: "r".into() };
        assert!(validate_policy_lists(&[ex], &[]).is_err());
    }
}
