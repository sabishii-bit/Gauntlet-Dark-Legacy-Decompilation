//! In-process tree-sitter parsing and small node helpers.

use std::cell::RefCell;

use tree_sitter::{Node, Parser, Tree};

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
pub fn parse(text: &str) -> Tree {
    PARSER.with(|p| {
        let mut p = p.borrow_mut();
        p.reset();
        p.parse(text, None).expect("parser has a language and no cancellation")
    })
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
}
