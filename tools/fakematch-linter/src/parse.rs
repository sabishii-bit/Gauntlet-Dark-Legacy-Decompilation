//! In-process tree-sitter parsing and small node helpers.

use std::borrow::Cow;
use std::cell::RefCell;

use tree_sitter::{Node, Parser, Tree};

use crate::lexer::{TokKind, tokenize};

thread_local! {
    static PARSER: RefCell<Parser> = RefCell::new({
        let mut p = Parser::new();
        p.set_language(&tree_sitter_cpp::LANGUAGE.into())
            .expect("tree-sitter-cpp grammar loads");
        p
    });
}

/// Parse C/C++ text with the C++ grammar. The reconstructed sources use MWCC
/// C++ regions even in `.c` files, so one grammar is used consistently.
/// The narrow `offsetof` compatibility projection below preserves every
/// source byte position; callers still retrieve excerpts from the original.
pub fn parse(text: &str) -> Tree {
    PARSER.with(|p| {
        let mut p = p.borrow_mut();
        let text = project_offsetof(text, &mut p);
        p.reset();
        p.parse(text.as_ref(), None).expect("parser has a language and no cancellation")
    })
}

/// tree-sitter-cpp 0.23's `offsetof_expression` accepts only one
/// `_field_identifier`, not valid subobject designators such as
/// `offsetof(MILESTONE, objgrp.worldmat[3][0])`. Parse those as ordinary
/// macro calls by changing only the eight-byte keyword in a private buffer.
/// This is not macro expansion or error suppression: every member/index
/// operand remains in the AST and is scanned using the original source.
///
/// Limit the workaround to a single typedef-name type and a separately
/// parsed, error-free member/subscript chain. More complex type spellings,
/// malformed designators, comments and strings are never rewritten. Their
/// normal parse/recovery behavior remains visible; no validation of types,
/// index constantness or actual member layout is implied.
fn project_offsetof<'a>(text: &'a str, parser: &mut Parser) -> Cow<'a, str> {
    if !text.contains("offsetof") {
        return Cow::Borrowed(text);
    }
    let tokens: Vec<_> = tokenize(text)
        .into_iter()
        .filter(|t| !matches!(t.kind, TokKind::Space | TokKind::Comment))
        .collect();
    let mut projected: Option<Vec<u8>> = None;
    for (i, token) in tokens.iter().enumerate() {
        if token.kind != TokKind::Id || token.text(text) != "offsetof" {
            continue;
        }
        let Some(head) = tokens.get(i + 1..i + 4) else { continue };
        if head[0].text(text) != "(" || head[1].kind != TokKind::Id || head[2].text(text) != "," {
            continue;
        }
        let mut depth = 0;
        let mut end = None;
        for t in &tokens[i + 1..] {
            // Punctuation inside string tokens must not delimit the call.
            if t.kind != TokKind::Op {
                continue;
            }
            match t.text(text) {
                "(" => depth += 1,
                ")" => {
                    depth -= 1;
                    if depth == 0 {
                        end = Some(t.end);
                        break;
                    }
                }
                ";" | "{" | "}" => break,
                _ => {}
            }
        }
        let Some(end) = end else { continue };
        let candidate = format!("void f(){{ off_set_{}; }}", &text[token.end..end]);
        parser.reset();
        let tree = parser.parse(&candidate, None).expect("parser has a language and no cancellation");
        if tree.root_node().has_error() {
            continue;
        }
        let nodes = all_nodes(&tree);
        let Some(call) = nodes.iter().find(|n| n.kind() == "call_expression") else { continue };
        let Some(args) = call.child_by_field_name("arguments") else { continue };
        let mut cursor = args.walk();
        let args: Vec<_> = args.named_children(&mut cursor).filter(|n| !n.is_extra()).collect();
        if args.len() != 2 || args[0].kind() != "identifier"
            || args[1].kind() == "identifier" || !member_designator(args[1])
        {
            continue;
        }
        projected.get_or_insert_with(|| text.as_bytes().to_vec())[token.start..token.end]
            .copy_from_slice(b"off_set_");
    }
    match projected {
        Some(bytes) => Cow::Owned(String::from_utf8(bytes).expect("only ASCII keyword bytes changed")),
        None => Cow::Borrowed(text),
    }
}

fn member_designator(node: Node<'_>) -> bool {
    match node.kind() {
        "identifier" => true,
        "field_expression" => {
            node.child_by_field_name("operator").is_some_and(|n| n.kind() == ".")
                && node.child_by_field_name("field").is_some_and(|n| n.kind() == "field_identifier")
                && node.child_by_field_name("argument").is_some_and(member_designator)
        }
        "subscript_expression" => {
            node.child_by_field_name("argument").is_some_and(member_designator)
                && node.child_by_field_name("indices").is_some_and(|indices| {
                    let mut cursor = indices.walk();
                    indices.named_children(&mut cursor).filter(|n| !n.is_extra()).count() == 1
                })
        }
        _ => false,
    }
}

/// Every node of the tree in pre-order, including anonymous nodes.
pub fn all_nodes(tree: &Tree) -> Vec<Node<'_>> {
    let mut out = Vec::new();
    let mut cursor = tree.walk();
    loop {
        out.push(cursor.node());
        if cursor.goto_first_child() {
            continue;
        }
        loop {
            if cursor.goto_next_sibling() {
                break;
            }
            if !cursor.goto_parent() {
                return out;
            }
        }
    }
}

pub fn node_text<'a>(node: Node<'_>, text: &'a str) -> &'a str {
    &text[node.start_byte()..node.end_byte()]
}

/// The node's child under `field`, if that child is `node`'s parent's field.
pub fn is_field_child(parent: Node<'_>, field: &str, child: Node<'_>) -> bool {
    // Declarations can have several children with the same field name:
    // `int *pointer, scalar;`. Checking only the first loses later bindings.
    let mut cursor = parent.walk();
    parent.children_by_field_name(field, &mut cursor).any(|c| c == child)
}

/// Whether any ancestor (excluding the node itself) has the given kind.
pub fn has_ancestor(node: Node<'_>, kind: &str) -> bool {
    let mut cur = node.parent();
    while let Some(n) = cur {
        if n.kind() == kind {
            return true;
        }
        cur = n.parent();
    }
    false
}

/// Nearest ancestor of the given kind.
pub fn ancestor<'t>(node: Node<'t>, kind: &str) -> Option<Node<'t>> {
    let mut cur = node.parent();
    while let Some(n) = cur {
        if n.kind() == kind {
            return Some(n);
        }
        cur = n.parent();
    }
    None
}

/// Follow only a variable's declarator chain to its declaration. A parameter
/// of a block-scope prototype is not a local variable of the outer function;
/// nor is a member of a locally declared type or a typedef's array extent.
pub fn variable_declaration(mut node: Node<'_>) -> Option<Node<'_>> {
    while let Some(parent) = node.parent() {
        match parent.kind() {
            "declaration" if is_field_child(parent, "declarator", node) => return Some(parent),
            "init_declarator" | "pointer_declarator" | "array_declarator"
                if is_field_child(parent, "declarator", node) => {}
            "reference_declarator" | "parenthesized_declarator"
                if parent.named_child(0) == Some(node) => {}
            _ => return None,
        }
        node = parent;
    }
    None
}

/// Strip layers of `parenthesized_expression`.
pub fn unparen<'t>(mut node: Node<'t>) -> Node<'t> {
    while node.kind() == "parenthesized_expression" {
        match node.named_child(0) {
            Some(inner) if node.named_child_count() == 1 => node = inner,
            _ => break,
        }
    }
    node
}

/// Whether a `cast_expression` casts to a pointer type (`(T*)`, `(const T**)`).
pub fn is_pointer_cast(cast: Node<'_>) -> bool {
    if cast.kind() != "cast_expression" {
        return false;
    }
    let Some(ty) = cast.child_by_field_name("type") else {
        return false;
    };
    let mut cur = ty.child_by_field_name("declarator");
    while let Some(d) = cur {
        if d.kind() == "abstract_pointer_declarator" {
            return true;
        }
        cur = d.child_by_field_name("declarator");
    }
    false
}

/// Whether the node is a `*expr` dereference whose operand is a cast.
pub fn is_cast_deref(node: Node<'_>) -> bool {
    node.kind() == "pointer_expression"
        && node
            .child_by_field_name("operator")
            .is_some_and(|op| op.kind() == "*")
        && node
            .child_by_field_name("argument")
            .is_some_and(|arg| arg.kind() == "cast_expression")
}

/// Whether the node has any descendant (excluding itself) with the given kind.
pub fn has_descendant(node: Node<'_>, kind: &str) -> bool {
    let mut cursor = node.walk();
    let mut depth = 0usize;
    loop {
        if cursor.goto_first_child() {
            depth += 1;
        } else {
            loop {
                if depth == 0 {
                    return false;
                }
                if cursor.goto_next_sibling() {
                    break;
                }
                cursor.goto_parent();
                depth -= 1;
            }
        }
        if cursor.node().kind() == kind {
            return true;
        }
    }
}

/// Whether the node has a descendant of `kind` whose text satisfies `pred`.
pub fn has_descendant_where(node: Node<'_>, kind: &str, text: &str, pred: impl Fn(&str) -> bool) -> bool {
    let mut cursor = node.walk();
    let mut depth = 0usize;
    loop {
        if cursor.goto_first_child() {
            depth += 1;
        } else {
            loop {
                if depth == 0 {
                    return false;
                }
                if cursor.goto_next_sibling() {
                    break;
                }
                cursor.goto_parent();
                depth -= 1;
            }
        }
        let n = cursor.node();
        if n.kind() == kind && pred(node_text(n, text)) {
            return true;
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parses_pointer_casts() {
        let text = "int f(){ return *(const int*)(p+8); }";
        let tree = parse(text);
        let nodes = all_nodes(&tree);
        let deref = nodes.iter().find(|n| n.kind() == "pointer_expression").unwrap();
        assert!(is_cast_deref(*deref));
        let cast = deref.child_by_field_name("argument").unwrap();
        assert!(is_pointer_cast(cast));
        assert_eq!(node_text(*deref, text), "*(const int*)(p+8)");
    }

    #[test]
    fn descendant_search_excludes_self() {
        let tree = parse("void f(){ { int x; } }");
        let nodes = all_nodes(&tree);
        let body = nodes.iter().find(|n| n.kind() == "compound_statement").unwrap();
        assert!(has_descendant(*body, "compound_statement"));
        let inner = nodes.iter().filter(|n| n.kind() == "compound_statement").nth(1).unwrap();
        assert!(!has_descendant(*inner, "compound_statement"));
    }

    #[test]
    fn offsetof_projection_is_narrow_and_preserves_source_ranges() {
        let text = "// offsetof(T, member[3])\r\n\
            const char *s = \"offsetof(T, member[3])\";\r\n\
            void f(){ use(offsetof(T, member), offsetof(T, member[3])); }";
        let mut expected = text.to_string();
        let at = expected.rfind("offsetof").unwrap();
        expected.replace_range(at..at + 8, "off_set_");
        PARSER.with(|p| assert_eq!(project_offsetof(text, &mut p.borrow_mut()), expected));
        let tree = parse(text);
        assert!(!tree.root_node().has_error());
        let nodes = all_nodes(&tree);
        let simple = nodes.iter().find(|n| n.kind() == "offsetof_expression").unwrap();
        assert_eq!(node_text(*simple, text), "offsetof(T, member)");
        let projected = nodes.iter().find(|n| {
            n.kind() == "call_expression" && node_text(**n, text) == "offsetof(T, member[3])"
        }).unwrap();
        let keyword = projected.child_by_field_name("function").unwrap();
        assert_eq!(node_text(keyword, text), "offsetof");
        assert_eq!(tree.root_node().end_byte(), text.len());
        assert!(nodes.iter().any(|n| n.kind() == "comment"
            && node_text(*n, text).trim_end() == "// offsetof(T, member[3])"));
        assert!(nodes.iter().any(|n| n.kind() == "string_literal"
            && node_text(*n, text) == "\"offsetof(T, member[3])\""));
    }
}
