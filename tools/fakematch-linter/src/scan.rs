//! Scan orchestration: file discovery, parallel per-file analysis, caching,
//! policy application and report assembly.

use std::collections::{BTreeMap, HashMap, HashSet};
use std::fmt;
use std::io::Write;
use std::path::{Path, PathBuf};
use std::sync::Mutex;
use std::time::Instant;

use anyhow::{Context, anyhow};
use globset::{GlobBuilder, GlobSet, GlobSetBuilder};
use rayon::prelude::*;

use crate::analysis::{Analysis, Parsed, Sink};
use crate::config::{Config, Format, INLINE_RULES, LoadedConfig, Policy, RULE_IDS, Severity};
use crate::finding::{Finding, ParseRecovery, sha256_hex};
use crate::guidance::{Guidance, LoadedGuidance};
use crate::lexer::Lexed;
use crate::policy;
use crate::postproc;
use crate::report::{self, Report, ReportGuidance, STATUS_COMPLETE};
use crate::rules::{self, Rule};
use crate::suppress::{self, SuppressionError};

#[derive(Debug)]
pub enum ScanError {
    Suppression(SuppressionError),
    Other(anyhow::Error),
}

impl fmt::Display for ScanError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            ScanError::Suppression(e) => write!(f, "{e}"),
            ScanError::Other(e) => write!(f, "{e:#}"),
        }
    }
}

impl std::error::Error for ScanError {}

impl From<anyhow::Error> for ScanError {
    fn from(e: anyhow::Error) -> Self {
        ScanError::Other(e)
    }
}

impl From<SuppressionError> for ScanError {
    fn from(e: SuppressionError) -> Self {
        ScanError::Suppression(e)
    }
}

/// Findings and parser-recovery regions for one file, before policy.
#[derive(Debug, Clone, Default)]
pub struct FileResult {
    pub findings: Vec<Finding>,
    pub recovery: Vec<ParseRecovery>,
}

/// Analyse one source text. Pure: no files are touched.
pub fn scan_text(path: &str, text: &str, rules: &[Box<dyn Rule>], config: &Config) -> Result<FileResult, ScanError> {
    if text.trim().is_empty() {
        return Ok(FileResult::default());
    }
    let lexed = Lexed::new(text);
    let parsed = Parsed::new(text, &lexed);
    let analysis = Analysis::new(path, text, &lexed, &parsed);
    let mut sink = Sink::new(&analysis);
    for rule in rules {
        rule.check(&analysis, &mut sink);
    }
    let mut findings = order(sink.findings);
    suppress::annotate(&analysis, &mut findings, &config.rules.fm005.keywords, &config.suppressions)?;
    Ok(FileResult { findings, recovery: analysis.recovery.clone() })
}

/// Deduplicate by (rule, line, column, variable), sort by position, and make
/// repeated identities unique by ordinal.
fn order(findings: Vec<Finding>) -> Vec<Finding> {
    let mut unique: BTreeMap<(String, usize, usize, Option<String>), Finding> = BTreeMap::new();
    for f in findings {
        let key = (f.rule.clone(), f.line, f.column, f.detail_str("variable").map(str::to_string));
        unique.insert(key, f);
    }
    let mut ordered: Vec<Finding> = unique.into_values().collect();
    ordered.sort_by(|a, b| (&a.path, a.line, a.column, &a.rule).cmp(&(&b.path, b.line, b.column, &b.rule)));
    let mut seen: HashMap<String, usize> = HashMap::new();
    for f in &mut ordered {
        let ordinal = seen.entry(f.fingerprint.clone()).or_insert(0);
        let base = f.fingerprint.clone();
        f.fingerprint = sha256_hex(format!("{base}:{ordinal}").as_bytes());
        *ordinal += 1;
    }
    ordered
}

#[derive(Debug, Clone)]
pub struct Options {
    pub paths: Vec<String>,
    pub rules: Option<Vec<String>>,
    pub postprocessors: bool,
    pub fail_on_findings: bool,
    pub warnings_as_errors: bool,
    pub format: Format,
    pub out: Option<PathBuf>,
    pub limit: usize,
}

struct CacheEntry {
    key: (String, String),
    result: FileResult,
}

/// A configured scanner that can be reused across watch iterations.
pub struct Scanner {
    pub root: PathBuf,
    pub loaded: LoadedConfig,
    pub policy: Policy,
    pub guidance: LoadedGuidance,
    rules: Vec<Box<dyn Rule>>,
    engine_hash: String,
    cache: Mutex<HashMap<String, CacheEntry>>,
    excludes: GlobSet,
}

pub struct Outcome {
    pub exit_code: i32,
    pub report: Report,
}

fn canonical(path: &Path) -> PathBuf {
    std::fs::canonicalize(path).unwrap_or_else(|_| path.to_path_buf())
}

impl Scanner {
    pub fn new(root: PathBuf, loaded: LoadedConfig, policy: Policy, guidance: LoadedGuidance) -> anyhow::Result<Scanner> {
        let rules = rules::build(&loaded.config);
        let engine_hash = sha256_hex(&[report::engine_name().as_bytes(), &loaded.raw].concat());
        let mut builder = GlobSetBuilder::new();
        for g in &loaded.config.scan.exclude {
            builder.add(GlobBuilder::new(g).literal_separator(false).build()?);
        }
        Ok(Scanner {
            root,
            loaded,
            policy,
            guidance,
            rules,
            engine_hash,
            cache: Mutex::new(HashMap::new()),
            excludes: builder.build()?,
        })
    }

    pub fn config(&self) -> &Config {
        &self.loaded.config
    }

    /// Reload watch-mode inputs atomically. Preserve the source cache when
    /// nothing changed; never silently scan with stale settings after a bad edit.
    pub fn refresh(&mut self) -> anyhow::Result<()> {
        let loaded = Config::load(&self.root, self.loaded.path.as_deref())?;
        let policy = Policy::load(&self.root, &loaded)?;
        let guidance = Guidance::load(&self.root, loaded.config.guidance.path.as_deref())?;
        if loaded.raw != self.loaded.raw || loaded.path != self.loaded.path
            || policy.raw != self.policy.raw || guidance.sha256 != self.guidance.sha256
        {
            *self = Scanner::new(self.root.clone(), loaded, policy, guidance)?;
        }
        Ok(())
    }

    pub fn rules(&self) -> &[Box<dyn Rule>] {
        &self.rules
    }

    fn relative(&self, path: &Path) -> Option<String> {
        path.strip_prefix(&self.root).ok().map(report::posix)
    }

    fn in_scope(&self, file: &Path) -> bool {
        let root_c = canonical(&self.root);
        let file_c = canonical(file);
        self.config().scan.paths.iter().any(|p| file_c.starts_with(canonical(&self.root.join(p))))
            && file_c.starts_with(&root_c)
    }

    /// Resolve the input list to in-scope source files.
    pub fn discover(&self, inputs: &[String]) -> anyhow::Result<Vec<PathBuf>> {
        let config = self.config();
        let extensions = config.extension_set();
        let has_ext = |p: &Path| {
            p.extension()
                .map(|e| format!(".{}", e.to_string_lossy().to_ascii_lowercase()))
                .is_some_and(|e| extensions.contains(&e))
        };
        let root_c = canonical(&self.root);
        let mut files = HashSet::new();
        let defaults = config.scan.paths.clone();
        let inputs: Vec<&String> = if inputs.is_empty() { defaults.iter().collect() } else { inputs.iter().collect() };
        for value in inputs {
            let path = self.root.join(value);
            let path_c = canonical(&path);
            if !path.exists() || !path_c.starts_with(&root_c) {
                if inputs_are_defaults(value, &defaults) && !path.exists() {
                    continue;
                }
                return Err(anyhow!("missing or outside-root input: {value}"));
            }
            if path.is_file() && !has_ext(&path) {
                return Err(anyhow!("not a C/C++ source input: {value}"));
            }
            let walker = walkdir::WalkDir::new(&path).follow_links(true).sort_by_file_name();
            for entry in walker {
                let entry = entry?;
                if !entry.file_type().is_file() || !has_ext(entry.path()) {
                    continue;
                }
                let entry_c = canonical(entry.path());
                if !entry_c.starts_with(&root_c) {
                    return Err(anyhow!("source symlink escapes repository: {}", entry.path().display()));
                }
                let Some(rel) = self.relative(entry.path()) else { continue };
                if self.excludes.is_match(&rel) {
                    continue;
                }
                if self.in_scope(entry.path()) {
                    files.insert(entry.path().to_path_buf());
                }
            }
        }
        let mut files: Vec<PathBuf> = files.into_iter().collect();
        files.sort();
        Ok(files)
    }

    /// Scan one file, reusing the cached result when its bytes and the engine
    /// configuration are unchanged.
    fn scan_file(&self, path: &Path) -> Result<(String, String, FileResult), ScanError> {
        let name = self.relative(path).ok_or_else(|| anyhow!("input outside root: {}", path.display()))?;
        let data = std::fs::read(path).with_context(|| format!("cannot read {name}"))?;
        let hash = sha256_hex(&data);
        let key = (hash.clone(), self.engine_hash.clone());
        if let Some(entry) = self.cache.lock().unwrap().get(&name)
            && entry.key == key
        {
            return Ok((name, hash, entry.result.clone()));
        }
        let text = match String::from_utf8(data) {
            Ok(t) => t,
            Err(e) => e.into_bytes().iter().map(|&b| b as char).collect(),
        };
        let result = match scan_text(&name, &text, &self.rules, self.config()) {
            Ok(r) => r,
            Err(ScanError::Suppression(e)) => return Err(ScanError::Suppression(e)),
            Err(ScanError::Other(e)) => return Err(ScanError::Other(anyhow!("{name}: {e:#}"))),
        };
        self.cache.lock().unwrap().insert(name.clone(), CacheEntry { key, result: result.clone() });
        Ok((name, hash, result))
    }

    /// Run a complete scan, print diagnostics to `out`, and return the outcome.
    pub fn run(&self, opts: &Options, out: &mut dyn Write) -> Result<Outcome, ScanError> {
        let started = Instant::now();
        let root = &self.root;
        let config = self.config();
        let report_path = match &opts.out {
            Some(o) => {
                let full = if o.is_absolute() { o.clone() } else { root.join(o) };
                let report_dir = canonical(&root.join(&config.output.report_dir));
                let parent = canonical(full.parent().unwrap_or(root));
                let protected: Vec<PathBuf> = self
                    .loaded
                    .path
                    .iter()
                    .cloned()
                    .chain(config.policy.file.iter().map(|p| root.join(p)))
                    .map(|p| canonical(&p))
                    .collect();
                let under_report_dir = parent.starts_with(&report_dir)
                    || full.parent().is_some_and(|p| p.starts_with(root.join(&config.output.report_dir)));
                if !under_report_dir
                    || full.extension().is_none_or(|e| e != "json")
                    || protected.iter().any(|p| *p == canonical(&full))
                {
                    return Err(anyhow!(
                        "--out must be a report .json under {}/, not the configuration or policy",
                        config.output.report_dir
                    )
                    .into());
                }
                Some(full)
            }
            None => None,
        };
        if let Some(rules) = &opts.rules {
            for r in rules {
                if !RULE_IDS.contains(&r.as_str()) {
                    return Err(anyhow!("unknown rule {r}").into());
                }
            }
            if rules.iter().any(|r| r == "FM008") && !opts.postprocessors {
                return Err(anyhow!("FM008 requires --postprocessors").into());
            }
        }
        let files = self.discover(&opts.paths)?;
        let results: Vec<Result<(String, String, FileResult), ScanError>> =
            files.par_iter().map(|f| self.scan_file(f)).collect();
        let mut rows = Vec::new();
        let mut recovery = Vec::new();
        let mut hashes = BTreeMap::new();
        for r in results {
            let (name, hash, result) = r?;
            hashes.insert(name, hash);
            rows.extend(result.findings);
            recovery.extend(result.recovery);
        }
        for (name, sha) in &hashes {
            let now = std::fs::read(root.join(name)).map(|d| sha256_hex(&d)).unwrap_or_default();
            if now != *sha {
                return Err(anyhow!("source changed during scan; rerun on stable inputs").into());
            }
        }
        if let Some(p) = &config.policy.file
            && std::fs::read(root.join(p)).ok().as_deref() != Some(self.policy.raw.as_slice())
        {
            return Err(anyhow!("policy changed during scan").into());
        }
        policy::apply(&mut rows, &self.policy);

        let mut post_hashes = BTreeMap::new();
        if opts.postprocessors && !files.is_empty() {
            let names: HashSet<String> = hashes.keys().cloned().collect();
            let (dependencies, h) = postproc::findings(root, &config.postprocessors, &names, opts.paths.is_empty())?;
            post_hashes = h;
            let pinned: HashMap<(String, String), &Finding> = dependencies
                .iter()
                .filter_map(|d| d.detail_str("source_path").map(|p| ((p.to_string(), d.scope.clone()), d)))
                .collect();
            for row in rows.iter_mut() {
                if let Some(dep) = pinned.get(&(row.path.clone(), row.scope.clone())) {
                    let mut m = serde_json::Map::new();
                    for k in ["postprocessor", "unit", "function"] {
                        m.insert(k.into(), dep.details.get(k).cloned().unwrap_or(serde_json::Value::Null));
                    }
                    row.details.insert("postprocessor_dependency".into(), serde_json::Value::Object(m));
                }
            }
            let mut all = dependencies;
            all.append(&mut rows);
            rows = all;
        }
        let selected: Vec<String> = match &opts.rules {
            Some(r) => {
                let mut s: Vec<String> = r.iter().cloned().collect::<HashSet<_>>().into_iter().collect();
                s.sort();
                s
            }
            None => RULE_IDS
                .iter()
                .filter(|r| (**r != "FM008" || opts.postprocessors) && config.rules.enabled(r))
                .map(|r| r.to_string())
                .collect(),
        };
        let mut rows: Vec<Finding> = rows
            .into_iter()
            .filter(|r| selected.contains(&r.rule))
            .map(|mut r| {
                r.guidance_id = Some(r.rule.clone());
                r
            })
            .collect();
        let _ = INLINE_RULES;
        let active_count = rows.iter().filter(|r| !r.suppressed).count();
        let warnings = rows.iter().filter(|r| !r.suppressed && r.severity == Severity::Warning).count();
        let errors = active_count - warnings;
        let mut by_rule = BTreeMap::new();
        let mut by_file: HashMap<String, usize> = HashMap::new();
        for r in rows.iter().filter(|r| !r.suppressed) {
            *by_rule.entry(r.rule.clone()).or_insert(0) += 1;
            *by_file.entry(r.path.clone()).or_insert(0) += 1;
        }
        let mut by_file: Vec<(String, usize)> = by_file.into_iter().collect();
        by_file.sort_by(|a, b| b.1.cmp(&a.1).then_with(|| a.0.cmp(&b.0)));
        let by_file: serde_json::Map<String, serde_json::Value> =
            by_file.into_iter().map(|(k, v)| (k, serde_json::json!(v))).collect();
        let guidance = &self.guidance.guidance;
        let report = Report {
            schema_version: 1,
            status: STATUS_COMPLETE,
            engine: report::engine_name(),
            interpretation: "Review candidates, not proven fakematches. Parse recovery limits coverage; macros are not expanded.",
            source_scope: config.scan.paths.join(", "),
            source_sha256: hashes,
            files_scanned: files.len(),
            findings: std::mem::take(&mut rows),
            unsuppressed: active_count,
            suppressed: 0,
            errors,
            warnings,
            warnings_as_errors: opts.warnings_as_errors,
            remediation_guidance: ReportGuidance {
                schema_version: guidance.schema_version,
                common: guidance.common.clone(),
                rules: selected
                    .iter()
                    .filter_map(|r| guidance.rules.get(r).map(|e| (r.clone(), e.clone())))
                    .collect(),
            },
            guidance_sha256: self.guidance.sha256.clone(),
            by_rule,
            by_file,
            rules_selected: selected,
            parse_recovery: recovery,
            policy_sha256: sha256_hex(&self.policy.raw),
            rules_sha256: self.engine_hash.clone(),
            postprocessor_config_sha256: post_hashes,
            config_sha256: sha256_hex(&self.loaded.raw),
            config_path: self.loaded.path.as_ref().and_then(|p| self.relative(p)),
            elapsed_ms: 0,
        };
        let mut report = report;
        report.suppressed = report.findings.len() - report.unsuppressed;
        report.elapsed_ms = started.elapsed().as_millis();
        if let Some(path) = &report_path {
            if let Some(parent) = path.parent() {
                std::fs::create_dir_all(parent).with_context(|| format!("cannot create {}", parent.display()))?;
            }
            let json = serde_json::to_string_pretty(&report).context("serialising report")?;
            std::fs::write(path, json + "\n").with_context(|| format!("cannot write {}", path.display()))?;
        }
        self.print(&report, opts, out, report_path.as_deref(), guidance).map_err(|e| anyhow!(e))?;
        let exit_code = if (opts.fail_on_findings && errors > 0) || (opts.warnings_as_errors && warnings > 0) {
            1
        } else {
            0
        };
        Ok(Outcome { exit_code, report })
    }

    fn print(
        &self,
        report: &Report,
        opts: &Options,
        out: &mut dyn Write,
        report_path: Option<&Path>,
        guidance: &Guidance,
    ) -> std::io::Result<()> {
        if opts.format == Format::Json {
            writeln!(out, "{}", serde_json::to_string_pretty(report).unwrap_or_default())?;
            return Ok(());
        }
        let explain = &self.config().output.explain_command;
        let active: Vec<&Finding> = report.findings.iter().filter(|r| !r.suppressed).collect();
        let shown: &[&Finding] = if opts.format == Format::Problems {
            &active
        } else {
            &active[..active.len().min(opts.limit)]
        };
        for r in shown {
            writeln!(out, "{}", report::diagnostic(r, &self.root, opts.format, Some(guidance), explain))?;
        }
        writeln!(
            out,
            "{STATUS_COMPLETE}: {} files; {} review candidates; {} reviewed exceptions.",
            report.files_scanned,
            active.len(),
            report.suppressed
        )?;
        writeln!(
            out,
            "Diagnostics: {} errors; {} warnings; warnings-as-errors={}.",
            report.errors, report.warnings, report.warnings_as_errors
        )?;
        writeln!(out, "By rule: {}", serde_json::to_string(&report.by_rule).unwrap_or_default())?;
        writeln!(
            out,
            "Parser recovery regions: {} (includes macro projection; not a clean-code certificate).",
            report.parse_recovery.len()
        )?;
        if active.len() > opts.limit && opts.format != Format::Problems {
            writeln!(out, "Console limited to {}; use --out for all findings.", opts.limit)?;
        }
        if let Some(p) = report_path {
            writeln!(out, "Report: {}", report::posix(p.strip_prefix(&self.root).unwrap_or(p)))?;
        }
        writeln!(out, "Elapsed: {} ms.", report.elapsed_ms)?;
        Ok(())
    }
}

fn inputs_are_defaults(value: &str, defaults: &[String]) -> bool {
    defaults.iter().any(|d| d == value)
}
