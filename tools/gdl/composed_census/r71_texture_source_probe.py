"""Replay the finite R71 pbInitTlutRegions source controls, without mutation.

Each probe compiles a complete scratch TU with the active Ninja compiler/flags.
The unedited source must first reproduce the complete active raw object. Results
are experiments, not a source-equivalence or universal source-exhaustion proof.
Compiler diagnostics and each complete source/object are retained under build/.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import itertools

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from tools.gdl.composed_census import cv_probe as cv
from tools.gdl.composed_census.r68_interfaces_raw_control import capture
from tools.gdl.composed_census.r67_runtime_visibility_probe import inventory
from tools.fix_exception_objects import Elf

UNIT, FN = 'game/pb/pb_texture', 'pbInitTlutRegions'
SOURCE_SHA256 = 'cf1c6a3d60e5bff87494fa5a873b3847586beb8b8e2517d057332d18e6029fa7'
# R89 changes only fn_800C72DC, not this tool's pbInitTlutRegions controls.
# Both complete contexts are reviewed, not a wildcard over adjacent source.
R89_SOURCE_SHA256 = 'f485e02d0e68969ce8c3bfbcbb2103486d7019d99f6de46b8d5344b9a8fbea20'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def source_forms(source):
    source = source.replace('\r\n', '\n')
    if sha(source.encode()) not in (SOURCE_SHA256, R89_SOURCE_SHA256):
        raise ValueError('R71 source baseline changed; rederive the finite controls before updating the digest')
    old = 'void GXInitTlutRegion(void* region, u32 tmem_addr, u32 tlut_size);'
    if source.count(old) != 1 or source.count('typedef u8 GXBool;') != 1:
        raise ValueError('source shape changed')
    enum_header = source.replace('typedef u8 GXBool;', '#include <dolphin/gx/GXEnum.h>')
    enum_arg = enum_header.replace(old, old.replace('u32 tlut_size', 'GXTlutSize tlut_size'))
    full_type = enum_arg.replace('#include <dolphin/gx/GXEnum.h>', '#include <dolphin/gx/GXStruct.h>')
    full_type = full_type.replace('GXInitTlutRegion(void* region', 'GXInitTlutRegion(GXTlutRegion* region')
    full_type = full_type.replace('GXInitTlutRegion(slot + 0x30,', 'GXInitTlutRegion((GXTlutRegion*)(slot + 0x30),')
    forms = {'baseline': source, 'header_only': enum_header, 'enum_arg': enum_arg,
             'enum_names': enum_arg.replace('slot + 0x30, addr, 1)', 'slot + 0x30, addr, GX_TLUT_16)').replace('slot + 0x30, addr, 0x10)', 'slot + 0x30, addr, GX_TLUT_256)'),
             'full_type': full_type}
    start = source.index('typedef u8 GXBool;')
    end = source.index('void DCFlushRange', start)
    forms['full_header'] = source[:start] + '#include <dolphin/gx/GXTexture.h>\n' + source[end:]
    typed = full_type.replace('u8  regions[0x2f][0x10];', 'GXTlutRegion regions[0x2f];')
    forms['typed_member_only'] = typed
    forms['typed_direct_member'] = typed.replace('(GXTlutRegion*)(slot + 0x30)', '&lbl_802C7438.regions[i]')
    forms['typed_view_member'] = typed.replace('(GXTlutRegion*)(slot + 0x30)', '&((PbTlutMgrView*)mgr)->regions[i]')
    forms['typed_region_pointer'] = typed.replace('    u8* slot;\n', '    u8* slot;\n    GXTlutRegion* region;\n').replace('        slot = mgr + i * 0x10;', '        region = &((PbTlutMgrView*)mgr)->regions[i];').replace('(GXTlutRegion*)(slot + 0x30)', 'region')
    forms['typed_manager'] = typed.replace('    u8* mgr = (u8*)&lbl_802C7438;\n    u8* slot;', '    PbTlutMgrView* mgr = &lbl_802C7438;\n    u8* slot;').replace('        slot = mgr + i * 0x10;', '        slot = (u8*)mgr + i * 0x10;').replace('(GXTlutRegion*)(slot + 0x30)', '&mgr->regions[i]').replace('*(s32*)((slot = mgr + i * 4) + 0x320)', '*(s32*)((slot = (u8*)mgr + i * 4) + 0x320)')
    forms['slot_view_field'] = typed.replace('(GXTlutRegion*)(slot + 0x30)', '&((PbTlutMgrView*)slot)->regions[0]')
    forms['slot_region_steps'] = full_type.replace('(GXTlutRegion*)(slot + 0x30)', '((GXTlutRegion*)slot) + 3')
    forms['slot_void_steps'] = enum_arg.replace('    u8* slot;\n', '    void* slot;\n').replace('slot + 0x30', '(u8*)slot + 0x30')
    forms['slot_inline_assignment'] = enum_arg.replace('        slot = mgr + i * 0x10;\n', '').replace('GXInitTlutRegion(slot + 0x30,', 'GXInitTlutRegion((slot = mgr + i * 0x10) + 0x30,')
    forms['slot_offsetof'] = enum_arg.replace('slot + 0x30', 'slot + offsetof(PbTlutMgrView, regions)')
    forms['addr_int'] = enum_arg.replace('    u32 addr = 0xc0000;', '    unsigned int addr = 0xc0000;')
    forms['index_int'] = enum_arg.replace('void pbInitTlutRegions(void) {\n    s32 i;', 'void pbInitTlutRegions(void) {\n    int i;')
    forms['addr_arg_uint'] = enum_arg.replace('u32 tmem_addr, GXTlutSize', 'unsigned int tmem_addr, GXTlutSize')
    forms['size_arg_uint'] = source.replace('u32 tlut_size);', 'unsigned int tlut_size);')
    forms['size_arg_int'] = source.replace('u32 tlut_size);', 'int tlut_size);')
    begin = source.index('void pbInitTlutRegions(void) {')
    end = source.index('\nstatic void* sTlutRegionCallback(u32 name) {',begin)
    fnbody = source[begin:end]
    def form(label, body):
        forms[label] = source[:begin]+body+source[end:]
    form('call_update_slot',fnbody.replace('slot + 0x30,', '(slot += 0x30),'))
    form('existing_callback',fnbody.replace('        slot = mgr + i * 0x10;\n', '').replace('slot + 0x30,', 'sTlutRegionCallback(i),'))
    forms['callback_member_joint'] = forms['existing_callback'].replace('return lbl_802C7468 + name * 0x10;', 'return ((u8*)&lbl_802C7438) + name * 0x10 + 0x30;')
    forms['callback_member_only'] = source.replace('return lbl_802C7468 + name * 0x10;', 'return ((u8*)&lbl_802C7438) + name * 0x10 + 0x30;')
    for label in ('existing_callback', 'callback_member_joint'):
        full = forms[label]
        cb_start = full.index('static void* sTlutRegionCallback(u32 name) {')
        cb_end = full.index('\n}', cb_start) + 2
        callback = full[cb_start:cb_end]
        full = full[:cb_start] + full[cb_end:]
        full = full.replace('static void* sTlutRegionCallback(u32 name);', callback)
        forms[label + '_defined_first'] = full
    form('call_array_address',fnbody.replace('slot + 0x30,', '&slot[0x30],'))
    form('handle_without_slot', fnbody.replace('*(s32*)((slot = mgr + i * 4) + 0x320)', '*(s32*)(mgr + i * 4 + 0x320)'))
    form('handle_typed_view', fnbody.replace('*(s32*)((slot = mgr + i * 4) + 0x320)', '((PbTlutMgrView*)mgr)->handles[i]'))
    form('handle_typed_global', fnbody.replace('*(s32*)((slot = mgr + i * 4) + 0x320)', 'lbl_802C7438.handles[i]'))
    forms['typed_manager_handle_joint'] = forms['typed_manager'].replace('*(s32*)((slot = (u8*)mgr + i * 4) + 0x320)', 'mgr->handles[i]')
    form('direct_region_expr', fnbody.replace('        slot = mgr + i * 0x10;\n', '').replace('slot + 0x30,', 'mgr + i * 0x10 + 0x30,'))
    form('advance_slot_before_call',fnbody.replace('        GXInitTlutRegion(slot + 0x30,', '        slot += 0x30;\n        GXInitTlutRegion(slot,'))
    form('scope_slot_each_loop',fnbody.replace('        slot = mgr + i * 0x10;', '        u8* region = mgr + i * 0x10;').replace('slot + 0x30,', 'region + 0x30,'))
    form('size_local',fnbody.replace('    u8* slot;','    u8* slot;\n    GXTlutSize size;').replace('    for (i = lbl_80348FC8[0];','    size = GX_TLUT_16;\n    for (i = lbl_80348FC8[0];').replace('    for (; i < lbl_80348FD0[1];','    size = GX_TLUT_256;\n    for (; i < lbl_80348FD0[1];').replace('slot + 0x30, addr, 1)', 'slot + 0x30, addr, size)').replace('slot + 0x30, addr, 0x10)', 'slot + 0x30, addr, size)'))
    forms['size_local'] = forms['size_local'].replace('typedef u8 GXBool;', '#include <dolphin/gx/GXEnum.h>')
    form('split_initializers',fnbody.replace('    u32 addr = 0xc0000;', '    u32 addr;').replace('    u8* mgr = (u8*)&lbl_802C7438;', '    u8* mgr;').replace('    for (i = lbl_80348FC8[0];', '    addr = 0xc0000;\n    mgr = (u8*)&lbl_802C7438;\n    for (i = lbl_80348FC8[0];'))
    declarations=['    s32 i;\n','    u32 addr = 0xc0000;\n','    u8* mgr = (u8*)&lbl_802C7438;\n','    u8* slot;\n']
    block=''.join(declarations)
    for order in itertools.permutations(range(4)):
        form('decl_'+''.join(map(str,order)), fnbody.replace(block,''.join(declarations[i] for i in order)))
    return forms


def section_layout(path):
    elf = Elf(str(path))
    return {elf.names[i]: dict(type=h[1], flags=h[2], size=h[5], alignment=h[8])
            for i, h in enumerate(elf.sh) if h[2] & 2}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--only', nargs='+', help='Probe these named forms plus baseline')
    args = parser.parse_args(argv)
    edge = cv.read_edges()[UNIT]
    source_path, raw_path = ROOT / edge['src'], ROOT / edge['body_o']
    source_bytes, raw_bytes = source_path.read_bytes(), raw_path.read_bytes()
    source = source_bytes.decode().replace('\r\n', '\n')
    forms = source_forms(source)
    if args.only and not set(args.only) <= forms.keys():
        parser.error('unknown source forms: ' + repr(sorted(set(args.only) - forms.keys())))
    baseline = capture(UNIT)
    folder = Path(tempfile.mkdtemp(prefix='r71_texture_probe_', dir=ROOT / 'build'))
    target, _ = inventory(ROOT / 'build/GUNE5D/obj/game/pb/pb_texture.o')
    before, before_data = inventory(raw_path)
    before_layout = section_layout(raw_path)
    result = dict(schema_version=1, unit=UNIT, function=FN, baseline=baseline, source=source,
                  raw_bytes=raw_bytes.hex(), processed_bytes=(ROOT/'build/GUNE5D/src/game/pb/pb_texture.o').read_bytes().hex(),
                  compiler_sha256=sha((ROOT/'build/compilers'/edge['mw']/'mwcceppc.exe').read_bytes()),
                  target=target[FN], baseline_layout=before_layout,
                  config=json.loads((ROOT/'config/GUNE5D/webfrank.json').read_text()), probes={})
    for label, fullsource in forms.items():
        if args.only and label not in ['baseline'] + args.only:
            continue
        subdir = folder / ('r71_texture_' + label)
        subdir.mkdir()
        path = subdir / 'pb_texture.c'
        path.write_bytes(fullsource.encode())
        trial = dict(edge, src=str(path.relative_to(ROOT)), _command_trace=[])
        obj, error = cv.compile_with(trial, edge['mw'], edge['cflags'], subdir/'r71_texture.o', subdir)
        row = dict(source=str(path.relative_to(ROOT)), source_sha256=sha(path.read_bytes()), error=error, commands=trial['_command_trace'])
        result['probes'][label] = row
        if not obj or error:
            row['status'] = 'COMPILE_FAILURE'
            print(label, row['status'], error)
            continue
        after, after_data = inventory(obj)
        raw = obj.read_bytes()
        if label == 'baseline' and raw != raw_bytes:
            raise ValueError('scratch source baseline failed full-object fidelity')
        elf = Elf(str(obj))
        sections = [h for i,h in enumerate(elf.sh) if elf.names[i] == '.text']
        if len(sections) != 1:
            raise ValueError('ambiguous text')
        begin, size = sections[0][4] + after[FN]['offset'], after[FN]['size']
        a, b = bytes.fromhex(after[FN]['body']), bytes.fromhex(target[FN]['body'])
        words = [dict(offset=hex(i), ours=a[i:i+4].hex(), target=b[i:i+4].hex())
                 for i in range(0, max(len(a), len(b)), 4) if a[i:i+4] != b[i:i+4]]
        row.update(status='COMPILED', object=str(obj.relative_to(ROOT)), raw_sha256=sha(raw), functions=after,
                   target_count=len(b)//4, ours_count=len(a)//4, differing_words=len(words), words=words,
                   changed_bodies=sorted(name for name in before.keys() | after.keys() if before.get(name, {}).get('body') != after.get(name, {}).get('body')),
                   changed_function_records=sorted(name for name in before.keys() | after.keys() if before.get(name) != after.get(name)),
                   nontext_equal={k:v for k,v in before_data.items() if k!='.text'} == {k:v for k,v in after_data.items() if k!='.text'},
                   allocated_layout_equal=before_layout == section_layout(obj),
                   complete_bytes_outside_flagship_equal=(len(raw)==len(raw_bytes) and raw[:begin]==raw_bytes[:begin] and raw[begin+size:]==raw_bytes[begin+size:]))
        print(label, f'{len(b)//4}/{len(a)//4}', 'words',len(words), 'bodies',row['changed_bodies'],
              'data',row['nontext_equal'], 'outside',row['complete_bytes_outside_flagship_equal'])
    if source_path.read_bytes()!=source_bytes or raw_path.read_bytes()!=raw_bytes:
        raise ValueError('production input drift')
    output=folder/'r71_texture_results.json'
    output.write_text(json.dumps(result,indent=2)+'\n')
    print(output)


if __name__ == '__main__':
    main()
