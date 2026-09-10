//! Reason-required inline suppressions and policy application.

use fakematch_lint::config::{Config, Exception, Policy, PragmaApproval};
use fakematch_lint::finding::Finding;
use fakematch_lint::policy;
use fakematch_lint::scan::ScanError;
use fakematch_lint::scan_source;

fn reviewed_with(source: &str, policy: &Policy) -> Result<Vec<Finding>, ScanError> {
    let mut rows = scan_source(source, "src/game/test.c", &Config::default())?.findings;
    policy::apply(&mut rows, policy);
    Ok(rows)
}

fn reviewed(source: &str) -> Vec<Finding> {
    reviewed_with(source, &Policy::default()).unwrap_or_else(|e| panic!("{e}"))
}

fn refused(source: &str) -> bool {
    matches!(reviewed_with(source, &Policy::default()), Err(ScanError::Suppression(_)))
}

fn suppressed(rows: &[Finding]) -> Vec<bool> {
    rows.iter().map(|r| r.suppressed).collect()
}

#[test]
fn reasoned_game_statement_is_audited_not_deleted() {
    let source = "void f(){\n// lint-allow-next-line FM007: Packed color; checked the callee.\nFatalError(message, 0x8000); use(0x40);\n}";
    let rows = reviewed(source);
    assert_eq!(rows.len(), 2);
    assert_eq!(suppressed(&rows), [true, false]);
    assert_eq!(rows[0].review_reason.as_deref(), Some("Packed color; checked the callee."));
    assert_eq!(rows[0].suppression_source.as_deref(), Some("source-comment"));
    let inline = rows[0].inline_suppression.as_ref().unwrap();
    assert_eq!(inline.comment_line, 2);
    assert_eq!(inline.target_start_line, 3);
    assert_eq!(inline.target_end_line, 3);
    assert_eq!(inline.target_kind, "ast");
    assert_eq!(inline.target_sha256.len(), 64);
}

#[test]
fn multiline_declaration_is_one_unit_not_neighboring_code() {
    let source = "void f(){\n// lint-allow-next-line FM003: Device-observed state; verified volatility.\nvolatile int state =\n  0;\nvolatile int other=0;\n}";
    let rows = reviewed(source);
    assert_eq!(suppressed(&rows), [true, false]);
    assert_eq!(rows[0].inline_suppression.as_ref().unwrap().target_end_line, 4);
}

#[test]
fn specific_rules_only_and_multiple_rules() {
    let source = "int f(){\n// lint-allow-next-line FM001: Verified packed buffer alignment.\nreturn *(int*)(p+0x8);\n}";
    let rows = reviewed(source);
    assert!(rows.iter().find(|r| r.rule == "FM001").unwrap().suppressed);
    assert!(!rows.iter().find(|r| r.rule == "FM007").unwrap().suppressed);
    let rows = reviewed(&source.replace("FM001:", "FM001, FM007:"));
    assert!(rows.iter().all(|r| r.suppressed));
}

#[test]
fn direct_gnu_and_mwcc_assembly_are_explicitly_waivable() {
    for body in [
        "void f(){\n// lint-allow-next-line FM005: Required device ordering barrier.\nasm(\"sync\");\nasm(\"isync\");\n}",
        "// lint-allow-next-line FM005: Required platform register access.\nasm void f(void) {\n  sync\n  blr\n}\nasm void g(void) { blr }",
        "void f(){\n// lint-allow-next-line FM005: Required platform register access.\nasm { sync }\nasm { isync }\n}",
    ] {
        let rows: Vec<Finding> = reviewed(body).into_iter().filter(|r| r.rule == "FM005").collect();
        assert_eq!(suppressed(&rows), [true, false], "{body}");
    }
}

#[test]
fn macro_and_pragma_comments_remain_narrow() {
    let source = "// lint-allow-next-line FM005: Verified synchronization macro.\n#define BARRIER() asm(\"sync\")\n#define OTHER() asm(\"isync\")\n";
    assert_eq!(suppressed(&reviewed(source)), [true, false]);
    let source = "// lint-allow-next-line FM006: Verified platform packing directive.\n#pragma pack(4)\n#pragma pack()\n";
    let rows = reviewed(source);
    assert_eq!(suppressed(&rows), [true, false]);
    assert!(rows.iter().all(|r| r.severity == fakematch_lint::config::Severity::Warning));
}

#[test]
fn single_line_block_comment_is_supported() {
    let source = "void f(){\n/* lint-allow-next-line FM007: Verified packed API value. */\nuse(0x40);\n}";
    assert!(reviewed(source)[0].suppressed);
}

#[test]
fn comment_text_in_strings_does_not_waive_findings() {
    let source = "void f(){use(\"// lint-allow-next-line FM007: pretend\");use(0x40);}";
    let rows = reviewed(source);
    assert!(!rows.is_empty());
    assert!(rows.iter().all(|r| !r.suppressed));
}

#[test]
fn malformed_stale_and_overbroad_comments_refuse() {
    let invalid = [
        "// lint-allow-next-line FM007:\nuse(0x40);",
        "// lint-allow-next-line FM007:   \nuse(0x40);",
        "// lint-allow-next-line FM999: unknown\nuse(0x40);",
        "// lint-allow-next-line FM000: waive scanner\nuse(0x40);",
        "// lint-allow-next-line FM008: waive patcher\nuse(0x40);",
        "// lint-allow-next-line FM007,FM007: repeated\nuse(0x40);",
        "// lint-disable FM007: blanket\nuse(0x40);",
        "// lint-allow-next-line *: blanket\nuse(0x40);",
        "// gdl-lint-allow-next-line FM007: old prefix\nuse(0x40);",
        "// lint-allow-next-line FM007: stale\nuse(1);",
        "// lint-allow-next-line FM001,FM007: partly stale\nuse(0x40);",
        "// lint-allow-next-line FM007: gap\n\nuse(0x40);",
        "// lint-allow-next-line FM007: gap\n// another comment\nuse(0x40);",
        "use(1); // lint-allow-next-line FM007: not standalone\nuse(0x40);",
        "// lint-allow-next-line FM007: block-wide\n{use(0x40);use(0x80);}",
        "// lint-allow-next-line FM007: whole function\nvoid nested(){use(0x40);}",
        "// lint-allow-next-line FM007: hidden function\nauto fn=[](){use(0x40);};",
        "// lint-allow-next-line FM005,FM007: asm body\nasm { sync }",
    ];
    for body in invalid {
        assert!(refused(&format!("void f(){{\n{body}\n}}")), "{body}");
    }
    assert!(refused("// lint-allow-next-line FM005: unclosed\nasm { sync"));
}

#[test]
fn region_is_explicit_bounded_reasoned_and_rule_specific() {
    let source = "void f(){\nuse(0x10);\n// lint-begin FM007: Verified packed values in this sequence.\nuse(0x20);\nasm(\"sync\");\nuse(0x30);\n// lint-end FM007\nuse(0x40);\n}";
    let rows = reviewed(source);
    assert_eq!(suppressed(&rows), [false, true, false, true, false]);
    let inline = rows[1].inline_suppression.as_ref().unwrap();
    assert_eq!(inline.target_kind, "region");
    assert_eq!(inline.comment_line, 3);
    assert_eq!(inline.end_comment_line, Some(7));
    let source = source
        .replace("lint-begin FM007:", "lint-begin FM005, FM007:")
        .replace("lint-end FM007", "lint-end FM007, FM005");
    assert_eq!(suppressed(&reviewed(&source)), [false, true, true, true, false]);
    assert_eq!(suppressed(&reviewed(&source.replace('\n', "\r\n"))), [false, true, true, true, false]);
}

#[test]
fn bad_region_pairs_and_overlapping_waivers_refuse() {
    let bad = [
        "// lint-begin FM007: reason\nuse(0x40);",
        "// lint-end FM007\nuse(0x40);",
        "// lint-begin FM007: reason\nuse(0x40);\n// lint-end FM005",
        "// lint-begin FM007:\nuse(0x40);\n// lint-end FM007",
        "// lint-begin FM007: reason\nuse(0x40);\n// lint-end FM007: extra",
        "// lint-begin FM007: reason\nuse(1);\n// lint-end FM007",
        "// lint-begin FM007: reason\n// lint-begin FM005: nested\nasm(\"sync\");\n// lint-end FM005\n// lint-end FM007",
        "// lint-begin FM007: reason\n// lint-allow-next-line FM007: overlapping\nuse(0x40);\n// lint-end FM007",
        "// lint-begin FM008: patcher\nuse(0x40);\n// lint-end FM008",
    ];
    for body in bad {
        assert!(refused(&format!("void f(){{\n{body}\n}}")), "{body}");
    }
}

#[test]
fn stale_comment_failure_names_the_source_line() {
    let err = reviewed_with(
        "void f(){\n// lint-allow-next-line FM007: stale reason\nuse(1);\n}",
        &Policy::default(),
    )
    .unwrap_err();
    match err {
        ScanError::Suppression(e) => {
            assert_eq!(e.path, "src/game/test.c");
            assert_eq!(e.line, 2);
            assert!(e.message.contains("unused rule"));
        }
        other => panic!("unexpected {other}"),
    }
}

#[test]
fn file_waiver_is_rule_specific_reasoned_and_file_local() {
    let source = "/* Copyright notice */\n// lint-file FM005: Verified platform primitives, not copied game logic.\nvoid f(){asm(\"sync\");use(0x40);}\nvoid g(){asm(\"isync\");}";
    let rows = reviewed(source);
    let asm: Vec<bool> = rows.iter().filter(|r| r.rule == "FM005").map(|r| r.suppressed).collect();
    assert_eq!(asm, [true, true]);
    assert!(!rows.iter().find(|r| r.rule == "FM007").unwrap().suppressed);
    let inline = rows[0].inline_suppression.as_ref().unwrap();
    assert_eq!(inline.target_kind, "file");
    assert_eq!(inline.comment_line, 2);
    assert!(!reviewed("void f(){asm(\"sync\");}")[0].suppressed);
}

#[test]
fn file_waiver_rejects_late_blank_unknown_and_overlapping_entries() {
    let bad = [
        "// lint-file FM005:\nvoid f(){asm(\"sync\");}",
        "#include \"a.h\"\n// lint-file FM005: too late\nvoid f(){asm(\"sync\");}",
        "// lint-file FM008: forbidden\nvoid f(){asm(\"sync\");}",
        "// lint-file *: all\nvoid f(){asm(\"sync\");}",
        "// lint-file FM005: stale\nvoid f(){}",
        "// lint-file FM005: first\n// lint-file FM005: redundant\nvoid f(){asm(\"sync\");}",
        "// lint-file FM005: first\nvoid f(){\n// lint-begin FM005: redundant\nasm(\"sync\");\n// lint-end FM005\n}",
    ];
    for source in bad {
        assert!(refused(source), "{source}");
    }
}

#[test]
fn custom_prefix_is_honoured() {
    let mut config = Config::default();
    config.suppressions.prefix = "debt-".into();
    let source = "void f(){\n// debt-allow-next-line FM007: Packed color.\nuse(0x40);\n}";
    let mut rows = scan_source(source, "src/game/a.c", &config).unwrap().findings;
    policy::apply(&mut rows, &Policy::default());
    assert!(rows[0].suppressed);
    // The old spelling is now an ordinary comment.
    let source = "void f(){\n// lint-allow-next-line FM007: Packed color.\nuse(0x40);\n}";
    let rows = scan_source(source, "src/game/a.c", &config).unwrap().findings;
    assert!(rows[0].inline_suppression.is_none());
}

// ---------------------------------------------------------------------------
// Policy
// ---------------------------------------------------------------------------

#[test]
fn reviewed_findings_remain_visible() {
    let rows = scan_source("int f(){return *(int*)(p+8);}", "src/game/test.c", &Config::default()).unwrap().findings;
    let policy = Policy {
        exceptions: vec![Exception { fingerprint: rows[0].fingerprint.clone(), reason: "Verified partial view; target offset 8.".into() }],
        ..Default::default()
    };
    let result = reviewed_with("int f(){return *(int*)(p+8);}", &policy).unwrap();
    assert!(result[0].suppressed);
    assert!(result[0].review_reason.is_some());
    let changed = reviewed_with("int f(){return *(int*)(p+12);}", &policy).unwrap();
    assert!(!changed[0].suppressed);
}

#[test]
fn legacy_pragma_approval_cannot_hide_warnings() {
    let policy = Policy {
        pragma_allowlist: vec![PragmaApproval {
            path: "src/game/test.c".into(),
            scope: "f".into(),
            directive: "#pragma scheduling off".into(),
            count: 1,
            reason: "Explicitly reviewed.".into(),
        }],
        ..Default::default()
    };
    let rows = reviewed_with(
        "void f(){\n#pragma scheduling off\n#pragma scheduling off\n}\nvoid g(){\n#pragma scheduling off\n}",
        &policy,
    )
    .unwrap();
    assert_eq!(rows.len(), 3);
    assert!(rows.iter().all(|r| r.severity == fakematch_lint::config::Severity::Warning && !r.suppressed));
}

#[test]
fn exceptions_do_not_hide_pragmas() {
    let source = "#pragma dont_inline on\n#pragma scheduling off\nvoid f(){}";
    let rows = scan_source(source, "src/game/a.c", &Config::default()).unwrap().findings;
    let policy = Policy {
        exceptions: vec![Exception { fingerprint: rows[0].fingerprint.clone(), reason: "Old exemption".into() }],
        ..Default::default()
    };
    let mut rows = scan_source(source, "src/game/a.c", &Config::default()).unwrap().findings;
    policy::apply(&mut rows, &policy);
    assert!(rows.iter().all(|r| r.severity == fakematch_lint::config::Severity::Warning && !r.suppressed));
}

#[test]
fn macro_assembly_approval_does_not_allow_changed_body_or_direct_asm() {
    let source = "#define WAIT() asm(\"nop\")\n";
    let rows = scan_source(source, "a.h", &Config::default()).unwrap().findings;
    let mut policy = Policy {
        exceptions: vec![Exception { fingerprint: rows[0].fingerprint.clone(), reason: "Verified macro only.".into() }],
        ..Default::default()
    };
    let mut rows = scan_source(source, "a.h", &Config::default()).unwrap().findings;
    policy::apply(&mut rows, &policy);
    assert!(rows[0].suppressed);
    let mut changed = scan_source(&source.replace("nop", "blr"), "a.h", &Config::default()).unwrap().findings;
    policy::apply(&mut changed, &policy);
    assert!(!changed[0].suppressed);
    let mut direct = scan_source("asm(\"nop\");", "src/game/a.c", &Config::default()).unwrap().findings;
    policy.exceptions = vec![Exception { fingerprint: direct[0].fingerprint.clone(), reason: "Not a permitted macro.".into() }];
    policy::apply(&mut direct, &policy);
    assert!(!direct[0].suppressed);
}
