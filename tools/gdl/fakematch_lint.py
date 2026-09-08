"""Review-only ast-grep C++ reconstruction-debt scanner; never rewrites source.

Source rule families run through the pinned ast-grep rules. Python supplies
lexical-use/byte-shape/mask filters, exact review policy, and MWCC asm fallback.
Macros are scanned in a separate offset-preserving projection, not expanded.
Parse recovery is reported, not treated as proof of clean source. No type/CFG
analysis or original-source authenticity proof is implied. Exit 0: scan complete;
1: errors with --fail-on-findings, or warnings with --warnings-as-errors;
2: missing input/dependency or scanner error.
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
import time
import tomllib

ROOT = Path(__file__).resolve().parents[2]
RULES = {
    'FM001': 'Raw offset or indexed pointer cast',
    'FM002': 'Nested dereference through pointer casts',
    'FM003': 'Possible stack/allocation scaffolding',
    'FM004': 'Float/address-shaped numeric byte array',
    'FM005': 'Assembly outside a reviewed macro',
    'FM006': 'Source-level compilation override',
    'FM007': 'Unnamed hexadecimal expression constant',
    'FM008': 'Configured postprocessor dependency requiring native retirement',
    'FM009': 'Unnamed constant-offset pointer or array access',
}
POSTPROCESSORS = [('WebFrank','config/GUNE5D/webfrank.json'),('P6Frank','config/GUNE5D/p6frank.json')]
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
    # Conservative lexical evidence, not C++ type inference. Refuse ambiguity
    # (shadowing/redeclarations), and never guess from a name
    # such as "strings". Typedef-hidden pointers and member expressions are
    # intentionally outside this check until type-backed analysis exists.
    blocks=[span(row) for row in rows if row['ruleId']=='decomp-block'
            and Path(row['file']).name=='source.cpp']
    storage={span(row)[0] for row in rows if row['ruleId']=='decomp-storage-name'
             and Path(row['file']).name=='source.cpp'}
    bindings=[]
    for row in rows:
        if row['ruleId']!='decomp-binding-name' or Path(row['file']).name!='source.cpp': continue
        a,b=span(row)
        scopes=[(y-x,x,y) for x,y in blocks if x<=a<y]
        scopes += [(y-x,x,y) for x,y,_ in functions if x<=a<y]
        _,begin,end=min(scopes) if scopes else (len(text),0,len(text))
        bindings.append((row['text'],a,begin,end,a in storage))
    def explicit_storage(name, at):
        visible=[row for row in bindings if row[0]==name and row[1]<at and row[2]<=at<row[3]]
        if not visible: return None
        width=min(row[3]-row[2] for row in visible)
        nearest=[row for row in visible if row[3]-row[2]==width]
        return nearest[0] if len(nearest)==1 and nearest[0][4] else None
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
    masks=[];local_names=[];local_writes=[]
    for row in rows:
        if row['ruleId']=='decomp-local-name' and Path(row['file']).name=='source.cpp':
            a,b=span(row);local_names.append((a,b,row['text']))
        if row['ruleId']=='decomp-local-write' and Path(row['file']).name=='source.cpp':
            a,b=span(row);local_writes.append((a,b,row['text']))
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
        elif rid in ('FM009','FM009-left'):
            meta=row['metaVariables']['single']
            binding=explicit_storage(meta['BASE']['text'],a)
            if binding:
                emit('FM009',a,b,'Unnamed numeric offset into a declared pointer/array; recover the referenced string, field or element from target data. This does not prove a struct or loop.',
                     'heuristic',base=meta['BASE']['text'],offset=meta['OFFSET']['text'],
                     declaration_line=bisect_right(lines,binding[1]))
        elif rid=='FM005':
            pass  # Unified token fallback below covers GNU + MWCC + opaque macro bodies once.
        else:
            raise ValueError('unhandled ast-grep rule '+rid)
    # Fable's Critter retirement exposed locals updated solely to steer MWCC's
    # induction-variable rank. This is a lexical candidate, not dead-store proof:
    # macro expansion, aliasing and original-source authenticity remain unproved.
    for begin,end,name in functions:
        declared=[(a,b,n) for a,b,n in local_names if begin<=a<end]
        for a,b,variable in declared:
            if sum(n==variable for _,_,n in declared)!=1: continue  # shadowing ambiguous
            declaration_start=max(clean.rfind(';',begin,a),clean.rfind('{',begin,a))+1
            if re.search(r'\b(?:volatile|static)\b',clean[declaration_start:a]): continue
            writes={x for x,y,n in local_writes if begin<=x<end and n==variable}
            if not writes: continue
            uses={m.start() for m in tokens if begin<=m.start()<end and m.lastgroup=='id' and m.group()==variable}
            if uses==writes|{a}:
                emit('FM003',a,b,'Local is only declared and assigned/incremented, never otherwise read; investigate an artificial induction-variable carrier.',
                     'heuristic',variable=variable,pattern='write-only-local')
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
    raw=path.read_text(encoding='utf-8')
    data=tomllib.loads(raw) if path.suffix=='.toml' else json.loads(raw)
    required={'schema_version','exceptions','pragma_allowlist'}
    if not required.issubset(data) or set(data)-required-{'warning_pragmas'} or data['schema_version']!=1:
        raise ValueError('policy requires schema_version=1, exceptions and pragma_allowlist')
    warnings=data.get('warning_pragmas',[])
    if not isinstance(warnings,list) or any(w not in ('#pragma dont_inline on','#pragma dont_inline off') for w in warnings) or len(set(warnings))!=len(warnings):
        raise ValueError('warning_pragmas must contain distinct approved dont_inline directives')
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
        row['severity']='error'
        if row['rule']=='FM006' and row.get('directive') in policy.get('warning_pragmas',[]):
            row.update(severity='warning',suppressed=False)
            continue  # Warning debt stays visible, even if an old exception exists.
        reason=exceptions.get(row['fingerprint'])
        if row['rule']=='FM008': reason=None  # Inventory is not a source-policy exception.
        if row['rule']=='FM005' and not row.get('macro'): reason=None
        if not reason and row['rule']=='FM006':
            for i,e in enumerate(policy['pragma_allowlist']):
                if (row['path'],row.get('pragma_scope'),row.get('directive'))==(e['path'],e['scope'],e['directive']) and used[i]<e['count']:
                    used[i]+=1;reason=e['reason'];break
        if reason: row.update(suppressed=True,review_reason=reason)
    return result


def postprocessor_findings(root, source_names, include_all=False):
    """Configured dependencies, NOT a claim that a rule executed in this build."""
    findings=[];hashes={}
    if not any((root/p).exists() for _,p in POSTPROCESSORS):
        # Retirement is explicit, not a missing-config fallback. Full graph
        # enforcement belongs to native_build and the build provenance gate.
        relative='tools/gdl/native_build.py'
        hashes[relative]=hashlib.sha256((root/relative).read_bytes()).hexdigest()
        return findings,hashes
    for engine,relative in POSTPROCESSORS:
        if not (root/relative).exists(): continue
        raw=(root/relative).read_bytes();hashes[relative]=hashlib.sha256(raw).hexdigest()
        text=raw.decode('utf-8');config=json.loads(text)
        if config.get('version')!=1 or not isinstance(config.get('units'),dict):
            raise ValueError('invalid postprocessor inventory: '+relative)
        locations={}
        for match in re.finditer(r'(?m)^[ \t]*"function"\s*:\s*("(?:[^"\\]|\\.)*")',text):
            name=json.loads(match.group(1))
            locations.setdefault(name,[]).append(text.count('\n',0,match.start())+1)
        for unit,rules in config['units'].items():
            if not isinstance(unit,str) or '..' in Path(unit).parts or ':' in unit or '\\' in unit:
                raise ValueError('invalid postprocessor unit path')
            rules=[rules] if isinstance(rules,dict) else rules
            if not isinstance(rules,list): raise ValueError('invalid postprocessor rule list')
            candidates=['src/'+unit+extension for extension in ('.c','.cpp','.cc')]
            selected=next((p for p in candidates if p in source_names),None)
            for rule in rules:
                name=rule.get('function') if isinstance(rule,dict) else None
                if not isinstance(name,str) or not name or not locations.get(name):
                    raise ValueError('missing function/location in '+relative)
                line=locations[name].pop(0)
                if selected is None and not include_all: continue
                identity=engine+'\0'+unit+'\0'+name
                findings.append(dict(rule='FM008',path=relative,line=line,column=1,scope=name,
                    message=f'{engine} dependency: {unit}::{name}. Reconstruct native output and prove whole-TU preservation before retiring; do not disable the guard.',
                    confidence='configured-dependency',excerpt=name,
                    fingerprint=hashlib.sha256(identity.encode()).hexdigest(),suppressed=False,
                    unit=unit,function=name,source_path=selected,postprocessor=engine))
    return findings,hashes


def diagnostic(row,root,style):
    message=f"{row['rule']} [{row['scope']}] {row['message']}"
    severity=row.get('severity','error')
    if style=='github':
        def escape(value,property=False):
            value=str(value).replace('%','%25').replace('\r','%0D').replace('\n','%0A')
            return value.replace(':','%3A').replace(',','%2C') if property else value
        return (f"::{severity} file={escape(row['path'],True)},line={row['line']},col={row['column']},"
                f"title={row['rule']}::{escape(message)}")
    if style=='problems':
        message=re.sub(r'[\r\n]+',' ',f"[{row['scope']}] {row['message']}")
        return f"{(root/row['path']).as_posix()}:{row['line']}:{row['column']}: {severity} {row['rule']}: {message}"
    return f"{row['path']}:{row['line']}:{row['column']}: {severity}: {message}"


def watch(argv):
    """VS Code task owns process lifetime; cache unchanged snapshots between saves."""
    cache={};args=[value for value in argv if value!='--watch']
    try:
        previous=None
        while True:
            watched=[p for base in ('src','include') for p in (ROOT/base).rglob('*')
                     if p.is_file() and p.suffix.lower() in EXTENSIONS]
            watched += [ROOT/p for p in ('config/GUNE5D/fakematch_lint.toml','sgconfig.yml',
                       'tools/gdl/lint/rules/reconstruction.yml',*[p for _,p in POSTPROCESSORS])]
            state=tuple((str(p),p.stat().st_mtime_ns,p.stat().st_size) if p.exists() else (str(p),None,None)
                        for p in sorted(watched))
            if state!=previous:
                print('GDL_LINT_BEGIN',flush=True)
                result=main(args,_cache=cache)
                print(f'GDL_LINT_END status={result}',flush=True)
                previous=state
            time.sleep(1)
    except KeyboardInterrupt:
        return 0


def main(argv=None, _cache=None):
    argv=list(sys.argv[1:] if argv is None else argv)
    if '--watch' in argv:
        if '--root' in argv or any(a.startswith('--root=') for a in argv):
            print('UNRESOLVED: watch uses the current project root',file=sys.stderr);return 2
        return watch(argv)
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('paths',nargs='*',help='repository-relative files/directories; default src and include')
    p.add_argument('--root',type=Path,default=ROOT)
    p.add_argument('--policy',type=Path,default=Path('config/GUNE5D/fakematch_lint.toml'))
    p.add_argument('--out',type=Path,help='JSON report under build/')
    p.add_argument('--limit',type=int,default=40,help='console rows only; JSON retains every finding')
    p.add_argument('--fail-on-findings',action='store_true')
    p.add_argument('--warnings-as-errors',action='store_true',help='also fail on warning-only debt')
    p.add_argument('--format',choices=('human','github','problems'),default='human')
    p.add_argument('--postprocessors',action='store_true',help='include configured WebFrank/P6Frank dependencies (FM008)')
    p.add_argument('--watch',action='store_true',help='editor task: rescan saved changes, reuse unchanged source snapshots')
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
        engine_hash=hashlib.sha256((ROOT/'sgconfig.yml').read_bytes()+(ROOT/'tools/gdl/lint/rules/reconstruction.yml').read_bytes()).hexdigest()
        for f in sorted(files):
            data=f.read_bytes();name=f.relative_to(root).as_posix();hashes[name]=hashlib.sha256(data).hexdigest()
            try: text=data.decode('utf-8')
            except UnicodeDecodeError: text=data.decode('latin-1')
            try:
                key=(hashes[name],engine_hash)
                saved=_cache.get(name) if _cache is not None else None
                if saved and saved[0]==key:
                    found,recovery=saved[1],saved[2]
                else:
                    recovery=[];found=scan_source(text,name,recovery)
                    if _cache is not None: _cache[name]=(key,found,recovery)
                rows.extend(found);diagnostics.extend(recovery)
            except ValueError as e: raise ValueError(name+': '+str(e)) from e
        if any(hashlib.sha256((root/name).read_bytes()).hexdigest()!=sha for name,sha in hashes.items()):
            raise ValueError('source changed during scan; rerun on stable inputs')
        if (root/args.policy).read_bytes()!=policy_bytes: raise ValueError('policy changed during scan')
        rows=apply_policy(rows,policy);post_hashes={}
        if args.postprocessors:
            dependencies,post_hashes=postprocessor_findings(root,set(hashes),include_all=not args.paths)
            pinned={(r['source_path'],r['function']):r for r in dependencies if r['source_path']}
            for row in rows:
                dependency=pinned.get((row['path'],row['scope']))
                if dependency:
                    row['postprocessor_dependency']={k:dependency[k] for k in ('postprocessor','unit','function')}
            rows=dependencies+rows  # Native-retirement obligations lead the CI/editor report.
        elif args.rule and 'FM008' in args.rule:
            raise ValueError('FM008 requires --postprocessors')
        selected=sorted(set(args.rule or [r for r in RULES if r!='FM008' or args.postprocessors]))
        rows=[r for r in rows if r['rule'] in selected];active=[r for r in rows if not r['suppressed']]
        warnings=sum(r.get('severity')=='warning' for r in active)
        errors=len(active)-warnings
        report=dict(schema_version=1,status='SCAN_COMPLETE',engine='ast-grep 0.45.3 + review filters',
                    interpretation='Review candidates, not proven fakematches. Parse recovery limits coverage; macros are not expanded.',
                    source_sha256=hashes,files_scanned=len(files),findings=rows,unsuppressed=len(active),suppressed=len(rows)-len(active),
                    errors=errors,warnings=warnings,warnings_as_errors=args.warnings_as_errors,
                    by_rule=dict(sorted(Counter(r['rule'] for r in active).items())),by_file=dict(Counter(r['path'] for r in active).most_common()),
                    rules_selected=selected,parse_recovery=diagnostics,policy_sha256=hashlib.sha256(policy_bytes).hexdigest(),
                    postprocessor_config_sha256=post_hashes,
                    rules_sha256=hashlib.sha256((ROOT/'tools/gdl/lint/rules/reconstruction.yml').read_bytes()).hexdigest(),
                    config_sha256=hashlib.sha256((ROOT/'sgconfig.yml').read_bytes()).hexdigest())
        if out:
            out.parent.mkdir(parents=True,exist_ok=True);out.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
        for r in active if args.format=='problems' else active[:args.limit]: print(diagnostic(r,root,args.format))
        print(f"SCAN_COMPLETE: {len(files)} files; {len(active)} review candidates; {len(rows)-len(active)} reviewed exceptions.")
        print(f'Diagnostics: {errors} errors; {warnings} warnings; warnings-as-errors={args.warnings_as_errors}.')
        print('By rule: '+json.dumps(report['by_rule'],sort_keys=True))
        print(f'Parser recovery regions: {len(diagnostics)} (includes macro projection; not a clean-code certificate).')
        if len(active)>args.limit and args.format!='problems': print(f'Console limited to {args.limit}; use --out for all findings.')
        if out: print('Report: '+str(args.out))
        return 1 if (args.fail_on_findings and errors) or (args.warnings_as_errors and warnings) else 0
    except (OSError,ValueError,TypeError,KeyError,subprocess.SubprocessError) as e:
        if args.format!='human':
            print(diagnostic(dict(path='sgconfig.yml',line=1,column=1,rule='FM000',scope='scanner',
                                  message='Scan incomplete: '+str(e)),args.root.resolve(),args.format))
        print('UNRESOLVED: '+str(e),file=sys.stderr);return 2


if __name__=='__main__':
    raise SystemExit(main())
