//! FM005: assembly without an explicit reviewed exception.
//!
//! Purely lexical: every `asm`/`__asm`/`__asm__`/`ASM` identifier token in
//! code (never in comments or strings) is a candidate. This covers GNU
//! statement asm, MWCC `asm void f() { ... }` functions and opaque macro
//! bodies uniformly, including files the parser cannot recover.

use crate::analysis::{Analysis, Sink, details};
use crate::config::{Fm005Config, Severity};
use crate::finding::Confidence;
use crate::lexer::TokKind;

use super::Rule;

pub const MESSAGE: &str = "Assembly requires a reasoned source exception or an exact reviewed macro exception; never auto-remove it.";

pub struct Fm005 {
    severity: Severity,
    keywords: Vec<String>,
}

impl Fm005 {
    pub fn new(config: &Fm005Config) -> Self {
        Fm005 { severity: config.severity, keywords: config.keywords.clone() }
    }

    pub fn is_keyword(&self, word: &str) -> bool {
        self.keywords.iter().any(|k| k == word)
    }
}

impl Rule for Fm005 {
    fn id(&self) -> &'static str {
        "FM005"
    }

    fn title(&self) -> &'static str {
        "Assembly without an explicit reviewed exception"
    }

    fn check(&self, a: &Analysis<'_, '_>, sink: &mut Sink<'_, '_, '_>) {
        for t in &a.lex.tokens {
            if t.kind != TokKind::Id || !self.is_keyword(t.text(a.text)) {
                continue;
            }
            let directive = a.lex.directive_at(t.start);
            let macro_name = directive.and_then(|d| d.macro_name.as_deref());
            // `#define ASM asm`: the defined name is not assembly, its body is.
            if macro_name == Some(t.text(a.text)) && a.lex.in_macro_body(t.start).is_none() {
                continue;
            }
            sink.emit(
                "FM005",
                t.start,
                t.end,
                MESSAGE,
                Confidence::Review,
                self.severity,
                details([
                    ("macro", serde_json::json!(macro_name)),
                    ("directive", serde_json::json!(directive.map(|d| d.text.as_str()))),
                ]),
            );
        }
    }
}
