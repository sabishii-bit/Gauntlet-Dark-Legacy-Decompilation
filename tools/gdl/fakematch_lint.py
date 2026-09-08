"""Review-only ast-grep C++ reconstruction-debt scanner; never rewrites source.

All seven rule families run through the pinned ast-grep rules. Python supplies
lexical-use/byte-shape/mask filters, exact review policy, and MWCC asm fallback.
Macros are scanned in a separate offset-preserving projection, not expanded.
Parse recovery is reported, not treated as proof of clean source. No type/CFG
analysis or original-source authenticity proof is implied. Exit 0: scan complete;
1: findings with --fail-on-findings; 2: missing input/dependency or scanner error.
scan_source creates temporary snapshots for the native parser; it is not a pure
analysis API. Importing this module does not start the parser or write files.
IMPORTABLE CORE: apply_policy -- apply review policy to already collected findings.
"""
import argparse
from bisect import bisect_right
from collections import Counter
from functools import lru_cache
import hashlib
import json
import math
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
RULES = {
    'FM001': 'Raw offset or indexed pointer cast',
    'FM002': 'Nested dereference through pointer casts',
    'FM003': 'Possible stack/allocation scaffolding',
    'FM004': 'Float/address-shaped numeric byte array',
    'FM005': 'Assembly outside a reviewed macro',
    'FM006': 'Source-level compilation override',
    'FM007': 'Unnamed hexadecimal expression constant',
}
EXTENSIONS = {'.c', '.cpp', '.cc', '.cxx', '.h', '.hpp', '.hh', '.hxx'}
TOKEN = re.compile(
    r'(?P<comment>//(?:\\\r?\n|[^\n])*|/\*.*?\*/)'
    r'|(?P<string>(?:u8|u|U|L)?R"(?P<delimiter>[^ ()\\\t\r\n]{0,16})\(.*?\)(?P=delimiter)"'
    r'|(?:u8|u|U|L)?"(?:\\.|[^"\\])*"|(?:u8|u|U|L)?\'(?:\\.|[^\'\\])*\')'
    r'|(?P<id>[A-Za-z_$][\w$]*)'
    r'|(?P<number>0[xX][0-9A-Fa-f]+[uUlL]*|\d+(?:\.\d*)?(?:[eE][+-]?\d+)?[uUlLfF]*)'
    r'|(?P<space>\s+)|(?P<op>::|->|&=|\|=|\^=|&&|\|\||.)', re.S)
DIRECTIVE = re.compile(r'(?m)^[ \t]*#(?:\\\r?\n|[^\n\\]|\\(?!\r?\n))*')
TRASH = re.compile(r'^(?:trash\w*|unused\w*|(?:stack|frame)_?pad\w*|pad(?:ding)?(?:[0-9_]\w*)?)$', re.I)


@lru_cache(maxsize=1)
def ast_binary():
    resolved = subprocess.run(['node', str(ROOT/'tools/gdl/lint/resolve_ast_grep.cjs')],
                              capture_output=True, text=True, timeout=30, cwd=ROOT)
    if resolved.returncode or not Path(resolved.stdout.strip()).is_file():
        raise ValueError('ast-grep unavailable; run pnpm install --frozen-lockfile. '+resolved.stderr.strip())
    binary = resolved.stdout.strip()
    version = subprocess.run([binary, '--version'], capture_output=True, text=True, timeout=30)
    if version.returncode or version.stdout.strip() != 'ast-grep 0.45.3':
        raise ValueError('expected pinned ast-grep 0.45.3; reinstall frozen dependencies')
    return binary


def ast_rows(text, projection):
    """Snapshot inputs; native scan has no rewrite flags and cannot touch source."""
    with tempfile.TemporaryDirectory(prefix='gdl-lint-') as td:
        paths = [Path(td)/'source.cpp']
        paths[0].write_text(text, encoding='utf-8', newline='')
        if projection.strip():
            paths.append(Path(td)/'macros.cpp')
            paths[-1].write_text(projection, encoding='utf-8', newline='')
        result = subprocess.run([ast_binary(), 'scan', '--config', str(ROOT/'sgconfig.yml'),
                                 '--json=compact', *map(str, paths)],
                                capture_output=True, encoding='utf-8', timeout=120, cwd=ROOT)
        if result.returncode:
            raise ValueError('ast-grep scan failed: '+result.stderr.strip())
        rows = json.loads(result.stdout)
        if not isinstance(rows, list):
            raise ValueError('ast-grep did not return a finding list')
        # Entire MWCC asm TUs can have an ERROR root instead of translation_unit.
        # That proves the file was visited, not that it parsed successfully; all
        # such recovery rows remain separately visible in the final report.
        covered = {Path(r['file']).name for r in rows
                   if r['ruleId'] in ('decomp-coverage','decomp-parse-error')}
        if covered != {p.name for p in paths}:
            raise ValueError('ast-grep coverage incomplete: '+str({p.name for p in paths}-covered))
        return rows


def scan_source(text, path='<input>', diagnostics=None):
    if not text.strip():
        # ast-grep emits no zero-width translation_unit match for empty files.
        # The snapshot is demonstrably whitespace-only; still require the engine.
        ast_binary()
        return []
    tokens = [m for m in TOKEN.finditer(text) if m.lastgroup not in ('comment','space')]
    clean = list(text)
    for m in TOKEN.finditer(text):
        if m.lastgroup in ('comment','string'):
            clean[m.start():m.end()] = ['\n' if c=='\n' else ' ' for c in m.group()]
    clean = ''.join(clean)
    directives, macro_ranges = [], []
    projection = ['\n' if c=='\n' else ' ' for c in text]
    for m in DIRECTIVE.finditer(clean):
        original = text[m.start():m.end()]
        directive = re.sub(r'\s+', ' ', re.sub(r'\\\r?\n',' ',original)).strip()
        macro = re.match(r'#\s*define\s+(\w+)(\([^\n]*?\))?', original)
        directives.append((m.start(),m.end(),directive,macro.group(1) if macro else None))
        if macro:
            start = m.start()+macro.end()
            # Object-like definitions are named constants, not FM007 debt.
            macro_ranges.append((start,m.end(),bool(macro.group(2))))
            projection[start:m.end()] = list(text[start:m.end()])
    projection = ''.join(projection)
    # Remove continuation backslashes without changing locations.
    projection = re.sub(r'\\(?=\r?\n)', ' ', projection)
    rows = ast_rows(text, projection)
    # ast-grep uses UTF-8 byte offsets; report original character columns.
    def offset_map(value):
        offsets, total = [0], 0
        for c in value:
            total += len(c.encode('utf-8')); offsets.append(total)
        return offsets
    maps = {'source.cpp':offset_map(text), 'macros.cpp':offset_map(projection)}
    def span(row):
        offsets = maps[Path(row['file']).name]
        value = row['range']['byteOffset']
        return bisect_right(offsets,value['start'])-1, bisect_right(offsets,value['end'])-1
    functions = []
    for row in rows:
        if row['ruleId']=='decomp-function' and Path(row['file']).name=='source.cpp':
            a,b=span(row)
            # Names are navigation labels only, not an ABI identity claim.
            header=text[a:b].split('{',1)[0]
            names=re.findall(r'([\w:~]+)\s*\(', header)
            name=next((n for n in names if n not in ('__attribute__','__declspec','noexcept','throw',
                                                   'optimize','optnone','target','noinline','always_inline')), '<function>')
            functions.append((a,b,name))
    functions.sort()
    def owner(at):
        inside=[(b-a,n) for a,b,n in functions if a<=at<b]
        return min(inside)[1] if inside else '<file>'
    starts=[m.start() for m in tokens]
    lines=[0]+[m.end() for m in re.finditer('\n',text)]
    findings=[]
    def emit(rule,a,b,message,confidence='review',**details):
        normalized=re.sub(r'\s+',' ',text[a:b]).strip()
        near=max(0,bisect_right(starts,a)-1)
        context=' '.join(m.group() for m in tokens[max(0,near-6):near+7])
        identity='\0'.join((path,rule,owner(a),normalized,context,str(details.get('directive',''))))
        line=bisect_right(lines,a)
        findings.append(dict(rule=rule,path=path,line=line,column=a-lines[line-1]+1,scope=owner(a),
                             message=message,confidence=confidence,excerpt=normalized[:240],
                             fingerprint=hashlib.sha256(identity.encode()).hexdigest(),suppressed=False,**details))
    masks=[];local_names=[]
    for row in rows:
        if row['ruleId']=='decomp-local-name' and Path(row['file']).name=='source.cpp':
            a,b=span(row);local_names.append((a,b,row['text']))
        if row['ruleId'].startswith('decomp-mask'):
            match=row['metaVariables']['single']['MASK']
            a,b=span(dict(file=row['file'],range=match['range']))
            masks.append((Path(row['file']).name,a,b))
    for row in rows:
        rid=row['ruleId'];a,b=span(row)
        projected=Path(row['file']).name=='macros.cpp'
        if rid=='decomp-parse-error':
            if diagnostics is not None:
                diagnostics.append(dict(path=path,line=bisect_right(lines,a),projection=projected,
                                        excerpt=re.sub(r'\s+',' ',text[a:b])[:120]))
            continue
        if rid.startswith('decomp-'): continue
        if projected and rid not in ('FM001','FM002','FM005','FM007'): continue
        if rid in ('FM001','FM002'):
            extra={'depth':len(re.findall(r'\*\s*\([^)]*\*[^)]*\)',text[a:b]))} if rid=='FM002' else {}
            emit(rid,a,b,row['message'],**extra)
        elif rid=='FM003-array':
            name=row['metaVariables']['single']['NAME']['text']
            enclosing=[(y-x,x,y) for x,y,_ in functions if x<=a<y]
            if enclosing:
                _,x,y=min(enclosing)
                if len(re.findall(r'\b'+re.escape(name)+r'\b',clean[x:y]))==1:
                    emit('FM003',a,b,'Local array has no lexical use beyond its declaration.',variable=name)
        elif rid in ('FM003-volatile','FM003-trash'):
            # AST declarator names exclude references to globals in initializers.
            for x,y,name in local_names:
                if not a<=x<y<=b: continue
                if rid=='FM003-volatile' or TRASH.fullmatch(name):
                    emit('FM003',a,b,row['message'],variable=name)
        elif rid=='FM004-bytes':
            match=re.search(r'\b(\w+)\s*\[[^\]]*\]\s*=\s*\{([^{}]*)\}',clean[a:b],re.S)
            if not match: continue
            items=[s.strip() for s in match.group(2).split(',') if s.strip()]
            if not items or not all(re.fullmatch(r'(?:0[xX][\da-fA-F]+|\d+)[uU]?',s) for s in items): continue
            values=[int(s.rstrip('uU'),16 if s.lower().startswith('0x') else 10) for s in items]
            if len(values)<8 or any(v>255 for v in values): continue
            data=bytes(values);shapes=[]
            if len(data)%4==0:
                words=struct.unpack('>'+str(len(data)//4)+'I',data)
                if len(words)>=3 and all(0x80000000<=v<0x81800000 and v%4==0 for v in words):
                    shapes.append('GameCube-address-shaped words')
            for width,fmt in ((4,'f'),(8,'d')):
                if len(data)%width or len(data)//width<2: continue
                numbers=struct.unpack('>'+str(len(data)//width)+fmt,data)
                if sum(math.isfinite(v) and 1e-8<=abs(v)<=1e8 for v in numbers)/len(numbers)>=0.75:
                    shapes.append('big-endian float'+str(width*8)+'-shaped values')
            if shapes: emit('FM004',a,b,'; '.join(shapes)+'; inspect consumers before retyping.','heuristic',variable=match.group(1),byte_count=len(data))
        elif rid=='FM006':
            directive=next((d for x,y,d,_ in directives if x<=a<y),re.sub(r'\s+',' ',text[a:b]).strip())
            following=next((n for x,y,n in functions if x>=b),'<end>')
            emit('FM006',a,b,row['message'],directive=directive,pragma_scope=owner(a) if owner(a)!='<file>' else 'before:'+following)
        elif rid=='FM007':
            if projected and not any(x<=a<y and functionlike for x,y,functionlike in macro_ranges): continue
            if not any(file==Path(row['file']).name and x<=a and b<=y for file,x,y in masks):
                emit('FM007',a,b,row['message'],'heuristic')
        elif rid=='FM005':
            pass  # Unified token fallback below covers GNU + MWCC + opaque macro bodies once.
        else:
            raise ValueError('unhandled ast-grep rule '+rid)
    for token in tokens:
        if token.lastgroup!='id' or token.group() not in ('asm','__asm','__asm__','ASM'): continue
        a,b=token.span()
        directive=next(((d,m) for x,y,d,m in directives if x<=a<y),(None,None))
        if directive[1]==token.group() and not any(x<=a<y for x,y,_ in macro_ranges): continue
        emit('FM005',a,b,'Assembly requires an exact reviewed macro exception; never auto-remove it.',macro=directive[1],directive=directive[0])
    unique={(r['rule'],r['line'],r['column'],r.get('variable')):r for r in findings}
    ordered=sorted(unique.values(),key=lambda r:(r['path'],r['line'],r['column'],r['rule']))
    occurrences=Counter()
    for row in ordered:
        key=row['fingerprint'];ordinal=occurrences[key];occurrences[key]+=1
        row['fingerprint']=hashlib.sha256((key+':'+str(ordinal)).encode()).hexdigest()
    return ordered


def load_policy(path):
    data=json.loads(path.read_text(encoding='utf-8'))
    if set(data)!={'schema_version','exceptions','pragma_allowlist'} or data['schema_version']!=1:
        raise ValueError('policy requires schema_version=1, exceptions and pragma_allowlist')
    if not isinstance(data['exceptions'],list) or not isinstance(data['pragma_allowlist'],list): raise ValueError('policy lists required')
    for e in data['exceptions']:
        if not isinstance(e,dict) or set(e)!={'fingerprint','reason'} or not isinstance(e['fingerprint'],str) or not re.fullmatch('[0-9a-f]{64}',e['fingerprint']) or not isinstance(e['reason'],str) or not e['reason'].strip():
            raise ValueError('exception requires exact fingerprint and nonempty reason')
    for e in data['pragma_allowlist']:
        if not isinstance(e,dict) or set(e)!={'path','scope','directive','count','reason'} or not all(isinstance(e[k],str) and e[k].strip() for k in ('path','scope','directive','reason')) or not isinstance(e['count'],int) or isinstance(e['count'],bool) or e['count']<1:
            raise ValueError('pragma approval requires exact path/scope/directive, positive count, reason')
        if any(c in e['path'] for c in '*?\\:') or Path(e['path']).is_absolute() or '..' in Path(e['path']).parts:
            raise ValueError('pragma path must be exact repository-relative POSIX path')
    if len({e['fingerprint'] for e in data['exceptions']})!=len(data['exceptions']): raise ValueError('duplicate exception fingerprint')
    if len({(e['path'],e['scope'],e['directive']) for e in data['pragma_allowlist']})!=len(data['pragma_allowlist']): raise ValueError('duplicate pragma approval')
    return data


def apply_policy(findings, policy):
    result=[dict(row) for row in findings]
    exceptions={e['fingerprint']:e['reason'] for e in policy['exceptions']}
    used=Counter()
    for row in result:
        reason=exceptions.get(row['fingerprint'])
        if row['rule']=='FM005' and not row.get('macro'): reason=None
        if not reason and row['rule']=='FM006':
            for i,e in enumerate(policy['pragma_allowlist']):
                if (row['path'],row.get('pragma_scope'),row.get('directive'))==(e['path'],e['scope'],e['directive']) and used[i]<e['count']:
                    used[i]+=1;reason=e['reason'];break
        if reason: row.update(suppressed=True,review_reason=reason)
    return result


def main(argv=None):
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('paths',nargs='*',help='repository-relative files/directories; default src and include')
    p.add_argument('--root',type=Path,default=ROOT)
    p.add_argument('--policy',type=Path,default=Path('config/GUNE5D/fakematch_lint.json'))
    p.add_argument('--out',type=Path,help='JSON report under build/')
    p.add_argument('--limit',type=int,default=40,help='console rows only; JSON retains every finding')
    p.add_argument('--fail-on-findings',action='store_true')
    p.add_argument('--rule',choices=sorted(RULES),action='append')
    args=p.parse_args(argv)
    try:
        root=args.root.resolve();files=set()
        if args.limit<0: raise ValueError('--limit must be nonnegative')
        out=(root/args.out).resolve() if args.out else None
        if out and (not out.is_relative_to(root/'build') or out.suffix!='.json' or out==(root/args.policy).resolve()):
            raise ValueError('--out must be a report .json under build/, not the policy')
        for value in args.paths or ['src','include']:
            path=(root/value).resolve()
            if not path.is_relative_to(root) or not path.exists(): raise ValueError('missing or outside-root input: '+value)
            if path.is_file() and path.suffix.lower() not in EXTENSIONS: raise ValueError('not a C/C++ source input: '+value)
            for f in [path] if path.is_file() else path.rglob('*'):
                if f.is_file() and f.suffix.lower() in EXTENSIONS:
                    if not f.resolve().is_relative_to(root): raise ValueError('source symlink escapes repository: '+str(f))
                    files.add(f)
        if not files: raise ValueError('no C/C++ source files selected')
        policy_bytes=(root/args.policy).read_bytes();policy=load_policy(root/args.policy)
        rows=[];hashes={};diagnostics=[]
        for f in sorted(files):
            data=f.read_bytes();name=f.relative_to(root).as_posix();hashes[name]=hashlib.sha256(data).hexdigest()
            try: text=data.decode('utf-8')
            except UnicodeDecodeError: text=data.decode('latin-1')
            try: rows.extend(scan_source(text,name,diagnostics))
            except ValueError as e: raise ValueError(name+': '+str(e)) from e
        if any(hashlib.sha256((root/name).read_bytes()).hexdigest()!=sha for name,sha in hashes.items()):
            raise ValueError('source changed during scan; rerun on stable inputs')
        if (root/args.policy).read_bytes()!=policy_bytes: raise ValueError('policy changed during scan')
        rows=apply_policy(rows,policy);selected=sorted(set(args.rule or RULES))
        rows=[r for r in rows if r['rule'] in selected];active=[r for r in rows if not r['suppressed']]
        report=dict(schema_version=1,status='SCAN_COMPLETE',engine='ast-grep 0.45.3 + review filters',
                    interpretation='Review candidates, not proven fakematches. Parse recovery limits coverage; macros are not expanded.',
                    source_sha256=hashes,files_scanned=len(files),findings=rows,unsuppressed=len(active),suppressed=len(rows)-len(active),
                    by_rule=dict(sorted(Counter(r['rule'] for r in active).items())),by_file=dict(Counter(r['path'] for r in active).most_common()),
                    rules_selected=selected,parse_recovery=diagnostics,policy_sha256=hashlib.sha256(policy_bytes).hexdigest(),
                    rules_sha256=hashlib.sha256((ROOT/'tools/gdl/lint/rules/reconstruction.yml').read_bytes()).hexdigest(),
                    config_sha256=hashlib.sha256((ROOT/'sgconfig.yml').read_bytes()).hexdigest())
        if out:
            out.parent.mkdir(parents=True,exist_ok=True);out.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
        for r in active[:args.limit]: print(f"{r['path']}:{r['line']}:{r['column']}: {r['rule']} [{r['scope']}] {r['message']}")
        print(f"SCAN_COMPLETE: {len(files)} files; {len(active)} review candidates; {len(rows)-len(active)} reviewed exceptions.")
        print('By rule: '+json.dumps(report['by_rule'],sort_keys=True))
        print(f'Parser recovery regions: {len(diagnostics)} (includes macro projection; not a clean-code certificate).')
        if len(active)>args.limit: print(f'Console limited to {args.limit}; use --out for all findings.')
        if out: print('Report: '+str(args.out))
        return 1 if args.fail_on_findings and active else 0
    except (OSError,ValueError,TypeError,KeyError,subprocess.SubprocessError) as e:
        print('UNRESOLVED: '+str(e),file=sys.stderr);return 2


if __name__=='__main__':
    raise SystemExit(main())
