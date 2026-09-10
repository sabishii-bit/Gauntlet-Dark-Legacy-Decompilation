//! Findings and the report rows they become.

use std::collections::BTreeMap;

use serde::{Deserialize, Serialize};
use sha2::{Digest, Sha256};

use crate::config::Severity;

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "kebab-case")]
pub enum Confidence {
    /// A structural match that deserves review.
    Review,
    /// A shape/lexical heuristic; false positives are expected.
    Heuristic,
    /// Configured build dependency, not a source construct.
    ConfiguredDependency,
}

/// Metadata bound to a finding by a reasoned source comment.
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct InlineSuppression {
    pub reason: String,
    pub comment_line: usize,
    pub target_start_line: usize,
    pub target_end_line: usize,
    pub target_kind: String,
    pub target_sha256: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub end_comment_line: Option<usize>,
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct Finding {
    pub rule: String,
    pub path: String,
    pub line: usize,
    pub column: usize,
    pub scope: String,
    pub message: String,
    pub confidence: Confidence,
    pub excerpt: String,
    pub fingerprint: String,
    pub suppressed: bool,
    pub severity: Severity,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub review_reason: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub suppression_source: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub inline_suppression: Option<InlineSuppression>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub guidance_id: Option<String>,
    /// Rule-specific details (variable, depth, directive, base, offset, ...).
    #[serde(flatten)]
    pub details: BTreeMap<String, serde_json::Value>,
    /// Byte offset of the finding in its file; internal only.
    #[serde(skip)]
    pub offset: usize,
}

impl Finding {
    pub fn detail_str(&self, key: &str) -> Option<&str> {
        self.details.get(key).and_then(|v| v.as_str())
    }

    pub fn detail_u64(&self, key: &str) -> Option<u64> {
        self.details.get(key).and_then(|v| v.as_u64())
    }

    /// Whether this FM006 finding is a `#pragma` (warning) rather than an attribute.
    pub fn is_pragma(&self) -> bool {
        self.rule == "FM006"
            && self
                .detail_str("directive")
                .is_some_and(|d| {
                    let t = d.trim_start_matches('#').trim_start();
                    t.starts_with("pragma")
                        && !t[6..].starts_with(|c: char| c.is_alphanumeric() || c == '_')
                })
    }
}

pub fn sha256_hex(bytes: &[u8]) -> String {
    let digest = Sha256::digest(bytes);
    let mut out = String::with_capacity(64);
    for b in digest {
        out.push_str(&format!("{b:02x}"));
    }
    out
}

/// Parser recovery region; absence of findings inside it is not evidence.
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct ParseRecovery {
    pub path: String,
    pub line: usize,
    pub projection: bool,
    pub excerpt: String,
}
