//! FM009: unnamed constant-offset pointer or array access.
//!
//! `strings + 364` where `strings` has a visible, unambiguous pointer/array
//! declaration is probably an unnamed string, field or element. Scalars,
//! typedef-hidden pointers and shadowed names are deliberately not claimed.

use regex::Regex;

use crate::analysis::{Analysis, Sink, details};
use crate::config::{Fm009Config, Severity};
use crate::finding::Confidence;
use crate::parse::node_text;

use super::Rule;

pub const MESSAGE: &str = "Unnamed numeric offset into a declared pointer/array; recover the referenced string, field or element from target data. This does not prove a struct or loop.";

pub struct Fm009 {
    severity: Severity,
    require_visible_declaration: bool,
    offset: Regex,
}

impl Fm009 {
    pub fn new(config: &Fm009Config) -> Self {
        Fm009 {
            severity: config.severity,
            require_visible_declaration: config.require_visible_declaration,
            offset: Regex::new(&config.offset_pattern).expect("validated by config"),
        }
    }
}

impl Rule for Fm009 {
    fn id(&self) -> &'static str {
        "FM009"
    }

    fn title(&self) -> &'static str {
        "Unnamed constant-offset pointer or array access"
    }

    fn check(&self, a: &Analysis<'_, '_>, sink: &mut Sink<'_, '_, '_>) {
        for &n in &a.source {
            if n.kind() != "binary_expression" {
                continue;
            }
            let (Some(left), Some(op), Some(right)) = (
                n.child_by_field_name("left"),
                n.child_by_field_name("operator"),
                n.child_by_field_name("right"),
            ) else {
                continue;
            };
            let (base, offset) = match op.kind() {
                "+" | "-" if left.kind() == "identifier" && self.offset.is_match(node_text(right, a.text)) => {
                    (left, right)
                }
                "+" if right.kind() == "identifier" && self.offset.is_match(node_text(left, a.text)) => {
                    (right, left)
                }
                _ => continue,
            };
            let name = node_text(base, a.text);
            let declaration_line = if self.require_visible_declaration {
                match a.explicit_storage(name, n.start_byte()) {
                    Some(binding) => Some(a.lex.line_of(binding.at)),
                    None => continue,
                }
            } else {
                a.explicit_storage(name, n.start_byte()).map(|b| a.lex.line_of(b.at))
            };
            sink.emit(
                "FM009",
                n.start_byte(),
                n.end_byte(),
                MESSAGE,
                Confidence::Heuristic,
                self.severity,
                details([
                    ("base", serde_json::json!(name)),
                    ("offset", serde_json::json!(node_text(offset, a.text))),
                    ("declaration_line", serde_json::json!(declaration_line)),
                ]),
            );
        }
    }
}
