//! Reason-required inline suppression comments.
//!
//! ```c
//! // lint-allow-next-line FM007: <reason>     one following declaration/statement
//! // lint-begin FM005, FM007: <reason>        explicit region ...
//! // lint-end FM005, FM007                    ... closed with the same rules
//! // lint-file FM005: <reason>                before all code and directives
//! ```
//!
//! AST ranges cover ordinary declarations and statements. A balanced-token
//! fallback covers MWCC assembly, which the parser recovers imperfectly; that
//! fallback can waive only FM005. Nothing here verifies that a reason is true.

use std::collections::BTreeSet;
use std::fmt;

use regex::Regex;

use crate::analysis::Analysis;
use crate::config::{INLINE_RULES, SuppressionConfig};
use crate::finding::{Finding, InlineSuppression, sha256_hex};
use crate::lexer::TokKind;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct SuppressionError {
    pub path: String,
    pub line: usize,
    pub message: String,
}

impl fmt::Display for SuppressionError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}:{}: invalid inline suppression: {}", self.path, self.line, self.message)
    }
}

impl std::error::Error for SuppressionError {}

struct Annotation {
    selected: Vec<usize>,
    metadata: InlineSuppression,
}

/// Bind reasoned comments to findings. On any invalid comment the whole file
/// is refused and no partial waivers are applied.
pub fn annotate(
    analysis: &Analysis<'_, '_>,
    findings: &mut [Finding],
    asm_keywords: &[String],
    config: &SuppressionConfig,
) -> Result<(), SuppressionError> {
    let text = analysis.text;
    let lex = analysis.lex;
    let path = analysis.path;
    let prefix = config.prefix.as_str();
    let grammar = Regex::new(&format!(
        r"^({p}allow-next-line|{p}begin|{p}end|{p}file)\s+(FM\d{{3}}(?:\s*,\s*FM\d{{3}})*)(?:\s*:\s*(\S(?:.*\S)?))?$",
        p = regex::escape(prefix)
    ))
    .expect("static grammar");
    let mut annotations: Vec<Annotation> = Vec::new();
    let mut region: Option<(usize, Vec<String>, String, usize)> = None;

    let refuse = |line: usize, message: &str| SuppressionError {
        path: path.to_string(),
        line,
        message: message.to_string(),
    };

    let mut collect = |a: usize,
                       b: usize,
                       kind: &str,
                       rules: &[String],
                       reason: &str,
                       line: usize,
                       end_line: Option<usize>|
     -> Result<(), SuppressionError> {
        let selected: Vec<usize> = findings
            .iter()
            .enumerate()
            .filter(|(_, row)| rules.contains(&row.rule) && a <= row.offset && row.offset < b)
            .map(|(i, _)| i)
            .collect();
        let covered: BTreeSet<&str> = selected.iter().map(|&i| findings[i].rule.as_str()).collect();
        let unused: Vec<&str> = rules
            .iter()
            .map(String::as_str)
            .filter(|r| !covered.contains(r))
            .collect();
        if !unused.is_empty() {
            return Err(refuse(
                line,
                &format!("unused rule(s) for this source scope: {}", unused.join(", ")),
            ));
        }
        if annotations.iter().any(|prior| prior.selected.iter().any(|i| selected.contains(i))) {
            return Err(refuse(line, "overlapping suppression comments for the same finding"));
        }
        annotations.push(Annotation {
            selected,
            metadata: InlineSuppression {
                reason: reason.to_string(),
                comment_line: line,
                target_start_line: lex.line_of(a),
                target_end_line: lex.line_of(a.max(b.saturating_sub(1))),
                target_kind: kind.to_string(),
                target_sha256: sha256_hex(&text.as_bytes()[a..b]),
                end_comment_line: end_line,
            },
        });
        Ok(())
    };

    for comment in lex.all_tokens.iter().filter(|t| t.kind == TokKind::Comment) {
        let raw = comment.text(text).trim_end_matches('\r');
        let body = if let Some(b) = raw.strip_prefix("//") {
            b
        } else {
            raw[2..raw.len().saturating_sub(2).max(2)].trim_end_matches("*/")
        }
        .trim();
        let recognised = body.starts_with(prefix)
            || config.rejected_prefixes.iter().any(|p| body.starts_with(p.as_str()));
        if !recognised {
            continue;
        }
        let line = lex.line_of(comment.start);
        if raw.contains('\n') || raw.contains('\r') || raw.trim_end().ends_with('\\') {
            return Err(refuse(line, "use a standalone, single-line comment without a continuation"));
        }
        let line_end = text[comment.end..].find('\n').map(|i| comment.end + i).unwrap_or(text.len());
        let line_start = lex.line_start(line);
        if !text[line_start..comment.start].trim().is_empty() || !text[comment.end..line_end].trim().is_empty() {
            return Err(refuse(line, "place the comment on its own line immediately before the target"));
        }
        let Some(cap) = grammar.captures(body) else {
            return Err(refuse(
                line,
                &format!(
                    "expected {p}allow-next-line, {p}begin or {p}file FM001[,FM002]: reason; close regions with {p}end FM001[,FM002]",
                    p = prefix
                ),
            ));
        };
        let command = cap[1].strip_prefix(prefix).unwrap_or(&cap[1]).to_string();
        let rules: Vec<String> = cap[2].split(',').map(|s| s.trim().to_string()).collect();
        let reason = cap.get(3).map(|m| m.as_str().to_string());
        let distinct: BTreeSet<&String> = rules.iter().collect();
        if distinct.len() != rules.len() || rules.iter().any(|r| !INLINE_RULES.contains(&r.as_str())) {
            return Err(refuse(
                line,
                "name distinct source rules FM001-FM007 or FM009; scanner/postprocessor failures cannot be waived",
            ));
        }
        if command == "end" {
            if reason.is_some() {
                return Err(refuse(line, "lint-end names rules only; put the reason on lint-begin"));
            }
            let Some((begin, active_rules, active_reason, begin_line)) = region.take() else {
                return Err(refuse(line, "lint-end has no matching lint-begin"));
            };
            let a: BTreeSet<&String> = rules.iter().collect();
            let b: BTreeSet<&String> = active_rules.iter().collect();
            if a != b {
                return Err(refuse(line, "lint-end must name the same rules as lint-begin"));
            }
            collect(begin, line_start, "region", &rules, &active_reason, begin_line, Some(line))?;
            continue;
        }
        let Some(reason) = reason.filter(|r| !r.trim().is_empty()) else {
            return Err(refuse(line, "a nonempty reason is required"));
        };
        if command == "file" {
            if lex.tokens.iter().any(|t| t.start < comment.start) {
                return Err(refuse(line, "lint-file must precede all code and preprocessor directives"));
            }
            collect(0, text.len(), "file", &rules, &reason, line, None)?;
            continue;
        }
        if command == "begin" {
            if region.is_some() {
                return Err(refuse(line, "nested lint-begin regions are not supported; close the current region first"));
            }
            region = Some(((line_end + 1).min(text.len()), rules, reason, line));
            continue;
        }
        // allow-next-line
        if line >= lex.line_count() {
            return Err(refuse(line, "no following source unit"));
        }
        let next_start = lex.line_start(line + 1);
        let first = lex.tokens.iter().find(|t| t.start >= next_start);
        let Some(first) = first.filter(|t| lex.line_of(t.start) == line + 1 && text[next_start..t.start].trim().is_empty()) else {
            return Err(refuse(line, "target must start on the immediately following line (no skipped comments or blank lines)"));
        };
        let start = first.start;
        let mut candidates: Vec<(usize, usize, &str)> = analysis
            .targets
            .iter()
            .filter(|(a, _)| *a == start)
            .map(|&(a, b)| (a, b, "ast"))
            .collect();
        // Whole function bodies are not suppression targets. Attributes on the
        // function's declaration header can still receive a specific FM006 waiver.
        for f in &analysis.functions {
            if f.start == start {
                let brace = lex
                    .tokens
                    .iter()
                    .filter(|t| f.start <= t.start && t.start < f.end && t.text(text) == "{")
                    .map(|t| t.start)
                    .next()
                    .unwrap_or(f.end);
                candidates.push((f.start, brace, "header"));
            }
        }
        for d in &lex.directives {
            if d.start <= start && start < d.end && (d.macro_name.is_some() || d.is_pragma()) {
                candidates.push((start, d.end, "directive"));
            }
        }
        if asm_keywords.iter().any(|k| k == first.text(text)) {
            let tail: Vec<_> = lex.tokens.iter().filter(|t| t.start >= start).collect();
            let mut opening = None;
            for (i, t) in tail.iter().enumerate().skip(1) {
                let s = t.text(text);
                if s == ";" || s == "=" {
                    break;
                }
                if s == "{" {
                    opening = Some(i);
                    break;
                }
            }
            if let Some(opening) = opening {
                let mut depth = 0i32;
                let mut closed = None;
                for t in &tail[opening..] {
                    match t.text(text) {
                        "{" => depth += 1,
                        "}" => {
                            depth -= 1;
                            if depth == 0 {
                                closed = Some(t.end);
                                break;
                            }
                        }
                        _ => {}
                    }
                }
                match closed {
                    Some(end) => candidates = vec![(start, end, "mwcc-asm")],
                    None => return Err(refuse(line, "unclosed asm block; cannot establish a safe boundary")),
                }
            }
        }
        let Some(&(a, b, kind)) = candidates.iter().max_by_key(|c| c.1) else {
            return Err(refuse(
                line,
                "no supported declaration/statement boundary; annotate the inner statement, not a function/block",
            ));
        };
        let only = |id: &str| rules.len() == 1 && rules[0] == id;
        if kind == "mwcc-asm" && !only("FM005") {
            return Err(refuse(line, "a MWCC asm block/function exception can name only FM005"));
        }
        if kind == "header" && !only("FM006") {
            return Err(refuse(line, "function-header exceptions can name only FM006, never the function body"));
        }
        collect(a, b, kind, &rules, &reason, line, None)?;
    }
    if let Some((_, _, _, begin_line)) = region {
        return Err(refuse(begin_line, "lint-begin is missing a matching lint-end; no implicit end-of-file waiver"));
    }
    for annotation in annotations {
        for i in annotation.selected {
            findings[i].inline_suppression = Some(annotation.metadata.clone());
        }
    }
    Ok(())
}
