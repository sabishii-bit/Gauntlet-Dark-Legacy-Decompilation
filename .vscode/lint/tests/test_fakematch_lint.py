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
        lint.scan_source('void f(){ @@@ return *(int*)(p+8); }','a.c',diagnostics)
        self.assertTrue(diagnostics)
        self.assertEqual(diagnostics[0]['path'],'a.c')

    def test_whole_file_mwcc_asm_error_root_is_not_silently_skipped(self):
        diagnostics=[]
        rows=lint.scan_source('#include "types.h"\nasm void f(){ nofralloc\n psq_l f0,0(src),0,qr0\n blr\n }','a.c',diagnostics)
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
        rows=lint.scan_source('int f(){return *(int*)(p+8);}','a.c')
        policy=self.policy();policy['exceptions']=[dict(fingerprint=rows[0]['fingerprint'],reason='Verified partial view; target offset 8.')]
        result=lint.apply_policy(rows,policy)
        self.assertTrue(result[0]['suppressed'])
        self.assertIn('review_reason',result[0])
        changed=lint.apply_policy(lint.scan_source('int f(){return *(int*)(p+12);}','a.c'),policy)
        self.assertFalse(changed[0]['suppressed'])

    def test_legacy_pragma_approval_cannot_hide_warnings(self):
        rows=lint.scan_source('void f(){\n#pragma scheduling off\n#pragma scheduling off\n}\nvoid g(){\n#pragma scheduling off\n}','a.c')
        policy=self.policy();policy['pragma_allowlist']=[dict(path='a.c',scope='f',directive='#pragma scheduling off',count=1,reason='Explicitly reviewed.')]
        result=lint.apply_policy(rows,policy)
        self.assertEqual(len(result),3)
        self.assertTrue(all(r['severity']=='warning' and not r['suppressed'] for r in result))

    def test_all_pragmas_are_visible_warnings_and_optional_build_failure(self):
        with tempfile.TemporaryDirectory() as td:
            root=Path(td)
            (root/'a.c').write_text('#pragma dont_inline on\nvoid f(){}\n#pragma dont_inline off\n#pragma scheduling off\n# pragma opt_propagation off\n#pragma unknown_setting on\n')
            (root/'policy.toml').write_text('schema_version=1\nexceptions=[]\npragma_allowlist=[]\n')
            args=['--root',td,'--policy','policy.toml','a.c','--format','problems','--fail-on-findings','--out','build/report.json']
            output=io.StringIO()
            with contextlib.redirect_stdout(output):
                self.assertEqual(lint.main(args),0)
                self.assertEqual(lint.main(args+['--warnings-as-errors']),1)
            report=json.loads((root/'build/report.json').read_text())
            self.assertEqual((report['errors'],report['warnings'],report['suppressed']),(0,5,0))
            self.assertIn(': warning FM006:',output.getvalue())
            self.assertTrue(lint.diagnostic(report['findings'][0],root,'github').startswith('::warning '))
            (root/'a.c').write_text('void __attribute__((optimize("O0"))) f(){}')
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(lint.main(args),1)
            report=json.loads((root/'build/report.json').read_text())
            self.assertEqual((report['errors'],report['warnings']),(1,0))
            self.assertEqual(report['findings'][0]['rule'],'FM006')

    def test_legacy_warning_selector_and_exceptions_do_not_hide_pragmas(self):
        source='#pragma dont_inline on\n#pragma scheduling off\nvoid f(){}'
        rows=lint.scan_source(source,'a.c');policy=self.policy()
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
            root=Path(td);policy=root/'tools/gdl/native_build.py'
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
        direct=lint.scan_source('asm("nop");','a.c')
        p['exceptions']=[dict(fingerprint=direct[0]['fingerprint'],reason='Not a permitted macro.')]
        self.assertFalse(lint.apply_policy(direct,p)[0]['suppressed'])

    def test_cli_reports_all_rows_even_with_zero_console_limit(self):
        with tempfile.TemporaryDirectory() as td:
            root=Path(td);(root/'a.c').write_text('int f(){return *(int*)(p+0x8);}')
            (root/'policy.json').write_text(json.dumps(self.policy()))
            args=['--root',td,'--policy','policy.json','a.c','--out','build/report.json','--limit','0']
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(lint.main(args),0)
                self.assertEqual(lint.main(args+['--fail-on-findings']),1)
            report=json.loads((root/'build/report.json').read_text())
            self.assertEqual(report['status'],'SCAN_COMPLETE')
            self.assertGreater(report['unsuppressed'],0)
            self.assertEqual(len(report['findings']),report['unsuppressed'])

    def test_bad_inputs_and_protected_output_refuse(self):
        with tempfile.TemporaryDirectory() as td:
            root=Path(td);(root/'a.c').write_text('void f() {}')
            (root/'policy.json').write_text(json.dumps(self.policy()))
            common=['--root',td,'--policy','policy.json']
            for tail in (['missing.c'],['a.c','--out','policy.json'],['a.c','--out','../escape.json']):
                with self.subTest(tail=tail),contextlib.redirect_stderr(io.StringIO()):
                    self.assertEqual(lint.main(common+tail),2)
            self.assertEqual(json.loads((root/'policy.json').read_text()),self.policy())

    def test_policy_does_not_accept_blanket_or_unexplained_approvals(self):
        with tempfile.TemporaryDirectory() as td:
            path=Path(td)/'policy.json'
            for row in (dict(path='src/*',scope='f',directive='#pragma optimize off',count=1,reason='review'),
                        dict(path='a.c',scope='f',directive='#pragma optimize off',count=1,reason='')):
                policy=self.policy();policy['pragma_allowlist']=[row];path.write_text(json.dumps(policy))
                with self.assertRaises(ValueError):lint.load_policy(path)

    def test_missing_coverage_and_bad_json_refuse(self):
        import subprocess
        with patch.object(lint,'ast_binary',return_value='not-executed'):
            for stdout in ('[]','{}','not json'):
                result=subprocess.CompletedProcess([],0,stdout,'')
                with self.subTest(stdout=stdout),patch.object(lint.subprocess,'run',return_value=result):
                    with self.assertRaises(ValueError):lint.scan_source('void f(){}','a.c')

    def test_failed_scanner_is_not_a_clean_result(self):
        import subprocess
        with patch.object(lint,'ast_binary',return_value='not-executed'):
            result=subprocess.CompletedProcess([],6,'[]','broken rule')
            with patch.object(lint.subprocess,'run',return_value=result):
                with self.assertRaisesRegex(ValueError,'scan failed'):lint.scan_source('void f(){}','a.c')

    def test_cli_dependency_failure_is_exit_two(self):
        with tempfile.TemporaryDirectory() as td:
            root=Path(td);(root/'a.c').write_text('void f(){}')
            (root/'policy.json').write_text(json.dumps(self.policy()))
            with patch.object(lint,'ast_binary',side_effect=FileNotFoundError('node unavailable')),contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(lint.main(['--root',td,'--policy','policy.json','a.c']),2)

    def test_editor_diagnostics_cover_every_row_even_at_limit_zero(self):
        with tempfile.TemporaryDirectory() as td:
            root=Path(td);(root/'a.c').write_text('void f(){use(0x40); use(0x80);}')
            (root/'policy.json').write_text(json.dumps(self.policy()))
            output=io.StringIO()
            with contextlib.redirect_stdout(output):
                result=lint.main(['--root',td,'--policy','policy.json','a.c','--format','problems','--limit','0','--fail-on-findings'])
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
            root=Path(td);folder=root/'config/GUNE5D';folder.mkdir(parents=True)
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
            root=Path(td);source=root/'a.c';source.write_text('void f(){use(0x40);}')
            (root/'policy.json').write_text(json.dumps(self.policy()))
            args=['--root',td,'--policy','policy.json','a.c','--limit','0']
            cache={}
            with patch.object(lint,'scan_source',wraps=lint.scan_source) as scanner,contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(lint.main(args,_cache=cache),0)
                self.assertEqual(lint.main(args,_cache=cache),0)
                self.assertEqual(scanner.call_count,1)
                source.write_text('void f(){use(0x80);}')
                self.assertEqual(lint.main(args,_cache=cache),0)
                self.assertEqual(scanner.call_count,2)


if __name__=='__main__':
    unittest.main()
