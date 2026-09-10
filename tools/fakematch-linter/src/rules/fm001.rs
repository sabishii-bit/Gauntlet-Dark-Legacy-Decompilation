//! FM001: raw offset or indexed pointer cast.
//!
//! Matches `*(T*)(p + K)`, `*(T*)((U*)p + K)`, `*((T*)(p - K))` and
//! `((T*)p)[K]`: a pointer cast applied to pointer arithmetic, or a
//! subscript applied directly to a parenthesised cast.

use crate::analysis::{Analysis, Sink, details};
use crate::config::{Fm001Config, Severity};
use crate::finding::Confidence;
use crate::parse::{is_cast_deref, is_pointer_cast, unparen};

use super::Rule;

pub const MESSAGE: &str =
    "Raw offset/index cast; verify the actual field, element type, width and stride.";

pub struct Fm001 {
    severity: Severity,
}

impl Fm001 {
    pub fn new(config: &Fm001Config) -> Self {
        Fm001 { severity: config.severity }
    }
}

impl Rule for Fm001 {
    fn id(&self) -> &'static str {
        "FM001"
    }

    fn title(&self) -> &'static str {
        "Raw offset or indexed pointer cast"
    }

    fn check(&self, analysis: &Analysis<'_, '_>, sink: &mut Sink<'_, '_, '_>) {
        for nodes in [&analysis.source, &analysis.projection] {
            for &n in nodes {
                let hit = match n.kind() {
                    "pointer_expression" => {
                        // `*((T*)(p + 8))` wraps the cast in parentheses.
                        let arg = n.child_by_field_name("argument").map(unparen);
                        n.child_by_field_name("operator").is_some_and(|o| o.kind() == "*")
                            && arg.is_some_and(|cast| {
                                is_pointer_cast(cast)
                                    && cast
                                        .child_by_field_name("value")
                                        .map(unparen)
                                        .is_some_and(|v| {
                                            v.kind() == "binary_expression"
                                                && v.child_by_field_name("operator")
                                                    .is_some_and(|o| matches!(o.kind(), "+" | "-"))
                                        })
                            })
                    }
                    "subscript_expression" => n
                        .child_by_field_name("argument")
                        .is_some_and(|arg| {
                            arg.kind() == "parenthesized_expression"
                                && arg.named_child(0).is_some_and(|c| c.kind() == "cast_expression")
                        }),
                    _ => false,
                };
                if hit {
                    let _ = is_cast_deref;
                    sink.emit(
                        "FM001",
                        n.start_byte(),
                        n.end_byte(),
                        MESSAGE,
                        Confidence::Review,
                        self.severity,
                        details([]),
                    );
                }
            }
        }
    }
}
