//! Two-sided source-debt classification, ported from the retired Python suite.

use fakematch_lint::config::Config;
use fakematch_lint::finding::{Confidence, Finding};
use fakematch_lint::scan_source;

fn rules(source: &str) -> Vec<Finding> {
    scan_source(source, "src/game/test.c", &Config::default())
        .unwrap_or_else(|e| panic!("scan failed: {e}"))
        .findings
}

fn hits(source: &str, rule: &str) -> Vec<Finding> {
    rules(source).into_iter().filter(|r| r.rule == rule).collect()
}

fn variables(rows: &[Finding]) -> Vec<String> {
    rows.iter().map(|r| r.detail_str("variable").unwrap().to_string()).collect()
}

#[test]
fn all_raw_offset_forms() {
    for expr in [
        "*(T*)(p + K)",
        "((T*)p)[K]",
        "*(T*)((u8*)p + 0x8)",
        "*((T*)(p + 8))",
        "((const struct Thing *)p)[2]",
        "*(const struct Thing*)(p - 8)",
        "*(void**)((u8*)p + 0x18)",
    ] {
        assert!(!hits(&format!("int f() {{ return {expr}; }}"), "FM001").is_empty(), "{expr}");
    }
}

#[test]
fn real_field_and_array_are_not_raw_casts() {
    for expr in ["p->field", "p[i]", "*(T*)p", "*p", "a * b", "(a*b)[2]", "*(int*)(p)"] {
        assert!(hits(&format!("int f() {{ return {expr}; }}"), "FM001").is_empty(), "{expr}");
    }
}

#[test]
fn numeric_pointer_offsets_include_decimal_and_both_sides() {
    for expr in ["strings + 364", "strings - 364", "364 + strings", "strings + (0x16Cu)"] {
        let rows = hits(&format!("void f(char* strings){{use({expr});}}"), "FM009");
        assert_eq!(rows.len(), 1, "{expr}");
        assert_eq!(rows[0].detail_str("base"), Some("strings"));
        assert_eq!(rows[0].detail_u64("declaration_line"), Some(1));
    }
    assert!(!hits("extern char names[]; void f(){use(names + 364);}", "FM009").is_empty());
}

#[test]
fn scalar_arithmetic_and_unknown_types_are_not_pointer_claims() {
    for source in [
        "int f(int strings){return strings+364;}",
        "void f(){int strings=0;use(strings+364);}",
        "void f(){use(unknown+364);}",
        "void f(){int* ptr, scalar;use(scalar+364);}",
        "void f(char* strings){use(strings+OFFSET);use(strings[i]);}",
        "void f(){/* strings + 364 */ use(\"strings + 364\");}",
        "void f(char* strings){use(strings + 1.5f);}",
    ] {
        assert!(hits(source, "FM009").is_empty(), "{source}");
    }
}

#[test]
fn pointer_scope_shadowing_is_respected() {
    for source in [
        "char* p; int f(int p){return p+364;}",
        "char* p; void f(){int p=0;use(p+364);}",
        "void f(){ {char* p;use(p);} use(p+364);}",
        "void f(char* p){{int p=0;use(p+364);}}",
    ] {
        assert!(hits(source, "FM009").is_empty(), "{source}");
    }
    assert_eq!(hits("char* p; void f(){{int p=0;use(p+364);} use(p+364);}", "FM009").len(), 1);
}

#[test]
fn texture_pool_regression_and_editor_locations() {
    let mut source = String::from("void f(){\n char* strings=pool;\n");
    for offset in (364..473).step_by(12) {
        source.push_str(&format!(" out=MBOX_FindTexture(strings + {offset}, 0);\n"));
    }
    source.push_str("}\n");
    let rows = hits(&source, "FM009");
    assert_eq!(rows.iter().map(|r| r.line).collect::<Vec<_>>(), (3..13).collect::<Vec<_>>());
    for row in &rows {
        assert!(row.message.contains("does not prove a struct or loop"));
        assert_eq!(row.scope, "f");
    }
}

#[test]
fn declaration_scope_in_for_loop_does_not_leak() {
    assert!(hits("void f(){for(char* p=0;p;){use(p);} use(p+364);}", "FM009").is_empty());
    assert_eq!(
        hits("char* p; void f(){for(int p=0;p<2;p++){use(p+364);} use(p+364);}", "FM009").len(),
        1
    );
}

#[test]
fn nested_dereference_threshold() {
    let rows = hits("int f() { return *(int*)*(void**)(p + 0x8); }", "FM002");
    assert_eq!(rows.len(), 1);
    assert_eq!(rows[0].detail_u64("depth"), Some(2));
    assert!(hits("int f() { return *(int*)(p + 8); }", "FM002").is_empty());
}

#[test]
fn comments_strings_chars_and_raw_strings_are_not_code() {
    let s = r#"// *(int*)(p+4) asm volatile trash[8] 0xCAFE
        /* ((u32*)p)[2] */
        const char* s="*(int*)(p+4) asm 0xCAFE";
        const char* t=R"tag(asm *(int*)(p+4)
        #pragma optimize off
        )tag";
        int f() { return 'x'; }
        "#;
    assert!(rules(s).is_empty());
}

#[test]
fn multiline_cast_location() {
    let rows = hits("int f() {\n return *(int*)\n ((u8*)p + 12);\n}", "FM001");
    assert_eq!(rows[0].line, 2);
    assert_eq!(rows[0].scope, "f");
}

#[test]
fn function_local_declarations() {
    let rows = hits(
        "u8 global[16]; volatile int device; void f() { u8 scratch[8]; volatile int home; int trash7; u8 used[3]; use(used); }",
        "FM003",
    );
    let mut vars = variables(&rows);
    vars.sort();
    assert_eq!(vars, ["home", "scratch", "trash7"]);
}

#[test]
fn multiple_local_arrays() {
    let rows = hits("void f() { u8 first[2], second[2]; use(first); }", "FM003");
    assert_eq!(variables(&rows), ["second"]);
}

#[test]
fn index_writes_and_calls_are_not_declarations() {
    assert!(hits("void f() { data[4] = 1; use(trash); }", "FM003").is_empty());
}

#[test]
fn initializer_reference_is_not_a_trash_declarator() {
    assert!(hits("void f(){ int real = trash; int other = use(unused); }", "FM003").is_empty());
}

#[test]
fn volatile_pointer_local() {
    let rows = hits("void f(){ int * volatile home = p; }", "FM003");
    assert_eq!(variables(&rows), ["home"]);
}

#[test]
fn dead_but_incremented_counter() {
    let rows = hits(
        "void f(){ int timeOffset; int i; timeOffset=0; i=0; while(i<4){ use(i); i++; timeOffset+=4; } }",
        "FM003",
    );
    let write_only: Vec<_> = rows
        .iter()
        .filter(|r| r.detail_str("pattern") == Some("write-only-local"))
        .collect();
    assert_eq!(write_only.len(), 1);
    assert_eq!(write_only[0].detail_str("variable"), Some("timeOffset"));
    assert_eq!(write_only[0].confidence, Confidence::Heuristic);
}

#[test]
fn observed_updates_and_volatile_are_not_dead_counter_claims() {
    for source in [
        "int f(){int i=0; i++; return i;}",
        "void f(){int i=0; use(i++);}",
        "void f(){volatile int i=0; i++;}",
        "void f(){static int i=0; i++;}",
        "void f(){int i=0; i++; {int i=2; use(i);} }",
    ] {
        let rows = hits(source, "FM003");
        assert!(
            !rows.iter().any(|r| r.detail_str("pattern") == Some("write-only-local")),
            "{source}"
        );
    }
}

#[test]
fn float_and_pointer_shaped_byte_arrays() {
    for items in ["0x3f,0x80,0,0,0x40,0,0,0", "0x80,0,0,4,0x80,0,0,8,0x80,0,0,12"] {
        let rows = hits(&format!("static const u8 data[] = {{{items}}};"), "FM004");
        assert_eq!(rows.len(), 1, "{items}");
        assert_eq!(rows[0].confidence, Confidence::Heuristic);
        assert_eq!(rows[0].detail_str("variable"), Some("data"));
    }
}

#[test]
fn zero_and_ordinary_bytes_not_float_blob() {
    for items in ["0,0,0,0,0,0,0,0", "0x89,0x50,0x4e,0x47,0xd,0xa,0x1a,0xa"] {
        assert!(hits(&format!("static const u8 data[] = {{{items}}};"), "FM004").is_empty());
    }
    assert!(hits("static const float values[]={1.0f,2.0f};", "FM004").is_empty());
}

#[test]
fn asm_and_macro_alias() {
    assert!(!hits("asm void f() { blr }", "FM005").is_empty());
    assert!(!hits("void f() { __asm__ volatile (\"nop\"); }", "FM005").is_empty());
    assert!(!hits("ASM void f() { blr }", "FM005").is_empty());
    let rows = hits("#define ASM asm\n", "FM005");
    assert_eq!(rows.len(), 1);
    assert_eq!(rows[0].detail_str("macro"), Some("ASM"));
    assert!(hits("void f(){ use(\"asm(nop)\"); }", "FM005").is_empty());
}

#[test]
fn pragma_scopes_and_attributes() {
    let rows = hits(
        "#pragma once\n#pragma scheduling off\nvoid f() {\n#pragma optimization_level 0\n}\n",
        "FM006",
    );
    assert_eq!(rows.len(), 2);
    assert_eq!(rows[0].detail_str("pragma_scope"), Some("before:f"));
    assert_eq!(rows[0].detail_str("directive"), Some("#pragma scheduling off"));
    assert_eq!(rows[1].detail_str("pragma_scope"), Some("f"));
    assert!(rows.iter().all(|r| r.is_pragma()));
    let attr = hits("void __attribute__((optimize(\"O0\"))) f() {}", "FM006");
    assert_eq!(attr.len(), 1);
    assert!(!attr[0].is_pragma());
}

#[test]
fn hex_constants_masks_and_named_definitions() {
    let s = "#define CAPACITY 0x800
        enum { SIZE = 0x20 };
        const int LIMIT = 0x100;
        void f() { flags &= ~0x40; flags |= (0x20); out = flags & 0xff;
          call(0x123); offset += 0x24; }
        ";
    let excerpts: Vec<String> = hits(s, "FM007").into_iter().map(|r| r.excerpt).collect();
    assert_eq!(excerpts, ["0x123", "0x24"]);
}

#[test]
fn enemy_gravity_flag_is_a_legitimate_mask() {
    let source = "void f(){ if (mp != NULL && !(mp->flags & 0x1000)) { goto gravity; } gravity: return; }";
    assert!(hits(source, "FM007").is_empty());
}

#[test]
fn mask_exemption_does_not_hide_arithmetic_or_call_arguments() {
    for expression in [
        "p + 0x1000",
        "call(0x1000)",
        "flags & (0x1000 + offset)",
        "(0x1000 + offset) & flags",
        "flags & call(0x1000)",
    ] {
        assert!(!hits(&format!("int f(){{return {expression};}}"), "FM007").is_empty(), "{expression}");
    }
}

#[test]
fn mask_sides_updates_complements_and_parentheses() {
    for expression in [
        "flags & 0x1000",
        "0x1000 & flags",
        "flags | (0x1000u)",
        "flags ^= 0x1000",
        "flags &= ~0x1000",
        "flags & ((0x1000))",
    ] {
        assert!(hits(&format!("void f(){{{expression};}}"), "FM007").is_empty(), "{expression}");
    }
}

#[test]
fn macros_are_scanned_and_continued() {
    let s = "#define FIELD(p) \\\n  (*(int*)((p) + 0x18))\n";
    assert!(!hits(s, "FM001").is_empty());
    let excerpts: Vec<String> = hits(s, "FM007").into_iter().map(|r| r.excerpt).collect();
    assert_eq!(excerpts, ["0x18"]);
}

#[test]
fn stable_fingerprint_across_line_insertions() {
    let source = "int f() { return *(int*)(p+8); }";
    let a = hits(source, "FM001").remove(0);
    let b = hits(&format!("\n\n{source}"), "FM001").remove(0);
    assert_eq!(a.fingerprint, b.fingerprint);
    assert_ne!(a.line, b.line);
    assert_eq!(a.fingerprint.len(), 64);
}

#[test]
fn const_cast_is_not_named_constant() {
    assert_eq!(hits("void f(){ x = *(const int*)(p+0x20); }", "FM007").len(), 1);
    assert!(hits("static const u8 data[]={0x3f,0x80,0,0};", "FM007").is_empty());
}

#[test]
fn real_cpp_destructor_scope() {
    let rows = hits("Thing::~Thing(){ use(0x40); }", "FM007");
    assert_eq!(rows[0].scope, "Thing::~Thing");
}

#[test]
fn unicode_and_crlf_location() {
    let rows = hits("// café λ\r\nint f(){return *(int*)(p+8);}", "FM001");
    assert_eq!((rows[0].line, rows[0].column), (2, 16));
}

#[test]
fn parse_recovery_is_reported() {
    let result = scan_source("void f(){ @@@ return *(int*)(p+8); }", "src/game/a.c", &Config::default()).unwrap();
    assert!(!result.recovery.is_empty());
    assert_eq!(result.recovery[0].path, "src/game/a.c");
}

#[test]
fn whole_file_mwcc_asm_is_not_silently_skipped() {
    let result = scan_source(
        "#include \"types.h\"\nasm void f(){ nofralloc\n psq_l f0,0(src),0,qr0\n blr\n }",
        "src/game/a.c",
        &Config::default(),
    )
    .unwrap();
    assert!(!result.recovery.is_empty());
    assert!(result.findings.iter().any(|r| r.rule == "FM005"));
}

#[test]
fn empty_file_is_clean() {
    assert!(rules("").is_empty());
    assert!(rules("   \n\t\n").is_empty());
}

#[test]
fn object_macro_constant_exempt_but_expression_is_scanned() {
    assert!(hits("#define SIZE 0x20\n", "FM007").is_empty());
    assert!(!hits("#define FIELD(p) (*(int*)((p)+8))\n", "FM001").is_empty());
}

#[test]
fn no_macro_assembly_in_string_or_comment() {
    assert!(hits("#define NAME \"asm\" /* asm */\n", "FM005").is_empty());
}

#[test]
fn rules_can_be_disabled_and_tuned() {
    let mut config = Config::default();
    config.rules.fm007.enabled = false;
    config.rules.fm003.trash_pattern = "(?i)^junk\\w*$".into();
    let rows = scan_source("void f(){ int junk1; int trash0; use(0x40); }", "src/game/a.c", &config)
        .unwrap()
        .findings;
    assert!(rows.iter().all(|r| r.rule != "FM007"));
    assert_eq!(variables(&rows), ["junk1"]);
}

#[test]
fn findings_are_sorted_and_deduplicated() {
    let rows = rules("void f(){ use(0x40);\n use(0x40); }");
    assert_eq!(rows.len(), 2);
    assert!(rows[0].line < rows[1].line);
    assert_ne!(rows[0].fingerprint, rows[1].fingerprint);
}
