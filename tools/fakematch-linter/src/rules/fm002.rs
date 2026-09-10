//! FM002: nested dereference through pointer casts.
//!
//! `*(int*)*(void**)(p + 8)` hides an intermediate pointer type. Only the
//! outermost cast-dereference of a chain is reported, with its depth.

use tree_sitter::Node;

use crate::analysis::{Analysis, Sink, details};
use crate::config::{Fm002Config, Severity};
use crate::finding::Confidence;
use crate::parse::is_cast_deref;

use super::Rule;

pub const MESSAGE: &str = "Nested dereference through casts exceeds depth one; reconstruct intermediate pointer types.";

pub struct Fm002 {
    severity: Severity,
    min_depth: usize,
}

impl Fm002 {
    pub fn new(config: &Fm002Config) -> Self {
        Fm002 { severity: config.severity, min_depth: config.min_depth.max(1) }
    }
}

/// Number of cast-dereferences in the subtree, including the node itself.
fn cast_deref_count(node: Node<'_>) -> usize {
    let mut count = 0;
    let mut cursor = node.walk();
    let mut depth = 0usize;
    loop {
        if is_cast_deref(cursor.node()) {
            count += 1;
        }
        if cursor.goto_first_child() {
            depth += 1;
            continue;
        }
        loop {
            if depth == 0 {
                return count;
            }
            if cursor.goto_next_sibling() {
                break;
            }
            cursor.goto_parent();
            depth -= 1;
        }
    }
}

impl Rule for Fm002 {
    fn id(&self) -> &'static str {
        "FM002"
    }

    fn title(&self) -> &'static str {
        "Nested dereference through pointer casts"
    }

    fn check(&self, analysis: &Analysis<'_, '_>, sink: &mut Sink<'_, '_, '_>) {
        for nodes in [&analysis.source, &analysis.projection] {
            for &n in nodes {
                if !is_cast_deref(n) {
                    continue;
                }
                // Report only the outermost link of a chain.
                let mut cur = n.parent();
                let mut nested = false;
                while let Some(p) = cur {
                    if is_cast_deref(p) {
                        nested = true;
                        break;
                    }
                    cur = p.parent();
                }
                if nested {
                    continue;
                }
                let depth = cast_deref_count(n);
                if depth >= self.min_depth && depth >= 2 {
                    sink.emit(
                        "FM002",
                        n.start_byte(),
                        n.end_byte(),
                        MESSAGE,
                        Confidence::Review,
                        self.severity,
                        details([("depth", serde_json::json!(depth))]),
                    );
                }
            }
        }
    }
}
