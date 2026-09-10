//! Offset-preserving C/C++ token scanner.
//!
//! The scanner never expands macros or resolves types. It exists so that the
//! rules can distinguish code from comments and string literals, locate
//! preprocessor directives (with continuation lines), and produce the
//! macro-body "projection" that is parsed separately from the main source.

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum TokKind {
    Comment,
    Str,
    Id,
    Number,
    Space,
    Op,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Token {
    pub kind: TokKind,
    pub start: usize,
    pub end: usize,
}

impl Token {
    pub fn text<'a>(&self, text: &'a str) -> &'a str {
        &text[self.start..self.end]
    }
}

fn is_id_start(b: u8) -> bool {
    b.is_ascii_alphabetic() || b == b'_' || b == b'$' || b >= 0x80
}

fn is_id_continue(b: u8) -> bool {
    b.is_ascii_alphanumeric() || b == b'_' || b == b'$' || b >= 0x80
}

fn is_space(b: u8) -> bool {
    matches!(b, b' ' | b'\t' | b'\n' | b'\r' | 0x0b | 0x0c)
}

/// Length of a backslash + optional CR + LF continuation at `i`, or 0.
fn continuation_len(b: &[u8], i: usize) -> usize {
    if b.get(i) != Some(&b'\\') {
        return 0;
    }
    match (b.get(i + 1), b.get(i + 2)) {
        (Some(b'\n'), _) => 2,
        (Some(b'\r'), Some(b'\n')) => 3,
        _ => 0,
    }
}

fn utf8_len(first: u8) -> usize {
    if first < 0x80 {
        1
    } else if first >= 0xF0 {
        4
    } else if first >= 0xE0 {
        3
    } else if first >= 0xC0 {
        2
    } else {
        1
    }
}

fn scan_comment(b: &[u8], i: usize) -> Option<usize> {
    if b.get(i) != Some(&b'/') {
        return None;
    }
    match b.get(i + 1) {
        Some(b'/') => {
            let mut j = i + 2;
            while j < b.len() {
                let c = continuation_len(b, j);
                if c > 0 {
                    j += c;
                    continue;
                }
                if b[j] == b'\n' {
                    break;
                }
                j += 1;
            }
            Some(j)
        }
        Some(b'*') => {
            let mut j = i + 2;
            while j + 1 < b.len() {
                if b[j] == b'*' && b[j + 1] == b'/' {
                    return Some(j + 2);
                }
                j += 1;
            }
            None
        }
        _ => None,
    }
}

fn string_prefix_len(b: &[u8], i: usize) -> usize {
    // Longest prefix first: u8 before u.
    let rest = &b[i..];
    for prefix in [&b"u8"[..], b"u", b"U", b"L"] {
        if rest.starts_with(prefix) {
            let after = rest.get(prefix.len());
            if matches!(after, Some(b'"') | Some(b'\'') | Some(b'R')) {
                return prefix.len();
            }
        }
    }
    0
}

fn scan_string_at(b: &[u8], start: usize) -> Option<usize> {
    let quote = *b.get(start)?;
    if quote == b'R' {
        if b.get(start + 1) != Some(&b'"') {
            return None;
        }
        let delim_start = start + 2;
        let mut j = delim_start;
        while j < b.len() && j - delim_start <= 16 {
            let c = b[j];
            if c == b'(' {
                break;
            }
            if matches!(c, b' ' | b')' | b'\\' | b'\t' | b'\r' | b'\n') {
                return None;
            }
            j += 1;
        }
        if b.get(j) != Some(&b'(') || j - delim_start > 16 {
            return None;
        }
        let delim = &b[delim_start..j];
        let mut k = j + 1;
        while k < b.len() {
            if b[k] == b')' && b[k + 1..].starts_with(delim) && b.get(k + 1 + delim.len()) == Some(&b'"') {
                return Some(k + 2 + delim.len());
            }
            k += 1;
        }
        return None;
    }
    if quote != b'"' && quote != b'\'' {
        return None;
    }
    let mut j = start + 1;
    while j < b.len() {
        if b[j] == b'\\' {
            j += 1 + utf8_len(*b.get(j + 1).unwrap_or(&b' '));
            continue;
        }
        if b[j] == quote {
            return Some(j + 1);
        }
        j += 1;
    }
    None
}

fn scan_string(b: &[u8], i: usize) -> Option<usize> {
    let prefix = string_prefix_len(b, i);
    scan_string_at(b, i + prefix)
}

fn scan_number(b: &[u8], i: usize) -> Option<usize> {
    if !b.get(i)?.is_ascii_digit() {
        return None;
    }
    let mut j = i;
    if b[i] == b'0'
        && matches!(b.get(i + 1), Some(b'x') | Some(b'X'))
        && b.get(i + 2).is_some_and(|c| c.is_ascii_hexdigit())
    {
        j = i + 2;
        while j < b.len() && b[j].is_ascii_hexdigit() {
            j += 1;
        }
        while j < b.len() && matches!(b[j], b'u' | b'U' | b'l' | b'L') {
            j += 1;
        }
        return Some(j);
    }
    while j < b.len() && b[j].is_ascii_digit() {
        j += 1;
    }
    if b.get(j) == Some(&b'.') {
        j += 1;
        while j < b.len() && b[j].is_ascii_digit() {
            j += 1;
        }
    }
    if matches!(b.get(j), Some(b'e') | Some(b'E')) {
        let mut k = j + 1;
        if matches!(b.get(k), Some(b'+') | Some(b'-')) {
            k += 1;
        }
        if b.get(k).is_some_and(|c| c.is_ascii_digit()) {
            while k < b.len() && b[k].is_ascii_digit() {
                k += 1;
            }
            j = k;
        }
    }
    while j < b.len() && matches!(b[j], b'u' | b'U' | b'l' | b'L' | b'f' | b'F') {
        j += 1;
    }
    Some(j)
}

/// Tokenize the complete text, including comments and whitespace.
pub fn tokenize(text: &str) -> Vec<Token> {
    let b = text.as_bytes();
    let mut out = Vec::with_capacity(b.len() / 3);
    let mut i = 0;
    while i < b.len() {
        let (kind, end) = if let Some(end) = scan_comment(b, i) {
            (TokKind::Comment, end)
        } else if let Some(end) = scan_string(b, i) {
            (TokKind::Str, end)
        } else if is_id_start(b[i]) {
            let mut j = i + utf8_len(b[i]);
            while j < b.len() && is_id_continue(b[j]) {
                j += utf8_len(b[j]);
            }
            (TokKind::Id, j.min(b.len()))
        } else if let Some(end) = scan_number(b, i) {
            (TokKind::Number, end)
        } else if is_space(b[i]) {
            let mut j = i + 1;
            while j < b.len() && is_space(b[j]) {
                j += 1;
            }
            (TokKind::Space, j)
        } else {
            let two = &b[i..(i + 2).min(b.len())];
            let len = if matches!(two, b"::" | b"->" | b"&=" | b"|=" | b"^=" | b"&&" | b"||") {
                2
            } else {
                utf8_len(b[i])
            };
            (TokKind::Op, (i + len).min(b.len()))
        };
        out.push(Token { kind, start: i, end });
        i = end;
    }
    out
}

/// A preprocessor directive, including any continuation lines.
#[derive(Clone, Debug)]
pub struct Directive {
    /// Start of the line that holds the directive (leading whitespace included).
    pub start: usize,
    /// Position of the `#` character.
    pub hash: usize,
    pub end: usize,
    /// Continuations joined and whitespace collapsed.
    pub text: String,
    /// `#define` name, if any.
    pub macro_name: Option<String>,
    /// Whether the macro is function-like (`#define F(x) ...`).
    pub function_like: bool,
    /// Start of the macro body (after the name and parameter list).
    pub body_start: usize,
}

impl Directive {
    /// The directive keyword (`pragma`, `define`, `include`, ...).
    pub fn keyword(&self) -> &str {
        let t = self.text.trim_start_matches('#').trim_start();
        let end = t
            .find(|c: char| !(c.is_ascii_alphanumeric() || c == '_'))
            .unwrap_or(t.len());
        &t[..end]
    }

    pub fn is_pragma(&self) -> bool {
        self.keyword() == "pragma"
    }
}

/// Lexical facts about one source file, all expressed in byte offsets of the
/// original text so they line up with the parser.
#[derive(Debug)]
pub struct Lexed {
    /// Code tokens only (no comments, no whitespace).
    pub tokens: Vec<Token>,
    /// Every token, including comments and whitespace.
    pub all_tokens: Vec<Token>,
    /// The text with comments and string literals blanked (newlines kept).
    pub clean: Vec<u8>,
    /// The text with everything but `#define` bodies blanked; continuation
    /// backslashes are removed without changing offsets.
    pub projection: String,
    pub directives: Vec<Directive>,
    /// Byte offset of each line start; index 0 is 0.
    pub line_starts: Vec<usize>,
}

impl Lexed {
    pub fn new(text: &str) -> Lexed {
        let b = text.as_bytes();
        let all_tokens = tokenize(text);
        let mut clean = b.to_vec();
        for t in &all_tokens {
            if matches!(t.kind, TokKind::Comment | TokKind::Str) {
                for c in &mut clean[t.start..t.end] {
                    if *c != b'\n' {
                        *c = b' ';
                    }
                }
            }
        }
        let tokens: Vec<Token> = all_tokens
            .iter()
            .copied()
            .filter(|t| !matches!(t.kind, TokKind::Comment | TokKind::Space))
            .collect();
        let mut line_starts = vec![0];
        line_starts.extend(
            b.iter()
                .enumerate()
                .filter(|(_, c)| **c == b'\n')
                .map(|(i, _)| i + 1),
        );

        let mut directives = Vec::new();
        let mut projection: Vec<u8> = b
            .iter()
            .map(|c| if *c == b'\n' { b'\n' } else { b' ' })
            .collect();
        for &ls in &line_starts {
            let mut i = ls;
            while i < clean.len() && (clean[i] == b' ' || clean[i] == b'\t') {
                i += 1;
            }
            if clean.get(i) != Some(&b'#') {
                continue;
            }
            let hash = i;
            let mut j = i;
            while j < clean.len() {
                let c = continuation_len(&clean, j);
                if c > 0 {
                    j += c;
                    continue;
                }
                if clean[j] == b'\n' {
                    break;
                }
                j += 1;
            }
            let end = j;
            let original = &text[ls..end];
            let joined = join_continuations(original);
            let collapsed = collapse_whitespace(&joined);
            let (macro_name, function_like, body_start) = parse_define(original, ls);
            if macro_name.is_some() {
                projection[body_start..end].copy_from_slice(&b[body_start..end]);
            }
            directives.push(Directive {
                start: ls,
                hash,
                end,
                text: collapsed,
                macro_name,
                function_like,
                body_start,
            });
        }
        let mut k = 0;
        while k < projection.len() {
            let c = continuation_len(&projection, k);
            if c > 0 {
                projection[k] = b' ';
                k += c;
            } else {
                k += 1;
            }
        }
        let projection =
            String::from_utf8(projection).expect("projection copies whole UTF-8 macro bodies");
        Lexed {
            tokens,
            all_tokens,
            clean,
            projection,
            directives,
            line_starts,
        }
    }

    /// 1-based line number for a byte offset.
    pub fn line_of(&self, offset: usize) -> usize {
        self.line_starts.partition_point(|&s| s <= offset).max(1)
    }

    /// 1-based column, counted in characters, for a byte offset.
    pub fn column_of(&self, text: &str, offset: usize) -> usize {
        let line = self.line_of(offset);
        let start = self.line_starts[line - 1];
        text[start..offset].chars().count() + 1
    }

    /// Byte offset of the first character of a 1-based line.
    pub fn line_start(&self, line: usize) -> usize {
        self.line_starts[line - 1]
    }

    pub fn line_count(&self) -> usize {
        self.line_starts.len()
    }

    pub fn macro_ranges(&self) -> impl Iterator<Item = (usize, usize, bool)> + '_ {
        self.directives
            .iter()
            .filter(|d| d.macro_name.is_some())
            .map(|d| (d.body_start, d.end, d.function_like))
    }

    pub fn in_macro_body(&self, offset: usize) -> Option<bool> {
        self.macro_ranges()
            .find(|&(a, b, _)| a <= offset && offset < b)
            .map(|(_, _, function_like)| function_like)
    }

    pub fn directive_at(&self, offset: usize) -> Option<&Directive> {
        self.directives
            .iter()
            .find(|d| d.start <= offset && offset < d.end)
    }

    /// Index of the last code token starting at or before `offset`.
    pub fn token_index_at(&self, offset: usize) -> usize {
        self.tokens
            .partition_point(|t| t.start <= offset)
            .saturating_sub(1)
    }
}

fn join_continuations(s: &str) -> String {
    let mut out = String::with_capacity(s.len());
    let b = s.as_bytes();
    let mut i = 0;
    while i < b.len() {
        let c = continuation_len(b, i);
        if c > 0 {
            out.push(' ');
            i += c;
        } else {
            let len = utf8_len(b[i]).min(b.len() - i);
            out.push_str(&s[i..i + len]);
            i += len;
        }
    }
    out
}

pub fn collapse_whitespace(s: &str) -> String {
    let mut out = String::with_capacity(s.len());
    let mut in_space = false;
    for ch in s.chars() {
        if ch.is_whitespace() {
            if !in_space {
                out.push(' ');
                in_space = true;
            }
        } else {
            out.push(ch);
            in_space = false;
        }
    }
    out.trim().to_string()
}

/// Recognise `#define NAME` / `#define NAME(params)` at the start of a directive.
fn parse_define(original: &str, base: usize) -> (Option<String>, bool, usize) {
    let b = original.as_bytes();
    let mut i = 0;
    while i < b.len() && (b[i] == b' ' || b[i] == b'\t') {
        i += 1;
    }
    if b.get(i) != Some(&b'#') {
        return (None, false, base);
    }
    i += 1;
    while i < b.len() && b[i].is_ascii_whitespace() {
        i += 1;
    }
    if !b[i..].starts_with(b"define") {
        return (None, false, base);
    }
    i += 6;
    let mut j = i;
    while j < b.len() && b[j].is_ascii_whitespace() {
        j += 1;
    }
    if j == i || j >= b.len() || !(b[j].is_ascii_alphanumeric() || b[j] == b'_') {
        return (None, false, base);
    }
    let name_start = j;
    while j < b.len() && (b[j].is_ascii_alphanumeric() || b[j] == b'_') {
        j += 1;
    }
    let name = original[name_start..j].to_string();
    let mut function_like = false;
    if b.get(j) == Some(&b'(') {
        let mut k = j + 1;
        while k < b.len() && b[k] != b')' && b[k] != b'\n' {
            k += 1;
        }
        if b.get(k) == Some(&b')') {
            function_like = true;
            j = k + 1;
        }
    }
    (Some(name), function_like, base + j)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn kinds(text: &str) -> Vec<(TokKind, &str)> {
        tokenize(text)
            .into_iter()
            .map(|t| (t.kind, t.text(text)))
            .collect()
    }

    #[test]
    fn comments_strings_and_operators() {
        let k = kinds("a->b // c\\\nd\n/* e */ \"s\\\"t\" 'c' 0x1Fu 1.5f");
        assert_eq!(k[0], (TokKind::Id, "a"));
        assert_eq!(k[1], (TokKind::Op, "->"));
        assert_eq!(k[3], (TokKind::Space, " "));
        assert_eq!(k[4], (TokKind::Comment, "// c\\\nd"));
        assert_eq!(k[6], (TokKind::Comment, "/* e */"));
        assert_eq!(k[8], (TokKind::Str, "\"s\\\"t\""));
        assert_eq!(k[10], (TokKind::Str, "'c'"));
        assert_eq!(k[12], (TokKind::Number, "0x1Fu"));
        assert_eq!(k[14], (TokKind::Number, "1.5f"));
    }

    #[test]
    fn raw_and_prefixed_strings() {
        let k = kinds("R\"tag(a)\" )tag\" u8\"x\" L'c' u8x");
        assert_eq!(k[0], (TokKind::Str, "R\"tag(a)\" )tag\""));
        assert_eq!(k[2], (TokKind::Str, "u8\"x\""));
        assert_eq!(k[4], (TokKind::Str, "L'c'"));
        assert_eq!(k[6], (TokKind::Id, "u8x"));
    }

    #[test]
    fn directives_and_projection() {
        let text = "#define F(p) \\\n  (*(int*)((p) + 0x18))\n#define SIZE 0x20\nint x;\n";
        let lexed = Lexed::new(text);
        assert_eq!(lexed.directives.len(), 2);
        let f = &lexed.directives[0];
        assert_eq!(f.macro_name.as_deref(), Some("F"));
        assert!(f.function_like);
        assert_eq!(f.text, "#define F(p) (*(int*)((p) + 0x18))");
        assert!(!lexed.directives[1].function_like);
        assert_eq!(lexed.projection.len(), text.len());
        assert!(lexed.projection.contains("(*(int*)((p) + 0x18))"));
        assert!(!lexed.projection.contains("int x"));
        assert!(!lexed.projection.contains('\\'));
    }

    #[test]
    fn lines_and_columns_count_characters() {
        let text = "// café λ\r\nint f(){return *(int*)(p+8);}";
        let lexed = Lexed::new(text);
        let offset = text.find("*(int*)").unwrap();
        assert_eq!(lexed.line_of(offset), 2);
        assert_eq!(lexed.column_of(text, offset), 16);
    }
}
