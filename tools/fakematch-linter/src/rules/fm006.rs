//! FM006: source-level compilation override.
//!
//! `#pragma` directives (except configured exclusions such as `#pragma once`)
//! are reported as warnings; optimisation attributes are errors. Pragmas are
//! found lexically so parser recovery cannot hide them.

use regex::Regex;

use crate::analysis::{Analysis, Sink, details};
use crate::config::{Fm006Config, Severity};
use crate::finding::Confidence;
use crate::lexer::collapse_whitespace;
use crate::parse::node_text;

use super::Rule;

pub const MESSAGE: &str = "Source pragma/optimization override; investigate scope and original source context. Pragmas warn; attributes remain errors under the reporting policy.";

pub struct Fm006 {
    severity: Severity,
    pragma_severity: Severity,
    ignored: Vec<Regex>,
    attributes: Regex,
}

impl Fm006 {
    pub fn new(config: &Fm006Config) -> Self {
        let names = config
            .attributes
            .iter()
            .map(|s| regex::escape(s))
            .collect::<Vec<_>>()
            .join("|");
        Fm006 {
            severity: config.severity,
            pragma_severity: config.pragma_severity,
            ignored: config
                .ignored_pragmas
                .iter()
                .map(|p| Regex::new(p).expect("validated by config"))
                .collect(),
            attributes: Regex::new(&format!(r"\b(?:{names})\b")).expect("escaped names"),
        }
    }
}

impl Rule for Fm006 {
    fn id(&self) -> &'static str {
        "FM006"
    }

    fn title(&self) -> &'static str {
        "Source-level compilation override"
    }

    fn check(&self, a: &Analysis<'_, '_>, sink: &mut Sink<'_, '_, '_>) {
        let scope_of = |start: usize, end: usize| -> String {
            let owner = a.owner(start);
            if owner != "<file>" {
                return owner.to_string();
            }
            let following = a
                .functions
                .iter()
                .find(|f| f.start >= end)
                .map(|f| f.name.as_str())
                .unwrap_or("<end>");
            format!("before:{following}")
        };
        for d in &a.lex.directives {
            if !d.is_pragma() || self.ignored.iter().any(|re| re.is_match(&d.text)) {
                continue;
            }
            let scope = scope_of(d.hash, d.end);
            sink.emit(
                "FM006",
                d.hash,
                d.end,
                MESSAGE,
                Confidence::Review,
                self.pragma_severity,
                details([
                    ("directive", serde_json::json!(d.text)),
                    ("pragma_scope", serde_json::json!(scope)),
                ]),
            );
        }
        if self.attributes.as_str() == r"\b(?:)\b" {
            return;
        }
        for &n in &a.source {
            if n.kind() != "attribute_specifier" {
                continue;
            }
            let text = node_text(n, a.text);
            if !self.attributes.is_match(text) {
                continue;
            }
            let (x, y) = (n.start_byte(), n.end_byte());
            let directive = a
                .lex
                .directive_at(x)
                .map(|d| d.text.clone())
                .unwrap_or_else(|| collapse_whitespace(text));
            let scope = scope_of(x, y);
            sink.emit(
                "FM006",
                x,
                y,
                MESSAGE,
                Confidence::Review,
                self.severity,
                details([
                    ("directive", serde_json::json!(directive)),
                    ("pragma_scope", serde_json::json!(scope)),
                ]),
            );
        }
    }
}
