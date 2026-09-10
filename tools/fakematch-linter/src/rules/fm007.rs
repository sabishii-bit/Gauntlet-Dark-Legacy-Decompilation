//! FM007: unnamed hexadecimal expression constant.
//!
//! Hex literals outside enums, `const` declarations, object-like macros and
//! direct bitwise masks may hide an enum, field offset or API value.

use crate::analysis::{Analysis, Sink, details};
use crate::config::{Fm007Config, Severity};
use crate::finding::Confidence;
use crate::parse::{self, node_text};

use super::Rule;

pub const MESSAGE: &str = "Hex expression candidate; the wrapper excludes named constants and direct bitwise masks.";

pub struct Fm007 {
    severity: Severity,
    ignore_masks: bool,
    ignore_enums: bool,
    ignore_const_declarations: bool,
    ignore_object_macros: bool,
}

impl Fm007 {
    pub fn new(config: &Fm007Config) -> Self {
        Fm007 {
            severity: config.severity,
            ignore_masks: config.ignore_masks,
            ignore_enums: config.ignore_enums,
            ignore_const_declarations: config.ignore_const_declarations,
            ignore_object_macros: config.ignore_object_macros,
        }
    }
}

impl Rule for Fm007 {
    fn id(&self) -> &'static str {
        "FM007"
    }

    fn title(&self) -> &'static str {
        "Unnamed hexadecimal expression constant"
    }

    fn check(&self, a: &Analysis<'_, '_>, sink: &mut Sink<'_, '_, '_>) {
        for (projected, nodes, text) in [
            (false, &a.source, a.text),
            (true, &a.projection, a.lex.projection.as_str()),
        ] {
            for &n in nodes {
                if n.kind() != "number_literal" {
                    continue;
                }
                let literal = node_text(n, text);
                if !(literal.starts_with("0x") || literal.starts_with("0X")) {
                    continue;
                }
                if self.ignore_enums && parse::has_ancestor(n, "enum_specifier") {
                    continue;
                }
                if self.ignore_const_declarations && a.in_const_declaration(n) {
                    continue;
                }
                if projected
                    && self.ignore_object_macros
                    && a.lex.in_macro_body(n.start_byte()) != Some(true)
                {
                    continue;
                }
                if self.ignore_masks && a.masked_literals.contains(&n.id()) {
                    continue;
                }
                sink.emit(
                    "FM007",
                    n.start_byte(),
                    n.end_byte(),
                    MESSAGE,
                    Confidence::Heuristic,
                    self.severity,
                    details([]),
                );
            }
        }
    }
}
