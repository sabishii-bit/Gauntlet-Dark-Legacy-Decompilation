//! End-to-end CLI behaviour: scope, exit codes, formats, reports, failures.

use std::path::Path;
use std::process::Command;

use tempfile::TempDir;

fn bin() -> Command {
    Command::new(env!("CARGO_BIN_EXE_fakematch-lint"))
}

struct Repo {
    dir: TempDir,
}

impl Repo {
    fn new() -> Repo {
        let dir = TempDir::new().unwrap();
        std::fs::create_dir_all(dir.path().join("src/game")).unwrap();
        std::fs::write(dir.path().join("fakematch.toml"), "schema_version = 1\n[scan]\npaths = [\"src/game\"]\n").unwrap();
        Repo { dir }
    }

    fn root(&self) -> &Path {
        self.dir.path()
    }

    fn write(&self, rel: &str, text: &str) {
        let p = self.root().join(rel);
        std::fs::create_dir_all(p.parent().unwrap()).unwrap();
        std::fs::write(p, text).unwrap();
    }

    fn run(&self, args: &[&str]) -> (i32, String, String) {
        let out = bin().arg("--root").arg(self.root()).args(args).output().unwrap();
        (
            out.status.code().unwrap_or(-1),
            String::from_utf8_lossy(&out.stdout).to_string(),
            String::from_utf8_lossy(&out.stderr).to_string(),
        )
    }

    fn report(&self, rel: &str) -> serde_json::Value {
        serde_json::from_str(&std::fs::read_to_string(self.root().join(rel)).unwrap()).unwrap()
    }
}

#[test]
fn reports_all_rows_even_with_zero_console_limit() {
    let repo = Repo::new();
    repo.write("src/game/a.c", "int f(){return *(int*)(p+0x8);}");
    let args = ["src/game/a.c", "--out", "build/report.json", "--limit", "0"];
    assert_eq!(repo.run(&args).0, 0);
    let (code, _, _) = repo.run(&[&args[..], &["--fail-on-findings"]].concat());
    assert_eq!(code, 1);
    let report = repo.report("build/report.json");
    assert_eq!(report["status"], "SCAN_COMPLETE");
    assert!(report["unsuppressed"].as_u64().unwrap() > 0);
    assert_eq!(report["findings"].as_array().unwrap().len() as u64, report["unsuppressed"].as_u64().unwrap());
    assert_eq!(report["files_scanned"], 1);
    assert!(report["source_sha256"]["src/game/a.c"].is_string());
}

#[test]
fn bad_inputs_and_protected_output_refuse() {
    let repo = Repo::new();
    repo.write("src/game/a.c", "void f() {}");
    for tail in [
        vec!["missing.c"],
        vec!["src/game/a.c", "--out", "fakematch.toml"],
        vec!["src/game/a.c", "--out", "../escape.json"],
        vec!["src/game/a.c", "--out", "build/report.txt"],
        vec!["src/game/a.c", "--rule", "FM999"],
        vec!["src/game/a.c", "--rule", "FM008"],
    ] {
        let (code, _, err) = repo.run(&tail);
        assert_eq!(code, 2, "{tail:?}");
        assert!(err.contains("UNRESOLVED"), "{tail:?}");
    }
    assert_eq!(std::fs::read_to_string(repo.root().join("fakematch.toml")).unwrap().trim(), "schema_version = 1\n[scan]\npaths = [\"src/game\"]".trim());
}

#[test]
fn scope_skips_sdk_and_headers_even_when_named() {
    let repo = Repo::new();
    for rel in ["src/game/a.c", "src/dolphin/os.c", "include/game/a.h"] {
        repo.write(rel, "void f(){use(0x40);asm(\"sync\");}");
    }
    let base = ["--out", "build/report.json", "--format", "problems", "--fail-on-findings"];
    for (inputs, expected) in [
        (vec![], 1),
        (vec!["src", "include"], 1),
        (vec!["src/dolphin/os.c"], 0),
        (vec!["include/game/a.h"], 0),
    ] {
        let (code, _, _) = repo.run(&[&base[..], &inputs[..]].concat());
        assert_eq!(code, expected, "{inputs:?}");
        let report = repo.report("build/report.json");
        assert_eq!(report["files_scanned"], expected);
        assert_eq!(report["source_scope"], "src/game");
        for f in report["findings"].as_array().unwrap() {
            assert!(f["path"].as_str().unwrap().starts_with("src/game/"));
        }
    }
}

#[test]
fn pragmas_are_warnings_and_attributes_are_errors() {
    let repo = Repo::new();
    repo.write(
        "src/game/a.c",
        "#pragma dont_inline on\nvoid f(){}\n#pragma dont_inline off\n#pragma scheduling off\n# pragma opt_propagation off\n#pragma unknown_setting on\n",
    );
    let args = ["src/game/a.c", "--format", "problems", "--fail-on-findings", "--out", "build/report.json"];
    let (code, out, _) = repo.run(&args);
    assert_eq!(code, 0);
    assert!(out.contains(": warning FM006:"));
    let (code, _, _) = repo.run(&[&args[..], &["--warnings-as-errors"]].concat());
    assert_eq!(code, 1);
    let report = repo.report("build/report.json");
    assert_eq!((report["errors"].as_u64(), report["warnings"].as_u64(), report["suppressed"].as_u64()), (Some(0), Some(5), Some(0)));
    let (_, out, _) = repo.run(&["src/game/a.c", "--format", "github", "--limit", "1"]);
    assert!(out.starts_with("::warning "));

    repo.write("src/game/a.c", "void __attribute__((optimize(\"O0\"))) f(){}");
    let (code, _, _) = repo.run(&args);
    assert_eq!(code, 1);
    let report = repo.report("build/report.json");
    assert_eq!((report["errors"].as_u64(), report["warnings"].as_u64()), (Some(1), Some(0)));
    assert_eq!(report["findings"][0]["rule"], "FM006");
}

#[test]
fn editor_diagnostics_cover_every_row_at_limit_zero() {
    let repo = Repo::new();
    repo.write("src/game/a.c", "void f(){use(0x40); use(0x80);}");
    let (code, out, _) = repo.run(&["src/game/a.c", "--format", "problems", "--limit", "0", "--fail-on-findings"]);
    assert_eq!(code, 1);
    assert_eq!(out.matches(": error FM007:").count(), 2);
    let expected_prefix = repo.root().join("src/game/a.c").to_string_lossy().replace('\\', "/");
    assert!(out.contains(&format!("{expected_prefix}:1:")), "{out}");
    assert!(out.contains("--explain FM007"));
}

#[test]
fn stale_comment_failure_points_to_source_in_editor() {
    let repo = Repo::new();
    repo.write("src/game/a.c", "void f(){\n// lint-allow-next-line FM007: stale reason\nuse(1);\n}");
    let (code, out, err) = repo.run(&["--format", "problems"]);
    assert_eq!(code, 2);
    assert!(out.contains("/src/game/a.c:2:1: error FM000:"), "{out}");
    assert!(out.contains("unused rule"));
    assert!(err.contains("UNRESOLVED"));
}

#[test]
fn suppressed_findings_survive_json_but_do_not_fail_ci() {
    let repo = Repo::new();
    repo.write("src/game/a.c", "void f(){\n// lint-allow-next-line FM007: Callee interprets a packed color.\nuse(0x40);\n}");
    let (code, out, _) = repo.run(&[
        "--out", "build/report.json", "--format", "problems", "--fail-on-findings", "--warnings-as-errors",
    ]);
    assert_eq!(code, 0);
    let report = repo.report("build/report.json");
    assert_eq!((report["errors"].as_u64(), report["warnings"].as_u64(), report["suppressed"].as_u64()), (Some(0), Some(0), Some(1)));
    assert_eq!(report["findings"][0]["inline_suppression"]["comment_line"], 2);
    assert!(!out.contains(": error FM007:"));
}

#[test]
fn explain_needs_no_source_and_does_not_scan() {
    let empty = TempDir::new().unwrap();
    let out = bin().arg("--root").arg(empty.path()).args(["--explain", "FM001", "--explain", "FM007", "--explain", "FM001"]).output().unwrap();
    assert_eq!(out.status.code(), Some(0));
    let text = String::from_utf8_lossy(&out.stdout);
    assert!(text.contains("effective BYTE offset"));
    assert!(text.contains("Direct bitwise masks"));
    assert!(!text.contains("SCAN_COMPLETE"));
    assert_eq!(text.matches("FM001: ").count(), 1);
    assert!(std::fs::read_dir(empty.path()).unwrap().next().is_none());
    let out = bin().arg("--root").arg(empty.path()).args(["--explain", "FM001", "--out", "build/report.json"]).output().unwrap();
    assert_eq!(out.status.code(), Some(2));
    let out = bin().arg("--root").arg(empty.path()).args(["--explain", "FM042"]).output().unwrap();
    assert_eq!(out.status.code(), Some(2));
}

#[test]
fn malformed_configuration_is_exit_two() {
    let repo = Repo::new();
    repo.write("fakematch.toml", "schema_version = 1\n[rules.FM001]\nbogus = true\n");
    repo.write("src/game/a.c", "void f(){}");
    let (code, _, err) = repo.run(&["--format", "problems"]);
    assert_eq!(code, 2);
    assert!(err.contains("UNRESOLVED"));
    let (code, _, _) = repo.run(&["--explain", "FM001"]);
    assert_eq!(code, 2);
}

#[test]
fn separate_policy_file_in_legacy_format_is_honoured() {
    let repo = Repo::new();
    repo.write("src/game/a.c", "int f(){return *(int*)(p+8);}");
    repo.write("fakematch.toml", "schema_version = 1\n[scan]\npaths = [\"src/game\"]\n[policy]\nfile = \"policy.toml\"\n");
    repo.write("policy.toml", "schema_version = 1\nexceptions = []\npragma_allowlist = []\n");
    let (code, _, _) = repo.run(&["--out", "build/report.json", "--fail-on-findings"]);
    assert_eq!(code, 1);
    let fingerprint = repo.report("build/report.json")["findings"][0]["fingerprint"].as_str().unwrap().to_string();
    repo.write(
        "policy.toml",
        &format!("schema_version = 1\nexceptions = [{{ fingerprint = \"{fingerprint}\", reason = \"Verified partial view.\" }}]\npragma_allowlist = []\n"),
    );
    let (code, _, _) = repo.run(&["--out", "build/report.json", "--fail-on-findings"]);
    assert_eq!(code, 0);
    let report = repo.report("build/report.json");
    assert_eq!(report["suppressed"], 1);
    assert_eq!(report["findings"][0]["review_reason"], "Verified partial view.");
    repo.write("policy.toml", "schema_version = 1\nexceptions = [{ fingerprint = \"nope\", reason = \"x\" }]\npragma_allowlist = []\n");
    assert_eq!(repo.run(&["--out", "build/report.json"]).0, 2);
}

#[test]
fn postprocessor_inventory_is_scoped_and_not_suppressed() {
    let repo = Repo::new();
    repo.write("src/game/test.c", "void f(){}");
    repo.write(
        "fakematch.toml",
        "schema_version = 1\n[scan]\npaths = [\"src/game\"]\n[postprocessors]\nengines = [{ name = \"WebFrank\", path = \"config/webfrank.json\" }, { name = \"P6Frank\", path = \"config/p6frank.json\" }]\nunit_prefix = \"game/\"\n",
    );
    repo.write(
        "config/webfrank.json",
        "{\n  \"version\": 1,\n  \"units\": {\n    \"game/test\": [\n      {\n        \"function\": \"f\",\n        \"before_sha256\": \"x\"\n      }\n    ],\n    \"dolphin/os\": [\n      {\n        \"function\": \"g\"\n      }\n    ]\n  }\n}\n",
    );
    repo.write("config/p6frank.json", "{\"version\": 1, \"units\": {}}");
    let (code, _, _) = repo.run(&["--postprocessors", "--out", "build/report.json", "--fail-on-findings"]);
    assert_eq!(code, 1);
    let report = repo.report("build/report.json");
    let rows: Vec<&serde_json::Value> = report["findings"].as_array().unwrap().iter().filter(|f| f["rule"] == "FM008").collect();
    assert_eq!(rows.len(), 1);
    assert_eq!(rows[0]["postprocessor"], "WebFrank");
    assert_eq!(rows[0]["unit"], "game/test");
    assert_eq!(rows[0]["source_path"], "src/game/test.c");
    assert_eq!(rows[0]["line"], 6);
    assert_eq!(report["postprocessor_config_sha256"].as_object().unwrap().len(), 2);
    // Without --postprocessors the inventory is not consulted.
    let (code, _, _) = repo.run(&["--out", "build/report.json", "--fail-on-findings"]);
    assert_eq!(code, 0);
}

#[test]
fn json_format_prints_the_report() {
    let repo = Repo::new();
    repo.write("src/game/a.c", "void f(){use(0x40);}");
    let (code, out, _) = repo.run(&["--format", "json"]);
    assert_eq!(code, 0);
    let report: serde_json::Value = serde_json::from_str(&out).unwrap();
    assert_eq!(report["by_rule"]["FM007"], 1);
}

#[test]
fn init_and_list_rules() {
    let dir = TempDir::new().unwrap();
    let out = bin().arg("--root").arg(dir.path()).arg("--init").output().unwrap();
    assert_eq!(out.status.code(), Some(0));
    assert!(dir.path().join("fakematch.toml").is_file());
    let out = bin().arg("--root").arg(dir.path()).arg("--init").output().unwrap();
    assert_eq!(out.status.code(), Some(2));
    let out = bin().arg("--root").arg(dir.path()).arg("--list-rules").output().unwrap();
    let text = String::from_utf8_lossy(&out.stdout);
    assert!(text.contains("FM001") && text.contains("FM009") && text.contains("enabled"));
}

#[test]
fn root_is_discovered_from_a_subdirectory() {
    let repo = Repo::new();
    repo.write("src/game/a.c", "void f(){use(0x40);}");
    let out = bin()
        .current_dir(repo.root().join("src/game"))
        .args(["--format", "json"])
        .output()
        .unwrap();
    assert_eq!(out.status.code(), Some(0));
    let report: serde_json::Value = serde_json::from_str(&String::from_utf8_lossy(&out.stdout)).unwrap();
    assert_eq!(report["files_scanned"], 1);
    assert_eq!(report["findings"][0]["path"], "src/game/a.c");
}

// Exercise real watcher events, not just a fresh process reading a new config.
// The guard reaps the watcher even if an assertion fails.
struct Watching {
    child: std::process::Child,
    lines: std::sync::mpsc::Receiver<String>,
}

impl Watching {
    fn new(repo: &Repo) -> Self {
        use std::io::BufRead;
        let mut child = bin().arg("--root").arg(repo.root())
            .args(["--watch", "--format", "problems"])
            .stdout(std::process::Stdio::piped()).spawn().unwrap();
        let stdout = child.stdout.take().unwrap();
        let (tx, lines) = std::sync::mpsc::channel();
        std::thread::spawn(move || {
            for line in std::io::BufReader::new(stdout).lines() {
                if tx.send(line.unwrap()).is_err() { break; }
            }
        });
        Self { child, lines }
    }

    fn until(&self, expected: &str) -> String {
        let deadline = std::time::Instant::now() + std::time::Duration::from_secs(20);
        let mut batch = String::new();
        loop {
            let timeout = deadline.saturating_duration_since(std::time::Instant::now());
            let line = self.lines.recv_timeout(timeout)
                .unwrap_or_else(|e| panic!("watch did not report {expected}: {e}; last batch: {batch}"));
            if line == "GDL_LINT_BEGIN" { batch.clear(); }
            batch.push_str(&line);
            batch.push('\n');
            if line.starts_with("GDL_LINT_END") && batch.contains(expected) { return batch; }
        }
    }
}

impl Drop for Watching {
    fn drop(&mut self) {
        let _ = self.child.kill();
        let _ = self.child.wait();
    }
}

#[test]
fn watch_reloads_settings_and_recovers_from_invalid_edits() {
    let repo = Repo::new();
    repo.write("src/game/a.c", "void f(){use(0x40);}");
    let watch = Watching::new(&repo);
    watch.until(": error FM007:");
    repo.write("fakematch.toml", "schema_version=1\n[rules.FM007]\nseverity='warning'\n");
    let batch = watch.until(": warning FM007:");
    assert!(!batch.contains(": error FM007:"));
    repo.write("fakematch.toml", "[broken");
    let batch = watch.until("GDL_LINT_END status=2");
    assert!(batch.contains(": error FM000:"));
    assert!(!batch.contains("SCAN_COMPLETE"));
    repo.write("fakematch.toml", "schema_version=1\n[rules.FM007]\nenabled=false\n");
    let batch = watch.until("0 review candidates");
    assert!(!batch.contains(": warning FM007:"));
    assert!(!batch.contains(": error FM007:"));
}

#[test]
fn watch_reloads_external_policy_without_source_edits() {
    let repo = Repo::new();
    repo.write("src/game/a.c", "void f(){use(0x40);}");
    repo.write("fakematch.toml", "schema_version=1\n[policy]\nfile='policy/review.toml'\n");
    repo.write("policy/review.toml", "schema_version=1\nexceptions=[]\npragma_allowlist=[]\n");
    assert_eq!(repo.run(&["--out", "build/baseline.json"]).0, 0);
    let fingerprint = repo.report("build/baseline.json")["findings"][0]["fingerprint"].as_str().unwrap().to_owned();
    let watch = Watching::new(&repo);
    watch.until(": error FM007:");
    repo.write("policy/review.toml", &format!("schema_version=1\nexceptions=[{{fingerprint='{fingerprint}',reason='Verified API constant.'}}]\npragma_allowlist=[]\n"));
    let batch = watch.until("1 reviewed exceptions");
    assert!(batch.contains("0 review candidates"));
    assert!(!batch.contains(": error FM007:"));
}
