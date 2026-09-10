//! FM008: configured legacy postprocessor dependencies.
//!
//! Reports inventory entries, NOT a claim that a rule executed in this build.

use std::collections::{BTreeMap, HashMap, HashSet};
use std::path::Path;

use anyhow::{Context, Result, bail};
use regex::Regex;

use crate::config::{PostprocessorConfig, Severity};
use crate::finding::{Confidence, Finding, sha256_hex};

#[derive(serde::Deserialize)]
struct Inventory {
    version: u32,
    units: serde_json::Map<String, serde_json::Value>,
}

/// Returns FM008 findings and the hashes of the inspected configuration.
pub fn findings(
    root: &Path,
    config: &PostprocessorConfig,
    source_names: &HashSet<String>,
    include_all: bool,
) -> Result<(Vec<Finding>, BTreeMap<String, String>)> {
    let mut out = Vec::new();
    let mut hashes = BTreeMap::new();
    let any_present = config.engines.iter().any(|e| root.join(&e.path).exists());
    if !any_present {
        // Retirement is explicit, not a missing-config fallback.
        if let Some(guard) = &config.native_guard {
            let bytes = std::fs::read(root.join(guard))
                .with_context(|| format!("native-only guard missing: {guard}"))?;
            hashes.insert(guard.clone(), sha256_hex(&bytes));
        }
        return Ok((out, hashes));
    }
    let function_line = Regex::new(r#"(?m)^[ \t]*"function"\s*:\s*("(?:[^"\\]|\\.)*")"#).unwrap();
    for engine in &config.engines {
        let path = root.join(&engine.path);
        if !path.exists() {
            continue;
        }
        let raw = std::fs::read(&path)?;
        hashes.insert(engine.path.clone(), sha256_hex(&raw));
        let text = String::from_utf8(raw).context("postprocessor inventory is not UTF-8")?;
        let inventory: Inventory = serde_json::from_str(&text)
            .with_context(|| format!("invalid postprocessor inventory: {}", engine.path))?;
        if inventory.version != 1 {
            bail!("invalid postprocessor inventory: {}", engine.path);
        }
        let mut locations: HashMap<String, Vec<usize>> = HashMap::new();
        for m in function_line.captures_iter(&text) {
            let name: String = serde_json::from_str(&m[1]).unwrap_or_default();
            let line = text[..m.get(0).unwrap().start()].matches('\n').count() + 1;
            locations.entry(name).or_default().push(line);
        }
        for (unit, rules) in &inventory.units {
            if unit.contains("..") || unit.contains(':') || unit.contains('\\') {
                bail!("invalid postprocessor unit path");
            }
            if !unit.starts_with(&config.unit_prefix) {
                continue;
            }
            let rules: Vec<&serde_json::Value> = match rules {
                serde_json::Value::Object(_) => vec![rules],
                serde_json::Value::Array(items) => items.iter().collect(),
                _ => bail!("invalid postprocessor rule list"),
            };
            let selected = [".c", ".cpp", ".cc"]
                .iter()
                .map(|ext| format!("{}{}{}", config.source_prefix, unit, ext))
                .find(|candidate| source_names.contains(candidate));
            for rule in rules {
                let name = rule.get("function").and_then(|v| v.as_str()).unwrap_or("");
                let line = match locations.get_mut(name) {
                    Some(lines) if !name.is_empty() && !lines.is_empty() => lines.remove(0),
                    _ => bail!("missing function/location in {}", engine.path),
                };
                if selected.is_none() && !include_all {
                    continue;
                }
                let identity = format!("{}\0{}\0{}", engine.name, unit, name);
                let mut details = BTreeMap::new();
                details.insert("unit".into(), serde_json::json!(unit));
                details.insert("function".into(), serde_json::json!(name));
                details.insert("source_path".into(), serde_json::json!(selected));
                details.insert("postprocessor".into(), serde_json::json!(engine.name));
                out.push(Finding {
                    rule: "FM008".into(),
                    path: engine.path.clone(),
                    line,
                    column: 1,
                    scope: name.to_string(),
                    message: format!(
                        "{} configuration reintroduced: {}::{}. Native-only policy forbids production postprocessing; coordinate removal and any required TU demotion, not guard bypass.",
                        engine.name, unit, name
                    ),
                    confidence: Confidence::ConfiguredDependency,
                    excerpt: name.to_string(),
                    fingerprint: sha256_hex(identity.as_bytes()),
                    suppressed: false,
                    severity: Severity::Error,
                    review_reason: None,
                    suppression_source: None,
                    inline_suppression: None,
                    guidance_id: None,
                    details,
                    offset: 0,
                });
            }
        }
    }
    Ok((out, hashes))
}
