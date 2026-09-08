"""Two-sided source-debt classification and CLI safety; no compiler required."""
import contextlib
import io
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

LINT_DIR = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('gdl_fakematch_lint', LINT_DIR / 'fakematch_lint.py')
lint = importlib.util.module_from_spec(spec)
spec.loader.exec_module(lint)


class SourceRules(unittest.TestCase):
    def rules(self, source):
        return lint.scan_source(source, 'src/game/test.c')

    def hits(self, source, rule):
        return [r for r in self.rules(source) if r['rule'] == rule]

    def test_all_raw_offset_forms(self):
        for expr in ('*(T*)(p + K)', '((T*)p)[K]', '*(T*)((u8*)p + 0x8)',
                     '*((T*)(p + 8))', '((const struct Thing *)p)[2]'):
            with self.subTest(expr=expr):
                self.assertTrue(self.hits('int f() { return '+expr+'; }', 'FM001'))

    def test_real_field_and_array_are_not_raw_casts(self):
        for expr in ('p->field', 'p[i]', '*(T*)p', '*p', 'a * b', '(a*b)[2]'):
            with self.subTest(expr=expr):
                self.assertFalse(self.hits('int f() { return '+expr+'; }', 'FM001'))

    def test_numeric_pointer_offsets_include_decimal_and_both_sides(self):
        for expr in ('strings + 364','strings - 364','364 + strings','strings + (0x16Cu)'):
            with self.subTest(expr=expr):
                rows=self.hits('void f(char* strings){use('+expr+');}','FM009')
                self.assertEqual(len(rows),1)
                self.assertEqual(rows[0]['base'],'strings')
        self.assertTrue(self.hits('extern char names[]; void f(){use(names + 364);}','FM009'))

    def test_numeric_scalar_arithmetic_and_unknown_types_are_not_pointer_claims(self):
        for source in ('int f(int strings){return strings+364;}',
                       'void f(){int strings=0;use(strings+364);}',
                       'void f(){use(unknown+364);}',
                       'void f(){int* ptr, scalar;use(scalar+364);}',
                       'void f(char* strings){use(strings+OFFSET);use(strings[i]);}',
                       'void f(){/* strings + 364 */ use("strings + 364");}'):
            with self.subTest(source=source):
                self.assertFalse(self.hits(source,'FM009'))

    def test_pointer_scope_shadowing_is_respected(self):
        for source in ('char* p; int f(int p){return p+364;}',
                       'char* p; void f(){int p=0;use(p+364);}',
                       'void f(){ {char* p;use(p);} use(p+364);}',
                       'void f(char* p){{int p=0;use(p+364);}}'):
            with self.subTest(source=source):
                self.assertFalse(self.hits(source,'FM009'))
        self.assertEqual(len(self.hits('char* p; void f(){{int p=0;use(p+364);} use(p+364);}','FM009')),1)

    def test_gamemain_texture_pool_regression_and_editor_locations(self):
        source='void f(){\n char* strings=pool;\n' + ''.join(
            ' out=MBOX_FindTexture(strings + '+str(offset)+', 0);\n'
            for offset in range(364,473,12)) + '}\n'
        rows=self.hits(source,'FM009')
        self.assertEqual([r['line'] for r in rows],list(range(3,13)))
        for row in rows:
            self.assertIn(': error FM009:',lint.diagnostic(row,Path('.'),'problems'))
            self.assertIn('does not prove a struct or loop',row['message'])

    def test_declaration_scope_in_for_loop_does_not_leak(self):
        self.assertFalse(self.hits('void f(){for(char* p=0;p;){use(p);} use(p+364);}','FM009'))
        self.assertEqual(len(self.hits('char* p; void f(){for(int p=0;p<2;p++){use(p+364);} use(p+364);}','FM009')),1)

    def test_nested_dereference_threshold(self):
        rows=self.hits('int f() { return *(int*)*(void**)(p + 0x8); }','FM002')
        self.assertEqual(len(rows),1)
        self.assertEqual(rows[0]['depth'],2)
        self.assertFalse(self.hits('int f() { return *(int*)(p + 8); }','FM002'))

    def test_comments_strings_chars_and_raw_strings_are_not_code(self):
        s='''// *(int*)(p+4) asm volatile trash[8] 0xCAFE
        /* ((u32*)p)[2] */
        const char* s="*(int*)(p+4) asm 0xCAFE";
        const char* t=R"tag(asm *(int*)(p+4)
        #pragma optimize off
        )tag";
        int f() { return 'x'; }
        '''
        self.assertEqual(self.rules(s),[])

    def test_multiline_cast_location(self):
        rows=self.hits('int f() {\n return *(int*)\n ((u8*)p + 12);\n}', 'FM001')
        self.assertEqual(rows[0]['line'],2)
        self.assertEqual(rows[0]['scope'],'f')

    def test_function_local_declarations(self):
        rows=self.hits('u8 global[16]; volatile int device; void f() { u8 scratch[8]; volatile int home; int trash7; u8 used[3]; use(used); }','FM003')
        self.assertEqual({r['variable'] for r in rows},{'scratch','home','trash7'})

    def test_multiple_local_arrays(self):
        rows=self.hits('void f() { u8 first[2], second[2]; use(first); }','FM003')
        self.assertEqual([r['variable'] for r in rows],['second'])

    def test_index_writes_and_calls_are_not_declarations(self):
        self.assertFalse(self.hits('void f() { data[4] = 1; use(trash); }','FM003'))

    def test_initializer_reference_is_not_a_trash_declarator(self):
        self.assertFalse(self.hits('void f(){ int real = trash; int other = use(unused); }','FM003'))

    def test_volatile_pointer_local(self):
        rows=self.hits('void f(){ int * volatile home = p; }','FM003')
        self.assertEqual([r['variable'] for r in rows],['home'])

    def test_fable_dead_but_incremented_counter(self):
        rows=self.hits('void f(){ int timeOffset; int i; timeOffset=0; i=0; while(i<4){ use(i); i++; timeOffset+=4; } }','FM003')
        self.assertEqual([r['variable'] for r in rows if r.get('pattern')=='write-only-local'],['timeOffset'])

    def test_observed_updates_and_volatile_are_not_dead_counter_claims(self):
        for source in ('int f(){int i=0; i++; return i;}',
                       'void f(){int i=0; use(i++);}',
                       'void f(){volatile int i=0; i++;}',
                       'void f(){static int i=0; i++;}',
                       'void f(){int i=0; i++; {int i=2; use(i);} }'):
            with self.subTest(source=source):
                self.assertFalse([r for r in self.hits(source,'FM003') if r.get('pattern')=='write-only-local'])

    def test_float_and_pointer_shaped_byte_arrays(self):
        for items in ('0x3f,0x80,0,0,0x40,0,0,0',
                      '0x80,0,0,4,0x80,0,0,8,0x80,0,0,12'):
            with self.subTest(items=items):
                rows=self.hits('static const u8 data[] = {'+items+'};','FM004')
                self.assertEqual(len(rows),1)
                self.assertEqual(rows[0]['confidence'],'heuristic')

    def test_zero_and_ordinary_bytes_not_float_blob(self):
        for items in ('0,0,0,0,0,0,0,0','0x89,0x50,0x4e,0x47,0xd,0xa,0x1a,0xa'):
            self.assertFalse(self.hits('static const u8 data[] = {'+items+'};','FM004'))

    def test_asm_and_macro_alias(self):
        self.assertTrue(self.hits('asm void f() { blr }','FM005'))
        self.assertTrue(self.hits('void f() { __asm__ volatile ("nop"); }','FM005'))
        self.assertTrue(self.hits('ASM void f() { blr }','FM005'))
        self.assertEqual(len(self.hits('#define ASM asm\n','FM005')),1)

    def test_pragma_scopes_and_attributes(self):
        rows=self.hits('#pragma once\n#pragma scheduling off\nvoid f() {\n#pragma optimization_level 0\n}\n','FM006')
        self.assertEqual(len(rows),2)
        self.assertEqual(rows[0]['pragma_scope'],'before:f')
        self.assertEqual(rows[1]['pragma_scope'],'f')
        self.assertTrue(self.hits('void __attribute__((optimize("O0"))) f() {}','FM006'))

    def test_hex_constants_masks_and_named_definitions(self):
        s='''#define CAPACITY 0x800
        enum { SIZE = 0x20 };
        const int LIMIT = 0x100;
        void f() { flags &= ~0x40; flags |= (0x20); out = flags & 0xff;
          call(0x123); offset += 0x24; }
        '''
        self.assertEqual([r['excerpt'] for r in self.hits(s,'FM007')],['0x123','0x24'])

    def test_enemy_gravity_flag_is_a_legitimate_mask(self):
        source='void f(){ if (mp != NULL && !(mp->flags & 0x1000)) { goto gravity; } gravity: return; }'
        self.assertFalse(self.hits(source,'FM007'))

    def test_mask_exemption_does_not_hide_arithmetic_or_call_arguments(self):
        for expression in ('p + 0x1000','call(0x1000)','flags & (0x1000 + offset)',
                           '(0x1000 + offset) & flags','flags & call(0x1000)'):
            with self.subTest(expression=expression):
                self.assertTrue(self.hits('int f(){return '+expression+';}','FM007'))

    def test_mask_sides_updates_complements_and_parentheses(self):
        for expression in ('flags & 0x1000','0x1000 & flags','flags | (0x1000u)',
                           'flags ^= 0x1000','flags &= ~0x1000','flags & ((0x1000))'):
            with self.subTest(expression=expression):
                self.assertFalse(self.hits('void f(){'+expression+';}','FM007'))

    def test_macros_are_scanned_and_continued(self):
        s='#define FIELD(p) \\\n  (*(int*)((p) + 0x18))\n'
        self.assertTrue(self.hits(s,'FM001'))
        self.assertEqual([r['excerpt'] for r in self.hits(s,'FM007')],['0x18'])

    def test_stable_fingerprint_across_line_insertions(self):
        source='int f() { return *(int*)(p+8); }'
        a=self.hits(source,'FM001')[0];b=self.hits('\n\n'+source,'FM001')[0]
        self.assertEqual(a['fingerprint'],b['fingerprint'])
        self.assertNotEqual(a['line'],b['line'])

    def test_const_cast_is_not_named_constant(self):
        self.assertEqual(len(self.hits('void f(){ x = *(const int*)(p+0x20); }','FM007')),1)
        self.assertFalse(self.hits('static const u8 data[]={0x3f,0x80,0,0};','FM007'))

    def test_real_cpp_destructor_scope(self):
        rows=self.hits('Thing::~Thing(){ use(0x40); }','FM007')
        self.assertEqual(rows[0]['scope'],'Thing::~Thing')

    def test_unicode_and_crlf_location(self):
        rows=self.hits('// café λ\r\nint f(){return *(int*)(p+8);}', 'FM001')
        self.assertEqual((rows[0]['line'],rows[0]['column']),(2,16))

    def test_parse_recovery_is_reported(self):
        diagnostics=[]
        lint.scan_source('void f(){ @@@ return *(int*)(p+8); }','src/game/a.c',diagnostics)
        self.assertTrue(diagnostics)
        self.assertEqual(diagnostics[0]['path'],'src/game/a.c')

    def test_whole_file_mwcc_asm_error_root_is_not_silently_skipped(self):
        diagnostics=[]
        rows=lint.scan_source('#include "types.h"\nasm void f(){ nofralloc\n psq_l f0,0(src),0,qr0\n blr\n }','src/game/a.c',diagnostics)
        self.assertTrue(diagnostics)
        self.assertTrue(any(r['rule']=='FM005' for r in rows))

    def test_empty_file_has_verified_coverage(self):
        self.assertEqual(self.rules(''),[])

    def test_object_macro_constant_exempt_but_expression_is_scanned(self):
        self.assertFalse(self.hits('#define SIZE 0x20\n','FM007'))
        self.assertTrue(self.hits('#define FIELD(p) (*(int*)((p)+8))\n','FM001'))

    def test_no_macro_assembly_in_string_or_comment(self):
        self.assertFalse(self.hits('#define NAME "asm" /* asm */\n','FM005'))


class PolicyAndCli(unittest.TestCase):
    def policy(self):
        return dict(schema_version=1,exceptions=[],pragma_allowlist=[])

    def test_reviewed_findings_remain_visible(self):
        rows=lint.scan_source('int f(){return *(int*)(p+8);}','src/game/a.c')
        policy=self.policy();policy['exceptions']=[dict(fingerprint=rows[0]['fingerprint'],reason='Verified partial view; target offset 8.')]
        result=lint.apply_policy(rows,policy)
        self.assertTrue(result[0]['suppressed'])
        self.assertIn('review_reason',result[0])
        changed=lint.apply_policy(lint.scan_source('int f(){return *(int*)(p+12);}','src/game/a.c'),policy)
        self.assertFalse(changed[0]['suppressed'])

    def test_legacy_pragma_approval_cannot_hide_warnings(self):
        rows=lint.scan_source('void f(){\n#pragma scheduling off\n#pragma scheduling off\n}\nvoid g(){\n#pragma scheduling off\n}','src/game/a.c')
        policy=self.policy();policy['pragma_allowlist']=[dict(path='src/game/a.c',scope='f',directive='#pragma scheduling off',count=1,reason='Explicitly reviewed.')]
        result=lint.apply_policy(rows,policy)
        self.assertEqual(len(result),3)
        self.assertTrue(all(r['severity']=='warning' and not r['suppressed'] for r in result))

    def test_all_pragmas_are_visible_warnings_and_optional_build_failure(self):
        with tempfile.TemporaryDirectory() as td:
            root=Path(td);(root/'src/game').mkdir(parents=True)
            (root/'src/game/a.c').write_text('#pragma dont_inline on\nvoid f(){}\n#pragma dont_inline off\n#pragma scheduling off\n# pragma opt_propagation off\n#pragma unknown_setting on\n')
            (root/'policy.toml').write_text('schema_version=1\nexceptions=[]\npragma_allowlist=[]\n')
            args=['--root',td,'--policy','policy.toml','src/game/a.c','--format','problems','--fail-on-findings','--out','build/report.json']
            output=io.StringIO()
            with contextlib.redirect_stdout(output):
                self.assertEqual(lint.main(args),0)
                self.assertEqual(lint.main(args+['--warnings-as-errors']),1)
            report=json.loads((root/'build/report.json').read_text())
            self.assertEqual((report['errors'],report['warnings'],report['suppressed']),(0,5,0))
            self.assertIn(': warning FM006:',output.getvalue())
            self.assertTrue(lint.diagnostic(report['findings'][0],root,'github').startswith('::warning '))
            (root/'src/game/a.c').write_text('void __attribute__((optimize("O0"))) f(){}')
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(lint.main(args),1)
            report=json.loads((root/'build/report.json').read_text())
            self.assertEqual((report['errors'],report['warnings']),(1,0))
            self.assertEqual(report['findings'][0]['rule'],'FM006')

    def test_legacy_warning_selector_and_exceptions_do_not_hide_pragmas(self):
        source='#pragma dont_inline on\n#pragma scheduling off\nvoid f(){}'
        rows=lint.scan_source(source,'src/game/a.c');policy=self.policy()
        policy['warning_pragmas']=['#pragma dont_inline on']
        policy['exceptions']=[dict(fingerprint=rows[0]['fingerprint'],reason='Old exemption')]
        result=lint.apply_policy(rows,policy)
        pragmas={r['directive']:r for r in result if r['rule']=='FM006'}
        self.assertEqual(pragmas['#pragma dont_inline on']['severity'],'warning')
        self.assertFalse(pragmas['#pragma dont_inline on']['suppressed'])
        self.assertEqual(pragmas['#pragma scheduling off']['severity'],'warning')
        self.assertFalse(pragmas['#pragma scheduling off']['suppressed'])

    def test_no_legacy_configs_requires_explicit_native_policy(self):
        with tempfile.TemporaryDirectory() as td:
            root=Path(td);(root/'src/game').mkdir(parents=True);policy=root/'tools/gdl/native_build.py'
            policy.parent.mkdir(parents=True);policy.write_text('native policy fixture')
            rows,hashes=lint.postprocessor_findings(root,set(),include_all=True)
            self.assertEqual(rows,[])
            self.assertIn('tools/gdl/native_build.py',hashes)

    def test_macro_assembly_approval_does_not_allow_changed_body_or_direct_asm(self):
        source='#define WAIT() asm("nop")\n'
        rows=lint.scan_source(source,'a.h');p=self.policy()
        p['exceptions']=[dict(fingerprint=rows[0]['fingerprint'],reason='Verified macro only.')]
        self.assertTrue(lint.apply_policy(rows,p)[0]['suppressed'])
        changed=lint.scan_source(source.replace('nop','blr'),'a.h')
        self.assertFalse(lint.apply_policy(changed,p)[0]['suppressed'])
        direct=lint.scan_source('asm("nop");','src/game/a.c')
        p['exceptions']=[dict(fingerprint=direct[0]['fingerprint'],reason='Not a permitted macro.')]
        self.assertFalse(lint.apply_policy(direct,p)[0]['suppressed'])

    def test_cli_reports_all_rows_even_with_zero_console_limit(self):
        with tempfile.TemporaryDirectory() as td:
            root=Path(td);(root/'src/game').mkdir(parents=True);(root/'src/game/a.c').write_text('int f(){return *(int*)(p+0x8);}')
            (root/'policy.json').write_text(json.dumps(self.policy()))
            args=['--root',td,'--policy','policy.json','src/game/a.c','--out','build/report.json','--limit','0']
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(lint.main(args),0)
                self.assertEqual(lint.main(args+['--fail-on-findings']),1)
            report=json.loads((root/'build/report.json').read_text())
            self.assertEqual(report['status'],'SCAN_COMPLETE')
            self.assertGreater(report['unsuppressed'],0)
            self.assertEqual(len(report['findings']),report['unsuppressed'])

    def test_bad_inputs_and_protected_output_refuse(self):
        with tempfile.TemporaryDirectory() as td:
            root=Path(td);(root/'src/game').mkdir(parents=True);(root/'src/game/a.c').write_text('void f() {}')
            (root/'policy.json').write_text(json.dumps(self.policy()))
            common=['--root',td,'--policy','policy.json']
            for tail in (['missing.c'],['src/game/a.c','--out','policy.json'],['src/game/a.c','--out','../escape.json']):
                with self.subTest(tail=tail),contextlib.redirect_stderr(io.StringIO()):
                    self.assertEqual(lint.main(common+tail),2)
            self.assertEqual(json.loads((root/'policy.json').read_text()),self.policy())

    def test_policy_does_not_accept_blanket_or_unexplained_approvals(self):
        with tempfile.TemporaryDirectory() as td:
            path=Path(td)/'policy.json'
            for row in (dict(path='src/*',scope='f',directive='#pragma optimize off',count=1,reason='review'),
                        dict(path='src/game/a.c',scope='f',directive='#pragma optimize off',count=1,reason='')):
                policy=self.policy();policy['pragma_allowlist']=[row];path.write_text(json.dumps(policy))
                with self.assertRaises(ValueError):lint.load_policy(path)

    def test_missing_coverage_and_bad_json_refuse(self):
        import subprocess
        with patch.object(lint,'ast_binary',return_value='not-executed'):
            for stdout in ('[]','{}','not json'):
                result=subprocess.CompletedProcess([],0,stdout,'')
                with self.subTest(stdout=stdout),patch.object(lint.subprocess,'run',return_value=result):
                    with self.assertRaises(ValueError):lint.scan_source('void f(){}','src/game/a.c')

    def test_failed_scanner_is_not_a_clean_result(self):
        import subprocess
        with patch.object(lint,'ast_binary',return_value='not-executed'):
            result=subprocess.CompletedProcess([],6,'[]','broken rule')
            with patch.object(lint.subprocess,'run',return_value=result):
                with self.assertRaisesRegex(ValueError,'scan failed'):lint.scan_source('void f(){}','src/game/a.c')

    def test_cli_dependency_failure_is_exit_two(self):
        with tempfile.TemporaryDirectory() as td:
            root=Path(td);(root/'src/game').mkdir(parents=True);(root/'src/game/a.c').write_text('void f(){}')
            (root/'policy.json').write_text(json.dumps(self.policy()))
            with patch.object(lint,'ast_binary',side_effect=FileNotFoundError('node unavailable')),contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(lint.main(['--root',td,'--policy','policy.json','src/game/a.c']),2)

    def test_editor_diagnostics_cover_every_row_even_at_limit_zero(self):
        with tempfile.TemporaryDirectory() as td:
            root=Path(td);(root/'src/game').mkdir(parents=True);(root/'src/game/a.c').write_text('void f(){use(0x40); use(0x80);}')
            (root/'policy.json').write_text(json.dumps(self.policy()))
            output=io.StringIO()
            with contextlib.redirect_stdout(output):
                result=lint.main(['--root',td,'--policy','policy.json','src/game/a.c','--format','problems','--limit','0','--fail-on-findings'])
            self.assertEqual(result,1)
            self.assertEqual(output.getvalue().count(': error FM007:'),2)

    def test_github_annotation_escapes_untrusted_text(self):
        row=dict(path='src/a,b.c',line=3,column=2,rule='FM001',scope='f',message='bad%\n::warning::injection')
        line=lint.diagnostic(row,Path('.'),'github')
        self.assertIn('file=src/a%2Cb.c',line)
        self.assertIn('bad%25%0A::warning::injection',line)
        self.assertNotIn('\n',line)

    def test_postprocessor_inventory_is_scoped_and_not_suppressed(self):
        with tempfile.TemporaryDirectory() as td:
            root=Path(td);(root/'src/game').mkdir(parents=True);folder=root/'config/GUNE5D';folder.mkdir(parents=True)
            for engine,path in lint.POSTPROCESSORS:
                rule=dict(function='f',before_sha256='x')
                units={'game/test':[rule]} if engine=='WebFrank' else {}
                (root/path).write_text(json.dumps(dict(version=1,units=units),indent=2))
            rows,hashes=lint.postprocessor_findings(root,{'src/game/test.c'})
            self.assertEqual(len(rows),1);self.assertEqual(rows[0]['rule'],'FM008')
            self.assertEqual(rows[0]['postprocessor'],'WebFrank')
            source=(folder/'webfrank.json').read_text().splitlines()
            self.assertIn('"function": "f"',source[rows[0]['line']-1])
            self.assertEqual(len(hashes),2)
            self.assertEqual(lint.postprocessor_findings(root,{'src/other.c'})[0],[])
            policy=self.policy()
            policy['exceptions']=[dict(fingerprint=rows[0]['fingerprint'],reason='Cannot waive a dependency.')]
            self.assertFalse(lint.apply_policy(rows,policy)[0]['suppressed'])
            self.assertEqual(len(lint.postprocessor_findings(root,{'src/other.c'},include_all=True)[0]),1)

    def test_missing_postprocessor_inventory_refuses(self):
        with tempfile.TemporaryDirectory() as td:
            with self.assertRaises(OSError):lint.postprocessor_findings(Path(td),{'src/a.c'})

    def test_editor_problem_pattern_captures_windows_path(self):
        import re
        tasks=(lint.ROOT/'.vscode/tasks.json').read_text()
        # Read only the actual regexp JSON string, since tasks.json permits comments.
        encoded=re.search(r'"regexp":\s*("(?:[^"\\]|\\.)*")',tasks).group(1)
        pattern=json.loads(encoded)
        match=re.match(pattern,'W:/My Project/a.c:12:3: error FM008: native retirement required')
        self.assertEqual(match.groups(),('W:/My Project/a.c','12','3','error','FM008','native retirement required'))

    def test_both_editor_tasks_recognize_new_errors_and_pragma_warnings(self):
        import re
        tasks=(lint.ROOT/'.vscode/tasks.json').read_text()
        patterns=re.findall(r'"regexp":\s*("(?:[^"\\]|\\.)*")',tasks)
        self.assertEqual(len(patterns),2)
        for encoded in patterns:
            for severity,rule in (('error','FM009'),('warning','FM006')):
                row=dict(path='src/a.c',line=3,column=5,rule=rule,severity=severity,scope='f',message='review')
                output=lint.diagnostic(row,Path('W:/My Project'),'problems')
                match=re.match(json.loads(encoded),output)
                self.assertIsNotNone(match)
                self.assertEqual(match.group(4),severity)
                self.assertEqual(match.group(5),rule)

    def test_watch_cache_reuses_only_unchanged_source(self):
        with tempfile.TemporaryDirectory() as td:
            root=Path(td);(root/'src/game').mkdir(parents=True);source=root/'src/game/a.c';source.write_text('void f(){use(0x40);}')
            (root/'policy.json').write_text(json.dumps(self.policy()))
            args=['--root',td,'--policy','policy.json','src/game/a.c','--limit','0']
            cache={}
            with patch.object(lint,'scan_source',wraps=lint.scan_source) as scanner,contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(lint.main(args,_cache=cache),0)
                self.assertEqual(lint.main(args,_cache=cache),0)
                self.assertEqual(scanner.call_count,1)
                source.write_text('void f(){use(0x80);}')
                self.assertEqual(lint.main(args,_cache=cache),0)
                self.assertEqual(scanner.call_count,2)


class InlineSuppressions(unittest.TestCase):
    def reviewed(self,source):
        rows=lint.scan_source(source,'src/game/test.c')
        return lint.apply_policy(rows,dict(schema_version=1,exceptions=[],pragma_allowlist=[]))

    def test_reasoned_game_statement_is_audited_not_deleted(self):
        source='void f(){\n// lint-allow-next-line FM007: Packed color; checked the callee.\nFatalError(message, 0x8000); use(0x40);\n}'
        rows=self.reviewed(source)
        self.assertEqual(len(rows),2)
        self.assertEqual([r['suppressed'] for r in rows],[True,False])
        self.assertEqual(rows[0]['review_reason'],'Packed color; checked the callee.')
        self.assertEqual(rows[0]['suppression_source'],'source-comment')
        self.assertEqual(rows[0]['inline_suppression']['comment_line'],2)
        self.assertEqual(rows[0]['inline_suppression']['target_end_line'],3)
        self.assertRegex(rows[0]['inline_suppression']['target_sha256'],r'^[0-9a-f]{64}$')

    def test_multiline_declaration_is_one_unit_not_neighboring_code(self):
        source='void f(){\n// lint-allow-next-line FM003: Device-observed state; verified volatility.\nvolatile int state =\n  0;\nvolatile int other=0;\n}'
        rows=self.reviewed(source)
        self.assertEqual([r['suppressed'] for r in rows],[True,False])
        self.assertEqual(rows[0]['inline_suppression']['target_end_line'],4)

    def test_specific_rules_only_and_multiple_rules(self):
        source='int f(){\n// lint-allow-next-line FM001: Verified packed buffer alignment.\nreturn *(int*)(p+0x8);\n}'
        rows=self.reviewed(source)
        self.assertTrue(next(r for r in rows if r['rule']=='FM001')['suppressed'])
        self.assertFalse(next(r for r in rows if r['rule']=='FM007')['suppressed'])
        rows=self.reviewed(source.replace('FM001:', 'FM001, FM007:'))
        self.assertTrue(all(r['suppressed'] for r in rows))

    def test_direct_gnu_and_mwcc_assembly_are_explicitly_waivable(self):
        for body in ('void f(){\n// lint-allow-next-line FM005: Required device ordering barrier.\nasm("sync");\nasm("isync");\n}',
                     '// lint-allow-next-line FM005: Required platform register access.\nasm void f(void) {\n  sync\n  blr\n}\nasm void g(void) { blr }',
                     'void f(){\n// lint-allow-next-line FM005: Required platform register access.\nasm { sync }\nasm { isync }\n}'):
            with self.subTest(body=body):
                rows=[r for r in self.reviewed(body) if r['rule']=='FM005']
                self.assertEqual([r['suppressed'] for r in rows],[True,False])

    def test_macro_and_pragma_comments_remain_narrow(self):
        source='// lint-allow-next-line FM005: Verified synchronization macro.\n#define BARRIER() asm("sync")\n#define OTHER() asm("isync")\n'
        self.assertEqual([r['suppressed'] for r in self.reviewed(source)],[True,False])
        source='// lint-allow-next-line FM006: Verified platform packing directive.\n#pragma pack(4)\n#pragma pack()\n'
        rows=self.reviewed(source)
        self.assertEqual([r['suppressed'] for r in rows],[True,False])
        self.assertEqual([r['severity'] for r in rows],['warning','warning'])

    def test_single_line_block_comment_is_supported(self):
        source='void f(){\n/* lint-allow-next-line FM007: Verified packed API value. */\nuse(0x40);\n}'
        self.assertTrue(self.reviewed(source)[0]['suppressed'])

    def test_comment_text_in_strings_does_not_waive_findings(self):
        source='void f(){use("// lint-allow-next-line FM007: pretend");use(0x40);}'
        rows=self.reviewed(source)
        self.assertTrue(rows)
        self.assertFalse(any(r['suppressed'] for r in rows))

    def test_malformed_stale_and_overbroad_comments_refuse(self):
        invalid=[
            '// lint-allow-next-line FM007:\nuse(0x40);',
            '// lint-allow-next-line FM007:   \nuse(0x40);',
            '// lint-allow-next-line FM999: unknown\nuse(0x40);',
            '// lint-allow-next-line FM000: waive scanner\nuse(0x40);',
            '// lint-allow-next-line FM008: waive patcher\nuse(0x40);',
            '// lint-allow-next-line FM007,FM007: repeated\nuse(0x40);',
            '// lint-disable FM007: blanket\nuse(0x40);',
            '// lint-allow-next-line *: blanket\nuse(0x40);',
            '// gdl-lint-allow-next-line FM007: old prefix\nuse(0x40);',
            '// lint-allow-next-line FM007: stale\nuse(1);',
            '// lint-allow-next-line FM001,FM007: partly stale\nuse(0x40);',
            '// lint-allow-next-line FM007: gap\n\nuse(0x40);',
            '// lint-allow-next-line FM007: gap\n// another comment\nuse(0x40);',
            'use(1); // lint-allow-next-line FM007: not standalone\nuse(0x40);',
            '// lint-allow-next-line FM007: block-wide\n{use(0x40);use(0x80);}',
            '// lint-allow-next-line FM007: whole function\nvoid nested(){use(0x40);}',
            '// lint-allow-next-line FM007: hidden function\nauto fn=[](){use(0x40);};',
            '// lint-allow-next-line FM005,FM007: asm body\nasm { sync }',
        ]
        for body in invalid:
            with self.subTest(body=body),self.assertRaises(lint.SuppressionError):
                self.reviewed('void f(){\n'+body+'\n}')
        with self.assertRaises(lint.SuppressionError):
            self.reviewed('// lint-allow-next-line FM005: unclosed\nasm { sync')

    def test_region_is_explicit_bounded_reasoned_and_rule_specific(self):
        source='void f(){\nuse(0x10);\n// lint-begin FM007: Verified packed values in this sequence.\nuse(0x20);\nasm("sync");\nuse(0x30);\n// lint-end FM007\nuse(0x40);\n}'
        rows=self.reviewed(source)
        self.assertEqual([r['suppressed'] for r in rows],[False,True,False,True,False])
        self.assertEqual(rows[1]['inline_suppression']['target_kind'],'region')
        self.assertEqual(rows[1]['inline_suppression']['comment_line'],3)
        self.assertEqual(rows[1]['inline_suppression']['end_comment_line'],7)
        source=source.replace('lint-begin FM007:', 'lint-begin FM005, FM007:').replace('lint-end FM007', 'lint-end FM007, FM005')
        self.assertEqual([r['suppressed'] for r in self.reviewed(source)],[False,True,True,True,False])
        self.assertEqual([r['suppressed'] for r in self.reviewed(source.replace('\n','\r\n'))],[False,True,True,True,False])

    def test_bad_region_pairs_and_overlapping_waivers_refuse(self):
        bad=[
            '// lint-begin FM007: reason\nuse(0x40);',
            '// lint-end FM007\nuse(0x40);',
            '// lint-begin FM007: reason\nuse(0x40);\n// lint-end FM005',
            '// lint-begin FM007:\nuse(0x40);\n// lint-end FM007',
            '// lint-begin FM007: reason\nuse(0x40);\n// lint-end FM007: extra',
            '// lint-begin FM007: reason\nuse(1);\n// lint-end FM007',
            '// lint-begin FM007: reason\n// lint-begin FM005: nested\nasm("sync");\n// lint-end FM005\n// lint-end FM007',
            '// lint-begin FM007: reason\n// lint-allow-next-line FM007: overlapping\nuse(0x40);\n// lint-end FM007',
            '// lint-begin FM008: patcher\nuse(0x40);\n// lint-end FM008',
        ]
        for body in bad:
            with self.subTest(body=body),self.assertRaises(lint.SuppressionError):
                self.reviewed('void f(){\n'+body+'\n}')

    def test_stale_comment_failure_points_to_source_in_editor(self):
        with tempfile.TemporaryDirectory() as td:
            root=Path(td);(root/'src/game').mkdir(parents=True)
            (root/'src/game/a.c').write_text('void f(){\n// lint-allow-next-line FM007: stale reason\nuse(1);\n}')
            (root/'policy.json').write_text(json.dumps(dict(schema_version=1,exceptions=[],pragma_allowlist=[])))
            out=io.StringIO()
            with contextlib.redirect_stdout(out),contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(lint.main(['--root',td,'--policy','policy.json','--format','problems']),2)
            self.assertIn('/src/game/a.c:2:1: error FM000:',out.getvalue())
            self.assertIn('unused rule',out.getvalue())

    def test_file_waiver_is_rule_specific_reasoned_and_file_local(self):
        source='/* Copyright notice */\n// lint-file FM005: Verified platform primitives, not copied game logic.\nvoid f(){asm("sync");use(0x40);}\nvoid g(){asm("isync");}'
        rows=self.reviewed(source)
        self.assertEqual([r['suppressed'] for r in rows if r['rule']=='FM005'],[True,True])
        self.assertFalse(next(r for r in rows if r['rule']=='FM007')['suppressed'])
        self.assertEqual(rows[0]['inline_suppression']['target_kind'],'file')
        self.assertEqual(rows[0]['inline_suppression']['comment_line'],2)
        self.assertFalse(self.reviewed('void f(){asm("sync");}')[0]['suppressed'])

    def test_file_waiver_rejects_late_blank_unknown_and_overlapping_entries(self):
        bad=[
            '// lint-file FM005:\nvoid f(){asm("sync");}',
            '#include "a.h"\n// lint-file FM005: too late\nvoid f(){asm("sync");}',
            '// lint-file FM008: forbidden\nvoid f(){asm("sync");}',
            '// lint-file *: all\nvoid f(){asm("sync");}',
            '// lint-file FM005: stale\nvoid f(){}',
            '// lint-file FM005: first\n// lint-file FM005: redundant\nvoid f(){asm("sync");}',
            '// lint-file FM005: first\nvoid f(){\n// lint-begin FM005: redundant\nasm("sync");\n// lint-end FM005\n}',
        ]
        for source in bad:
            with self.subTest(source=source),self.assertRaises(lint.SuppressionError):self.reviewed(source)

    def test_suppressed_findings_survive_json_but_do_not_fail_ci(self):
        with tempfile.TemporaryDirectory() as td:
            root=Path(td);(root/'src/game').mkdir(parents=True)
            (root/'src/game/a.c').write_text('void f(){\n// lint-allow-next-line FM007: Callee interprets a packed color.\nuse(0x40);\n}')
            (root/'policy.json').write_text(json.dumps(dict(schema_version=1,exceptions=[],pragma_allowlist=[])))
            args=['--root',td,'--policy','policy.json','--out','build/report.json','--format','problems',
                  '--fail-on-findings','--warnings-as-errors']
            out=io.StringIO()
            with contextlib.redirect_stdout(out):self.assertEqual(lint.main(args),0)
            report=json.loads((root/'build/report.json').read_text())
            self.assertEqual((report['errors'],report['warnings'],report['suppressed']),(0,0,1))
            self.assertEqual(report['findings'][0]['inline_suppression']['comment_line'],2)
            self.assertNotIn(': error FM007:',out.getvalue())


class GameOnlyScope(unittest.TestCase):
    def test_defaults_explicit_inputs_and_editor_skip_sdk_and_headers(self):
        with tempfile.TemporaryDirectory() as td:
            root=Path(td)
            for relative in ('src/game/a.c','src/dolphin/os.c','include/game/a.h'):
                path=root/relative;path.parent.mkdir(parents=True,exist_ok=True)
                path.write_text('void f(){use(0x40);asm("sync");}')
            (root/'policy.json').write_text(json.dumps(dict(schema_version=1,exceptions=[],pragma_allowlist=[])))
            args=['--root',td,'--policy','policy.json','--out','build/report.json','--format','problems','--fail-on-findings']
            for inputs,expected in (([],1),(['src','include'],1),(['src/dolphin/os.c'],0),(['include/game/a.h'],0)):
                with self.subTest(inputs=inputs),contextlib.redirect_stdout(io.StringIO()):
                    self.assertEqual(lint.main(args+inputs),expected)
                    report=json.loads((root/'build/report.json').read_text())
                    self.assertEqual(report['files_scanned'],expected)
                    self.assertEqual(report['source_scope'],'src/game')
                    self.assertTrue(all(r['path'].startswith('src/game/') for r in report['findings']))

    def test_postprocessor_source_inventory_is_game_only(self):
        with tempfile.TemporaryDirectory() as td:
            root=Path(td);(root/'config/GUNE5D').mkdir(parents=True)
            config=dict(version=1,units={'game/test':[dict(function='f')],'dolphin/os':[dict(function='g')]})
            (root/'config/GUNE5D/webfrank.json').write_text(json.dumps(config,indent=2))
            rows,_=lint.postprocessor_findings(root,set(),include_all=True)
            self.assertEqual([r['unit'] for r in rows],['game/test'])


class RemediationGuidance(unittest.TestCase):
    def test_every_rule_and_scanner_failure_have_complete_guidance(self):
        guidance,digest=lint.load_guidance(lint.GUIDANCE_PATH)
        self.assertEqual(set(guidance['rules']),set(lint.RULES)|{'FM000'})
        self.assertRegex(digest,r'^[0-9a-f]{64}$')
        for rule in guidance['rules']:
            with self.subTest(rule=rule):
                explanation=lint.explain_rule(rule,guidance)
                for heading in ('Investigation:','Conditional example:','Before:','After:',
                                'Legitimate cases:','Avoid:','Verification for source changes:'):
                    self.assertIn(heading,explanation)
                self.assertIn('not proof',explanation)

    def test_explain_needs_no_parser_policy_or_source_and_does_not_scan(self):
        with tempfile.TemporaryDirectory() as td,patch.object(lint,'ast_binary',side_effect=AssertionError('must not start parser')):
            output=io.StringIO()
            with contextlib.redirect_stdout(output):
                self.assertEqual(lint.main(['--root',td,'--explain','FM001','--explain','FM007']),0)
            self.assertIn('effective BYTE offset',output.getvalue())
            self.assertIn('Direct bitwise masks',output.getvalue())
            self.assertNotIn('SCAN_COMPLETE',output.getvalue())
            self.assertEqual(list(Path(td).iterdir()),[])
            with contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(lint.main(['--explain','FM001','--out','build/report.json']),2)

    def test_missing_or_malformed_guidance_is_not_a_successful_scan(self):
        with tempfile.TemporaryDirectory() as td:
            path=Path(td)/'guidance.toml'
            malformed=['schema_version=1\n[rules]\n',
                       lint.GUIDANCE_PATH.read_text().replace('hint =','missing_hint =',1)]
            for content in malformed:
                path.write_text(content)
                with self.assertRaises(ValueError):lint.load_guidance(path)
            with patch.object(lint,'GUIDANCE_PATH',path),contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(lint.main(['--explain','FM001']),2)

    def test_editor_includes_single_line_conditional_example_and_help(self):
        guidance,_=lint.load_guidance(lint.GUIDANCE_PATH)
        tasks=(lint.ROOT/'.vscode/tasks.json').read_text()
        import re
        patterns=re.findall(r'"regexp":\s*("(?:[^"\\]|\\.)*")',tasks)
        for rule in guidance['rules']:
            row=dict(path='src/a.c',line=3,column=5,rule=rule,scope='f',message='review',
                     severity='warning' if rule=='FM006' else 'error')
            original=dict(row)
            output=lint.diagnostic(row,Path('W:/My Project'),'problems',guidance)
            self.assertNotIn('\n',output)
            self.assertIn('Conditional example (Only if' if rule=='FM001' else 'Conditional example (',output)
            self.assertIn('--explain '+rule,output)
            for encoded in patterns:
                match=re.match(json.loads(encoded),output)
                self.assertIsNotNone(match)
                self.assertEqual(match.group(4),row['severity'])
                self.assertEqual(match.group(5),rule)
            self.assertEqual(row,original)

    def test_report_guidance_is_shared_and_cannot_change_cached_findings(self):
        with tempfile.TemporaryDirectory() as td:
            root=Path(td);(root/'src/game').mkdir(parents=True);(root/'src/game/a.c').write_text('#pragma scheduling off\nint f(){return *(int*)(p+0x8);}')
            (root/'policy.json').write_text(json.dumps(dict(schema_version=1,exceptions=[],pragma_allowlist=[])))
            local_guide=root/'guidance.toml';local_guide.write_bytes(lint.GUIDANCE_PATH.read_bytes())
            args=['--root',td,'--policy','policy.json','src/game/a.c','--out','build/report.json','--limit','0']
            cache={}
            with patch.object(lint,'GUIDANCE_PATH',local_guide),patch.object(lint,'scan_source',wraps=lint.scan_source) as scanner,contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(lint.main(args,_cache=cache),0)
                before=json.loads((root/'build/report.json').read_text())
                local_guide.write_text(local_guide.read_text().replace('Resolve the base object', 'First resolve the base object'))
                self.assertEqual(lint.main(args,_cache=cache),0)
                after=json.loads((root/'build/report.json').read_text())
                self.assertEqual(scanner.call_count,1)
            self.assertEqual(before['findings'],after['findings'])
            self.assertEqual((before['errors'],before['warnings']),(after['errors'],after['warnings']))
            self.assertNotEqual(before['guidance_sha256'],after['guidance_sha256'])
            for row in after['findings']:
                self.assertEqual(row['guidance_id'],row['rule'])
                self.assertIn(row['guidance_id'],after['remediation_guidance']['rules'])
            self.assertTrue(all('guidance_id' not in row for row in cache['src/game/a.c'][1]))


if __name__=='__main__':
    unittest.main()
