//! Per-file facts shared by the rules: lexical tokens, both parse trees and
//! the conservative lexical scope information used for review filters.
//!
//! Nothing here is type inference. Scope lookups refuse ambiguity (shadowing,
//! redeclaration) rather than guessing.

use std::collections::{BTreeMap, HashMap, HashSet};

use regex::Regex;
use tree_sitter::{Node, Tree};

use crate::config::Severity;
use crate::finding::{Confidence, Finding, ParseRecovery, sha256_hex};
use crate::lexer::{Lexed, TokKind, collapse_whitespace};
use crate::parse::{self, is_field_child, node_text};

#[derive(Debug, Clone)]
pub struct Function {
    pub start: usize,
    pub end: usize,
    pub name: String,
}

#[derive(Debug, Clone)]
pub struct Binding {
    pub name: String,
    pub at: usize,
    pub scope_start: usize,
    pub scope_end: usize,
    /// Syntactically explicit pointer/array declarator.
    pub storage: bool,
}

#[derive(Debug, Clone)]
pub struct Named {
    pub start: usize,
    pub end: usize,
    pub name: String,
}

/// Both trees for a file. Owned separately so the borrowed `Analysis` can
/// hold nodes that point into them.
pub struct Parsed {
    pub tree: Tree,
    pub projection: Option<Tree>,
}

impl Parsed {
    pub fn new(text: &str, lexed: &Lexed) -> Parsed {
        let tree = parse::parse(text);
        let projection = if lexed.projection.trim().is_empty() {
            None
        } else {
            Some(parse::parse(&lexed.projection))
        };
        Parsed { tree, projection }
    }
}

pub struct Analysis<'t, 'a> {
    pub path: &'a str,
    pub text: &'a str,
    pub lex: &'a Lexed,
    /// Pre-order nodes of the source tree.
    pub source: Vec<Node<'t>>,
    /// Pre-order nodes of the macro projection tree (may be empty).
    pub projection: Vec<Node<'t>>,
    pub functions: Vec<Function>,
    pub blocks: Vec<(usize, usize)>,
    pub bindings: HashMap<String, Vec<Binding>>,
    pub local_names: Vec<Named>,
    pub local_writes: Vec<Named>,
    /// Node ids of hex literals that are direct bitwise-mask operands.
    pub masked_literals: HashSet<usize>,
    /// Node ids of declarations that carry a `const`/`constexpr` qualifier.
    pub const_declarations: HashSet<usize>,
    /// Bounded statement/declaration spans that inline waivers may target.
    pub targets: Vec<(usize, usize)>,
    pub recovery: Vec<ParseRecovery>,
}

const NAME_SKIP: [&str; 10] = [
    "__attribute__", "__declspec", "noexcept", "throw", "optimize", "optnone", "target",
    "noinline", "always_inline", "asm",
];

fn function_name(header: &str) -> String {
    thread_local! {
        static NAME: Regex = Regex::new(r"([\w:~]+)\s*\(").unwrap();
    }
    NAME.with(|re| {
        re.captures_iter(header)
            .map(|c| c[1].to_string())
            .find(|n| !NAME_SKIP.contains(&n.as_str()))
            .unwrap_or_else(|| "<function>".to_string())
    })
}

const DECLARATOR_PARENTS: [&str; 6] = [
    "declaration",
    "parameter_declaration",
    "init_declarator",
    "pointer_declarator",
    "array_declarator",
    "reference_declarator",
];

const LOCAL_DECLARATOR_PARENTS: [&str; 5] = [
    "declaration",
    "init_declarator",
    "array_declarator",
    "pointer_declarator",
    "reference_declarator",
];

fn is_declarator_of(node: Node<'_>, parents: &[&str]) -> bool {
    node.parent().is_some_and(|p| {
        parents.contains(&p.kind())
            && (is_field_child(p, "declarator", node)
                // reference_declarator has no field name for its declarator.
                || (p.kind() == "reference_declarator" && p.named_child(0) == Some(node)))
    })
}

impl<'t, 'a> Analysis<'t, 'a> {
    pub fn new(path: &'a str, text: &'a str, lex: &'a Lexed, parsed: &'t Parsed) -> Self {
        let source = parse::all_nodes(&parsed.tree);
        let projection = parsed
            .projection
            .as_ref()
            .map(parse::all_nodes)
            .unwrap_or_default();

        let mut functions = Vec::new();
        let mut blocks = Vec::new();
        let mut storage = HashSet::new();
        let mut raw_bindings = Vec::new();
        let mut local_names = Vec::new();
        let mut local_writes = Vec::new();
        let mut masked_literals = HashSet::new();
        let mut const_declarations = HashSet::new();
        let mut targets = Vec::new();
        let mut recovery = Vec::new();

        for &n in &source {
            let kind = n.kind();
            match kind {
                "function_definition" => {
                    let a = n.start_byte();
                    let b = n.end_byte();
                    let header = text[a..b].split('{').next().unwrap_or("");
                    functions.push(Function { start: a, end: b, name: function_name(header) });
                }
                "compound_statement" | "for_statement" | "if_statement" | "switch_statement" => {
                    blocks.push((n.start_byte(), n.end_byte()));
                }
                "identifier" => {
                    let in_decl = parse::has_ancestor(n, "declaration")
                        || parse::has_ancestor(n, "parameter_declaration");
                    if in_decl {
                        if is_declarator_of(n, &["pointer_declarator", "array_declarator"]) {
                            storage.insert(n.start_byte());
                        }
                        if is_declarator_of(n, &DECLARATOR_PARENTS) {
                            raw_bindings.push((node_text(n, text).to_string(), n.start_byte()));
                        }
                    }
                    if let Some(decl) = parse::variable_declaration(n)
                        && parse::has_ancestor(decl, "function_definition")
                        && is_declarator_of(n, &LOCAL_DECLARATOR_PARENTS)
                    {
                        local_names.push(Named {
                            start: n.start_byte(),
                            end: n.end_byte(),
                            name: node_text(n, text).to_string(),
                        });
                    }
                    if let Some(p) = n.parent() {
                        let standalone = |x: Node<'_>| {
                            x.parent().is_some_and(|s| s.kind() == "expression_statement")
                        };
                        let is_write = (p.kind() == "assignment_expression"
                            && is_field_child(p, "left", n)
                            && standalone(p))
                            || (p.kind() == "update_expression" && standalone(p));
                        if is_write {
                            local_writes.push(Named {
                                start: n.start_byte(),
                                end: n.end_byte(),
                                name: node_text(n, text).to_string(),
                            });
                        }
                    }
                }
                "type_qualifier" => {
                    let t = node_text(n, text);
                    if t == "const" || t == "constexpr" {
                        let mut cur = n.parent();
                        while let Some(p) = cur {
                            if p.kind() == "declaration" {
                                const_declarations.insert(p.id());
                            }
                            cur = p.parent();
                        }
                    }
                }
                "declaration" | "field_declaration" | "expression_statement" | "return_statement"
                | "gnu_asm_expression" => {
                    let nested = ["compound_statement", "struct_specifier", "class_specifier",
                        "union_specifier", "enum_specifier"]
                        .iter()
                        .any(|k| parse::has_descendant(n, k));
                    if !nested {
                        targets.push((n.start_byte(), n.end_byte()));
                    }
                }
                "ERROR" => {
                    recovery.push(ParseRecovery {
                        path: path.to_string(),
                        line: lex.line_of(n.start_byte()),
                        projection: false,
                        excerpt: truncate(&collapse_whitespace(node_text(n, text)), 120),
                    });
                }
                _ => {}
            }
        }
        collect_masks(&source, text, &mut masked_literals);
        collect_masks(&projection, &lex.projection, &mut masked_literals);
        for &n in &projection {
            if n.kind() == "ERROR" {
                recovery.push(ParseRecovery {
                    path: path.to_string(),
                    line: lex.line_of(n.start_byte()),
                    projection: true,
                    excerpt: truncate(&collapse_whitespace(node_text(n, &lex.projection)), 120),
                });
            }
        }
        functions.sort_by_key(|f| (f.start, f.end));

        let mut bindings: HashMap<String, Vec<Binding>> = HashMap::new();
        for (name, at) in raw_bindings {
            let mut best: Option<(usize, usize, usize)> = None;
            for &(x, y) in blocks.iter().chain(functions.iter().map(|f| (f.start, f.end)).collect::<Vec<_>>().iter()) {
                if x <= at && at < y {
                    let w = y - x;
                    if best.is_none_or(|(bw, _, _)| w < bw) {
                        best = Some((w, x, y));
                    }
                }
            }
            let (scope_start, scope_end) = best.map(|(_, x, y)| (x, y)).unwrap_or((0, text.len()));
            let storage = storage.contains(&at);
            bindings
                .entry(name.clone())
                .or_default()
                .push(Binding { name, at, scope_start, scope_end, storage });
        }

        Analysis {
            path,
            text,
            lex,
            source,
            projection,
            functions,
            blocks,
            bindings,
            local_names,
            local_writes,
            masked_literals,
            const_declarations,
            targets,
            recovery,
        }
    }

    /// Name of the smallest function containing `at`, or `<file>`.
    pub fn owner(&self, at: usize) -> &str {
        self.functions
            .iter()
            .filter(|f| f.start <= at && at < f.end)
            .min_by_key(|f| f.end - f.start)
            .map(|f| f.name.as_str())
            .unwrap_or("<file>")
    }

    /// Smallest function span containing `at`.
    pub fn enclosing_function(&self, at: usize) -> Option<&Function> {
        self.functions
            .iter()
            .filter(|f| f.start <= at && at < f.end)
            .min_by_key(|f| f.end - f.start)
    }

    /// The unique, nearest visible declaration of `name` before `at`, only if
    /// it is a syntactically explicit pointer/array declarator.
    pub fn explicit_storage(&self, name: &str, at: usize) -> Option<&Binding> {
        let visible: Vec<&Binding> = self
            .bindings
            .get(name)?
            .iter()
            .filter(|b| b.at < at && b.scope_start <= at && at < b.scope_end)
            .collect();
        let width = visible.iter().map(|b| b.scope_end - b.scope_start).min()?;
        let nearest: Vec<&&Binding> = visible
            .iter()
            .filter(|b| b.scope_end - b.scope_start == width)
            .collect();
        match nearest.as_slice() {
            [only] if only.storage => Some(**only),
            _ => None,
        }
    }

    /// Whether the node sits inside a declaration carrying `const`/`constexpr`.
    pub fn in_const_declaration(&self, node: Node<'_>) -> bool {
        let mut cur = node.parent();
        while let Some(p) = cur {
            if p.kind() == "declaration" && self.const_declarations.contains(&p.id()) {
                return true;
            }
            cur = p.parent();
        }
        false
    }
}

fn collect_masks(nodes: &[Node<'_>], text: &str, out: &mut HashSet<usize>) {
    thread_local! {
        static MASK: Regex = Regex::new(r"^\(*\s*0[xX][0-9a-fA-F]+[uUlL]*\s*\)*$").unwrap();
    }
    let literal_id = |n: Node<'_>| -> Option<usize> {
        let inner = parse::unparen(n);
        if inner.kind() == "number_literal" && MASK.with(|re| re.is_match(node_text(n, text))) {
            Some(inner.id())
        } else {
            None
        }
    };
    for &n in nodes {
        match n.kind() {
            "binary_expression" => {
                let op = n.child_by_field_name("operator").map(|o| o.kind());
                if matches!(op, Some("&") | Some("|") | Some("^")) {
                    for field in ["left", "right"] {
                        if let Some(id) = n.child_by_field_name(field).and_then(literal_id) {
                            out.insert(id);
                        }
                    }
                }
            }
            "assignment_expression" => {
                let op = n.child_by_field_name("operator").map(|o| o.kind());
                if matches!(op, Some("&=") | Some("|=") | Some("^="))
                    && let Some(id) = n.child_by_field_name("right").and_then(literal_id) {
                        out.insert(id);
                    }
            }
            "unary_expression" => {
                let op = n.child_by_field_name("operator").map(|o| o.kind());
                if op == Some("~")
                    && let Some(id) = n.child_by_field_name("argument").and_then(literal_id) {
                        out.insert(id);
                    }
            }
            _ => {}
        }
    }
}

pub fn truncate(s: &str, max: usize) -> String {
    s.chars().take(max).collect()
}

/// Collects findings for one file and computes their identity.
pub struct Sink<'s, 't, 'a> {
    pub analysis: &'s Analysis<'t, 'a>,
    pub findings: Vec<Finding>,
}

impl<'s, 't, 'a> Sink<'s, 't, 'a> {
    pub fn new(analysis: &'s Analysis<'t, 'a>) -> Self {
        Sink { analysis, findings: Vec::new() }
    }

    #[allow(clippy::too_many_arguments)]
    pub fn emit(
        &mut self,
        rule: &str,
        start: usize,
        end: usize,
        message: &str,
        confidence: Confidence,
        severity: Severity,
        details: BTreeMap<String, serde_json::Value>,
    ) {
        let a = self.analysis;
        let normalized = collapse_whitespace(&a.text[start..end]);
        let near = a.lex.token_index_at(start);
        let lo = near.saturating_sub(6);
        let hi = (near + 7).min(a.lex.tokens.len());
        let context: Vec<&str> = a.lex.tokens[lo..hi].iter().map(|t| t.text(a.text)).collect();
        let directive = details
            .get("directive")
            .and_then(|v| v.as_str())
            .unwrap_or("");
        let identity = [a.path, rule, a.owner(start), &normalized, &context.join(" "), directive].join("\0");
        let line = a.lex.line_of(start);
        let column = a.lex.column_of(a.text, start);
        self.findings.push(Finding {
            rule: rule.to_string(),
            path: a.path.to_string(),
            line,
            column,
            scope: a.owner(start).to_string(),
            message: message.to_string(),
            confidence,
            excerpt: truncate(&normalized, 240),
            fingerprint: sha256_hex(identity.as_bytes()),
            suppressed: false,
            severity,
            review_reason: None,
            suppression_source: None,
            inline_suppression: None,
            guidance_id: None,
            details,
            offset: start,
        });
    }

    /// Identifier tokens within a byte range.
    pub fn identifiers(&self, start: usize, end: usize) -> impl Iterator<Item = (usize, &'a str)> + '_ {
        let a = self.analysis;
        let lo = a.lex.tokens.partition_point(|t| t.start < start);
        a.lex.tokens[lo..]
            .iter()
            .take_while(move |t| t.start < end)
            .filter(|t| t.kind == TokKind::Id)
            .map(move |t| (t.start, t.text(a.text)))
    }
}

/// Small helper for building the `details` map.
pub fn details<const N: usize>(pairs: [(&str, serde_json::Value); N]) -> BTreeMap<String, serde_json::Value> {
    pairs.into_iter().map(|(k, v)| (k.to_string(), v)).collect()
}
