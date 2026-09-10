//! FM003: possible stack/allocation scaffolding.
//!
//! Four lexical candidates: local arrays with no use beyond their
//! declaration, `volatile` locals, trash/padding-named locals, and locals
//! that are only ever assigned or incremented. None of these is a def-use
//! proof; macro expansion, aliasing and hardware semantics stay unverified.

use std::collections::HashMap;

use regex::Regex;

use crate::analysis::{Analysis, Sink, details};
use crate::config::{Fm003Config, Severity};
use crate::finding::Confidence;
use crate::parse::{self, has_descendant_where, is_field_child, node_text};

use super::Rule;

pub const ARRAY_MESSAGE: &str = "Local array has no lexical use beyond its declaration.";
pub const VOLATILE_MESSAGE: &str = "Volatile local declaration; verify legitimate hardware/concurrency semantics, not allocation scaffolding.";
pub const TRASH_MESSAGE: &str = "Trash/unused/padding-family declaration; investigate original local structure.";
pub const WRITE_ONLY_MESSAGE: &str = "Local is only declared and assigned/incremented, never otherwise read; investigate an artificial induction-variable carrier.";

pub struct Fm003 {
    severity: Severity,
    unused_arrays: bool,
    volatile_locals: bool,
    trash_names: bool,
    write_only_locals: bool,
    trash: Regex,
}

impl Fm003 {
    pub fn new(config: &Fm003Config) -> Self {
        Fm003 {
            severity: config.severity,
            unused_arrays: config.unused_arrays,
            volatile_locals: config.volatile_locals,
            trash_names: config.trash_names,
            write_only_locals: config.write_only_locals,
            trash: Regex::new(&config.trash_pattern).expect("validated by config"),
        }
    }
}

impl Rule for Fm003 {
    fn id(&self) -> &'static str {
        "FM003"
    }

    fn title(&self) -> &'static str {
        "Possible stack/allocation scaffolding"
    }

    fn check(&self, a: &Analysis<'_, '_>, sink: &mut Sink<'_, '_, '_>) {
        let text = a.text;
        // Identifier counts per function, computed lazily.
        let mut counts: HashMap<(usize, usize), HashMap<&str, usize>> = HashMap::new();

        for &n in &a.source {
            match n.kind() {
                "array_declarator" if self.unused_arrays => {
                    let Some(name_node) = n.child_by_field_name("declarator") else { continue };
                    if name_node.kind() != "identifier"
                        || !parse::has_ancestor(n, "compound_statement")
                        || !parse::has_ancestor(n, "function_definition")
                    {
                        continue;
                    }
                    let Some(f) = a.enclosing_function(n.start_byte()) else { continue };
                    let name = node_text(name_node, text);
                    let uses = counts.entry((f.start, f.end)).or_insert_with(|| {
                        let mut m = HashMap::new();
                        for (_, id) in sink.identifiers(f.start, f.end) {
                            *m.entry(id).or_insert(0) += 1;
                        }
                        m
                    });
                    if uses.get(name).copied().unwrap_or(0) == 1 {
                        sink.emit(
                            "FM003",
                            n.start_byte(),
                            n.end_byte(),
                            ARRAY_MESSAGE,
                            Confidence::Review,
                            self.severity,
                            details([("variable", serde_json::json!(name))]),
                        );
                    }
                }
                "declaration" if self.volatile_locals || self.trash_names => {
                    if !parse::has_ancestor(n, "function_definition") {
                        continue;
                    }
                    let volatile = self.volatile_locals
                        && has_descendant_where(n, "type_qualifier", text, |t| t == "volatile");
                    let (x, y) = (n.start_byte(), n.end_byte());
                    for local in a.local_names.iter().filter(|l| x <= l.start && l.end <= y) {
                        if volatile {
                            sink.emit(
                                "FM003",
                                x,
                                y,
                                VOLATILE_MESSAGE,
                                Confidence::Review,
                                self.severity,
                                details([("variable", serde_json::json!(local.name))]),
                            );
                        }
                        if self.trash_names && self.trash.is_match(&local.name) {
                            sink.emit(
                                "FM003",
                                x,
                                y,
                                TRASH_MESSAGE,
                                Confidence::Review,
                                self.severity,
                                details([("variable", serde_json::json!(local.name))]),
                            );
                        }
                    }
                }
                _ => {}
            }
        }

        if !self.write_only_locals {
            return;
        }
        let _ = is_field_child;
        for f in &a.functions {
            let declared: Vec<_> = a
                .local_names
                .iter()
                .filter(|l| f.start <= l.start && l.start < f.end)
                .collect();
            for local in &declared {
                if declared.iter().filter(|d| d.name == local.name).count() != 1 {
                    continue; // shadowing is ambiguous
                }
                let clean = &a.lex.clean[f.start..local.start];
                let decl_start = clean
                    .iter()
                    .rposition(|&c| c == b';' || c == b'{')
                    .map(|i| i + 1)
                    .unwrap_or(0);
                let head = String::from_utf8_lossy(&clean[decl_start..]);
                if head.split(|c: char| !(c.is_alphanumeric() || c == '_'))
                    .any(|w| w == "volatile" || w == "static")
                {
                    continue;
                }
                let writes: std::collections::BTreeSet<usize> = a
                    .local_writes
                    .iter()
                    .filter(|w| f.start <= w.start && w.start < f.end && w.name == local.name)
                    .map(|w| w.start)
                    .collect();
                if writes.is_empty() {
                    continue;
                }
                let uses: std::collections::BTreeSet<usize> = sink
                    .identifiers(f.start, f.end)
                    .filter(|(_, id)| *id == local.name)
                    .map(|(at, _)| at)
                    .collect();
                let mut expected = writes.clone();
                expected.insert(local.start);
                if uses == expected {
                    sink.emit(
                        "FM003",
                        local.start,
                        local.end,
                        WRITE_ONLY_MESSAGE,
                        Confidence::Heuristic,
                        self.severity,
                        details([
                            ("variable", serde_json::json!(local.name)),
                            ("pattern", serde_json::json!("write-only-local")),
                        ]),
                    );
                }
            }
        }
    }
}
