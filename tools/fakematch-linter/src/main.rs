use std::io::Write;
use std::path::{Path, PathBuf};
use std::process::ExitCode;

use anyhow::{Context, Result, anyhow, bail};
use clap::Parser;

use fakematch_lint::config::{self, Config, EXAMPLE_CONFIG, Format, Policy, RULE_IDS};
use fakematch_lint::guidance::Guidance;
use fakematch_lint::scan::{Options, ScanError, Scanner};
use fakematch_lint::{find_root, print_failure, watch};

/// Review-only C/C++ reconstruction-debt scanner; never rewrites source.
///
/// Findings are review candidates, not proof of fakematching. Exit 0: scan
/// complete; 1: errors with --fail-on-findings, or warnings with
/// --warnings-as-errors; 2: missing input, bad configuration or scanner error.
#[derive(Parser, Debug)]
#[command(name = "fakematch-lint", version, about, long_about = None)]
struct Cli {
    /// Files/directories to scan (repository-relative). Default: the configured
    /// scan paths. Inputs outside the configured scope are skipped.
    paths: Vec<String>,

    /// Repository root. Default: the nearest ancestor of the current directory
    /// holding the configuration file, else the current directory.
    #[arg(long, value_name = "DIR")]
    root: Option<PathBuf>,

    /// Configuration file (repository-relative). Default: fakematch.toml.
    #[arg(long, value_name = "FILE")]
    config: Option<PathBuf>,

    /// Write the JSON report here (under the configured report directory).
    #[arg(long, value_name = "FILE")]
    out: Option<PathBuf>,

    /// Console rows only; the JSON report retains every finding.
    #[arg(long, value_name = "N")]
    limit: Option<usize>,

    /// Exit 1 when unsuppressed errors remain.
    #[arg(long)]
    fail_on_findings: bool,

    /// Also exit 1 on unsuppressed warnings.
    #[arg(long)]
    warnings_as_errors: bool,

    /// Console output style.
    #[arg(long, value_enum)]
    format: Option<Format>,

    /// Include configured postprocessor dependencies (FM008).
    #[arg(long)]
    postprocessors: bool,

    /// Editor task: rescan on saved changes, reusing unchanged results.
    #[arg(long)]
    watch: bool,

    /// Restrict the report to these rules (repeatable).
    #[arg(long, value_name = "FMnnn")]
    rule: Vec<String>,

    /// Show repair guidance for a rule and exit (repeatable); no scan.
    #[arg(long, value_name = "FMnnn")]
    explain: Vec<String>,

    /// List rules with their titles and enabled state, then exit.
    #[arg(long)]
    list_rules: bool,

    /// Write a documented default configuration file and exit.
    #[arg(long)]
    init: bool,

    /// Worker threads (default: all cores).
    #[arg(long, value_name = "N")]
    jobs: Option<usize>,

    /// Print the parse tree (S-expression) of one file and exit; a rule
    /// development aid, not a scan.
    #[arg(long, value_name = "FILE")]
    dump_ast: Option<PathBuf>,
}

fn main() -> ExitCode {
    let cli = Cli::parse();
    match run(cli) {
        Ok(code) => ExitCode::from(code as u8),
        Err(e) => {
            eprintln!("UNRESOLVED: {e:#}");
            ExitCode::from(2)
        }
    }
}

fn run(cli: Cli) -> Result<i32> {
    if let Some(jobs) = cli.jobs {
        rayon::ThreadPoolBuilder::new().num_threads(jobs.max(1)).build_global().ok();
    }
    let config_name = cli
        .config
        .as_deref()
        .map(|p| p.to_string_lossy().to_string())
        .unwrap_or_else(|| config::DEFAULT_CONFIG_NAME.to_string());
    let cwd = std::env::current_dir().context("current directory")?;
    let root = match &cli.root {
        Some(r) => std::path::absolute(r).context("--root")?,
        None => find_root(&cwd, &config_name),
    };
    if !root.is_dir() {
        bail!("root is not a directory: {}", root.display());
    }

    if let Some(file) = &cli.dump_ast {
        let text = std::fs::read_to_string(file).with_context(|| format!("cannot read {}", file.display()))?;
        let tree = fakematch_lint::parse::parse(&text);
        println!("{}", tree.root_node().to_sexp());
        return Ok(0);
    }

    if cli.init {
        let target = root.join(&config_name);
        if target.exists() {
            bail!("refusing to overwrite existing {}", target.display());
        }
        std::fs::write(&target, EXAMPLE_CONFIG)?;
        println!("Wrote {}", target.display());
        return Ok(0);
    }

    let loaded = Config::load(&root, cli.config.as_deref())?;
    let config = &loaded.config;
    let mut stdout = std::io::stdout().lock();

    if cli.list_rules {
        for id in RULE_IDS {
            let enabled = if id == "FM008" {
                "with --postprocessors".to_string()
            } else if config.rules.enabled(id) {
                "enabled".to_string()
            } else {
                "disabled".to_string()
            };
            writeln!(stdout, "{id}  {:<64} {enabled}", config::rule_title(id))?;
        }
        return Ok(0);
    }

    let guidance = Guidance::load(&root, config.guidance.path.as_deref())?;
    if !cli.explain.is_empty() {
        let scan_options = !cli.paths.is_empty()
            || cli.out.is_some()
            || !cli.rule.is_empty()
            || cli.postprocessors
            || cli.watch
            || cli.format.is_some_and(|f| f != Format::Human);
        if scan_options {
            bail!("--explain is a standalone text lookup; do not combine with scan/output options");
        }
        let mut seen = std::collections::BTreeSet::new();
        for rule in &cli.explain {
            if !seen.insert(rule.clone()) {
                continue;
            }
            let text = guidance
                .guidance
                .explain(rule)
                .ok_or_else(|| anyhow!("unknown rule {rule}; choose FM000 or FM001-FM009"))?;
            writeln!(stdout, "{text}")?;
        }
        return Ok(0);
    }

    let policy = Policy::load(&root, &loaded)?;
    let format = cli.format.unwrap_or(config.output.format);
    let opts = Options {
        paths: cli.paths.clone(),
        rules: if cli.rule.is_empty() { None } else { Some(cli.rule.clone()) },
        postprocessors: cli.postprocessors,
        fail_on_findings: cli.fail_on_findings,
        warnings_as_errors: cli.warnings_as_errors,
        format,
        out: cli.out.clone(),
        limit: cli.limit.unwrap_or(config.output.limit),
    };
    let scanner = Scanner::new(root, loaded, policy, guidance)?;
    if cli.watch {
        return watch::watch(&scanner, &opts, &mut stdout);
    }
    match scanner.run(&opts, &mut stdout) {
        Ok(outcome) => Ok(outcome.exit_code),
        Err(e) => {
            print_failure(&scanner, &e, format, &mut stdout);
            let _ = stdout.flush();
            let _ = ScanError::Other as fn(_) -> _;
            let _ = Path::new(".");
            Ok(2)
        }
    }
}
