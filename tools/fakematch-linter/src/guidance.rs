//! Versioned repair guidance shown with `--explain` and embedded in reports.

use std::collections::BTreeMap;
use std::path::Path;

use anyhow::{Context, Result, bail};
use serde::{Deserialize, Serialize};

use crate::config::RULE_IDS;
use crate::finding::sha256_hex;

pub const EMBEDDED: &str = include_str!("../guidance/guidance.toml");

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Common {
    pub interpretation: String,
    pub verification: Vec<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Entry {
    pub summary: String,
    pub hint: String,
    pub steps: Vec<String>,
    pub example_condition: String,
    pub before: String,
    pub after: String,
    pub legitimate: String,
    pub avoid: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Guidance {
    pub schema_version: u32,
    pub common: Common,
    pub rules: BTreeMap<String, Entry>,
}

#[derive(Debug, Clone)]
pub struct LoadedGuidance {
    pub guidance: Guidance,
    pub sha256: String,
}

fn nonempty(s: &str) -> bool {
    !s.trim().is_empty()
}

impl Guidance {
    pub fn parse(text: &str) -> Result<Guidance> {
        let guidance: Guidance = toml::from_str(text).context("invalid guidance")?;
        if guidance.schema_version != 1 {
            bail!("guidance requires schema_version=1 and every FM000-FM009 rule");
        }
        let expected: std::collections::BTreeSet<&str> =
            RULE_IDS.iter().copied().chain(["FM000"]).collect();
        let present: std::collections::BTreeSet<&str> = guidance.rules.keys().map(String::as_str).collect();
        if expected != present {
            bail!("guidance requires schema_version=1 and every FM000-FM009 rule");
        }
        if !nonempty(&guidance.common.interpretation)
            || guidance.common.verification.is_empty()
            || !guidance.common.verification.iter().all(|s| nonempty(s))
        {
            bail!("guidance requires common interpretation and verification steps");
        }
        for (rule, e) in &guidance.rules {
            let texts = [&e.summary, &e.hint, &e.example_condition, &e.before, &e.after, &e.legitimate, &e.avoid];
            if !texts.iter().all(|s| nonempty(s)) || e.steps.is_empty() || !e.steps.iter().all(|s| nonempty(s)) {
                bail!("incomplete remediation guidance for {rule}");
            }
        }
        Ok(guidance)
    }

    /// Load from `root/path` when configured, else the embedded default.
    pub fn load(root: &Path, path: Option<&str>) -> Result<LoadedGuidance> {
        let raw = match path {
            Some(p) => std::fs::read(root.join(p)).with_context(|| format!("cannot read guidance {p}"))?,
            None => EMBEDDED.as_bytes().to_vec(),
        };
        let text = String::from_utf8(raw.clone()).context("guidance is not UTF-8")?;
        Ok(LoadedGuidance { guidance: Guidance::parse(&text)?, sha256: sha256_hex(&raw) })
    }

    pub fn explain(&self, rule: &str) -> Option<String> {
        let e = self.rules.get(rule)?;
        let mut lines = vec![
            format!("{rule}: {}", e.summary),
            self.common.interpretation.clone(),
            String::new(),
            "Investigation:".to_string(),
        ];
        lines.extend(e.steps.iter().enumerate().map(|(i, s)| format!("{}. {s}", i + 1)));
        lines.extend([
            String::new(),
            format!("Conditional example: {}", e.example_condition),
            format!("Before:\n{}", e.before),
            format!("After:\n{}", e.after),
            String::new(),
            format!("Legitimate cases: {}", e.legitimate),
            format!("Avoid: {}", e.avoid),
            String::new(),
            "Verification for source changes:".to_string(),
        ]);
        lines.extend(self.common.verification.iter().enumerate().map(|(i, s)| format!("{}. {s}", i + 1)));
        Some(lines.join("\n"))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn embedded_guidance_is_complete() {
        let g = Guidance::parse(EMBEDDED).unwrap();
        for rule in RULE_IDS.iter().chain(["FM000"].iter()) {
            let text = g.explain(rule).unwrap();
            for heading in ["Investigation:", "Conditional example:", "Before:", "After:", "Legitimate cases:", "Avoid:", "Verification for source changes:"] {
                assert!(text.contains(heading), "{rule} lacks {heading}");
            }
            assert!(text.contains("not proof"));
        }
    }

    #[test]
    fn malformed_guidance_refuses() {
        assert!(Guidance::parse("schema_version=1\n[rules]\n").is_err());
        assert!(Guidance::parse(&EMBEDDED.replacen("hint =", "missing_hint =", 1)).is_err());
    }
}
