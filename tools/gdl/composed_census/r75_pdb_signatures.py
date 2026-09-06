"""Read bounded AUDIO/ML_MEM PDB2 signatures and lexical record inventories.

IMPORTABLE CORE: parse_types, symbol_records, inspect_streams, inspect_pdb.
No build, source edits, or symbol-corpus writes. Xbox metadata is evidence,
not proof of GameCube source identity. Optimized-away locals can be absent.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from tools.gdl.composed_census.r68_sound_data_recovery import pdb_streams

REQUIRED = {
    'AUDIO.OBJ': frozenset(('AudioFindPart', 'AudioFindBank', 'AudioFindSound', 'AudioUnloadPart')),
    'ML_MEM.OBJ': frozenset(('AllocMem', 'AllocMem32', 'GetMemBase', 'BytesFree', 'AllocFile', 'MLMReadFile')),
}
OPTIONAL = frozenset(('AudioBankLoadName', 'AudioBankQueueName'))
PRIMITIVES = {3: 'void', 0x70: 'char', 0x74: 'int32', 0x75: 'uint32',
              0x470: 'char* (32-bit near)', 0x475: 'uint32* (32-bit near)',
              0x12: 'long32', 0x403: 'void* (32-bit near)', 0x23: 'uint64',
              0x73: 'uint16', 0x72: 'int16', 0x11: 'short16', 0x21: 'ushort16'}


def unpack(fmt, buf, at=0):
    if at < 0 or at + struct.calcsize(fmt) > len(buf):
        raise ValueError('truncated record field')
    return struct.unpack_from(fmt, buf, at)


def pstr(buf, at):
    if at >= len(buf) or at + 1 + buf[at] > len(buf):
        raise ValueError('truncated counted name')
    return buf[at+1:at+1+buf[at]].decode('latin1')


def parse_types(tpi):
    version, header, first, last, size = unpack('<5I', tpi)
    if header < 20 or header > len(tpi) or last < first or header+size > len(tpi):
        raise ValueError('invalid TPI header')
    types, at = {}, header
    for index in range(first, last):
        length, kind = unpack('<HH', tpi, at)
        end = at + length + 2
        if length < 2 or end > header+size:
            raise ValueError('invalid TPI record')
        types[index] = dict(index=hex(index), offset=hex(at), kind=hex(kind),
                            bytes=tpi[at:end].hex(), payload=tpi[at+4:end].hex())
        at = end
    if at != header+size:
        raise ValueError('TPI record range does not cover declared bytes')

    def describe(index, depth=0):
        if depth > 12:
            raise ValueError('type recursion')
        if index < 0x1000:
            return dict(index=hex(index), primitive=PRIMITIVES.get(index, 'UNMODELLED'))
        if index not in types:
            raise ValueError('unresolved type index')
        row = dict(types[index])
        body, kind = bytes.fromhex(row['payload']), int(row['kind'], 16)
        if kind == 0x1008:
            ret, call, attr, count, args = unpack('<IBBHI', body)
            arguments = describe(args, depth+1)
            if arguments.get('kind') != '0x1201' or len(arguments['arguments']) != count:
                raise ValueError('procedure parameter count differs from argument list')
            row.update(return_type=describe(ret, depth+1), calling_convention=call,
                       function_attributes=attr, parameter_count=count, arguments=arguments)
        elif kind == 0x1201:
            count = unpack('<I', body)[0]
            row['arguments'] = [describe(x, depth+1) for x in unpack(f'<{count}I', body, 4)]
        elif kind in (0x1001, 0x1002):
            row['target'] = describe(unpack('<I', body)[0], depth+1)
            if kind == 0x1001:
                row['modifiers'] = unpack('<H', body, 4)[0]
        elif kind == 0x1005:
            count, prop, fields, derived, vshape, amount = unpack('<HHIIIH', body)
            row.update(member_count=count, properties=hex(prop), fieldlist=hex(fields),
                       size=amount if amount < 0x8000 else None,
                       name=pstr(body, 18) if amount < 0x8000 else None,
                       forward_reference=bool(prop & 0x80))
        elif kind == 0x1003:
            elem, idx, amount = unpack('<IIH', body)
            row.update(element=describe(elem, depth+1), index_type=describe(idx, depth+1),
                       size_bytes=amount if amount < 0x8000 else None)
        return row

    return types, describe, dict(tpi_version=version, tpi_header=header,
                                 first_type=hex(first), last_type=hex(last))


def symbol_records(buf, describe, names):
    if len(buf) < 4:
        raise ValueError('truncated symbol stream')
    pos, records = 4, []
    while pos < len(buf):
        length, kind = unpack('<HH', buf, pos)
        end = pos+length+2
        if length < 2 or end > len(buf):
            raise ValueError('invalid symbol record')
        body = buf[pos+4:end]
        row = dict(record_offset=pos, kind=hex(kind), bytes=buf[pos:end].hex())
        if kind in (0x100a, 0x100b):
            parent, pend, nextp, size, dbgs, dbge, typ, off, seg = unpack('<8IH', body)
            row.update(name=pstr(body, 35), parent=parent, end=pend, next=nextp, size=size,
                       debug_start=dbgs, debug_end=dbge, type=describe(typ), offset=off,
                       segment=seg, flags=body[34], category='procedure')
        elif kind == 0x1006:
            off, typ = unpack('<iI', body)
            # The location sign is evidence, not a universal argument/local classifier.
            row.update(name=pstr(body, 8), offset=off, type=describe(typ), category='BP-relative')
        elif kind == 0x1001:
            typ, reg = unpack('<IH', body)
            row.update(name=pstr(body, 6), register=reg, type=describe(typ), category='register')
        elif kind == 0x100d:
            off, typ, reg = unpack('<IIH', body)
            row.update(name=pstr(body, 10), offset=off, register=reg, type=describe(typ), category='register-relative')
        elif kind == 0x207:
            parent, pend, size, off, seg = unpack('<4IH', body)
            row.update(name=pstr(body, 18), parent=parent, end=pend, size=size,
                       offset=off, segment=seg, category='block')
        records.append(row)
        pos = end
    byoff = {r['record_offset']: r for r in records}
    scopes = [r for r in records if r.get('category') in ('procedure', 'block')]
    for scope in scopes:
        if scope['end'] <= scope['record_offset'] or byoff.get(scope['end'], {}).get('kind') != '0x6':
            raise ValueError('scope endpoint is not a later S_END')
        if scope['parent']:
            parent = byoff.get(scope['parent'], {})
            if parent.get('category') not in ('procedure', 'block') or not parent['record_offset'] < scope['record_offset'] < scope['end'] < parent['end']:
                raise ValueError('invalid parent scope interval')
    procs = [r for r in records if r.get('category') == 'procedure']
    result = []
    for proc in procs:
        if proc['name'] in names:
            children = []
            for r in records:
                if proc['record_offset'] < r['record_offset'] <= proc['end']:
                    owners = [hex(s['record_offset']) for s in scopes if s['record_offset'] < r['record_offset'] < s['end']]
                    children.append(dict(r, lexical_scope_offsets=owners))
            result.append(dict(procedure=proc, children=children))
    return result, [p['name'] for p in procs]


def inspect_streams(streams, required=REQUIRED):
    if len(streams) < 4:
        raise ValueError('missing TPI/DBI streams')
    types, describe, header = parse_types(streams[2])
    dbi, at, modules = streams[3], 64, []
    modsize = unpack('<I', dbi, 24)[0]
    if at+modsize > len(dbi):
        raise ValueError('truncated DBI modules')
    while at < 64+modsize:
        sn, = unpack('<H', dbi, at+34)
        symsize, = unpack('<I', dbi, at+36)
        end = dbi.index(b'\0', at+64, 64+modsize)
        name = dbi[at+64:end].decode('latin1')
        end2 = dbi.index(b'\0', end+1, 64+modsize)
        modoff, at = at, (end2+4) & ~3
        key = name.upper().replace('\\', '/').split('/')[-1]
        if key not in required:
            continue
        if sn >= len(streams) or symsize > len(streams[sn]):
            raise ValueError('module symbol extent outside stream')
        buf = streams[sn][:symsize]
        result, roster = symbol_records(buf, describe, required[key] | OPTIONAL)
        if any(roster.count(n) != 1 for n in required[key]):
            raise ValueError('missing or duplicate required procedure')
        modules.append(dict(name=name, key=key, dbi_offset=modoff, stream=sn, symbol_bytes=symsize,
                            symbol_sha256=hashlib.sha256(buf).hexdigest(), procedure_count=len(roster),
                            procedure_names=roster, records=result))
    if sorted(m['key'] for m in modules) != sorted(required):
        raise ValueError('missing or duplicate required module')
    structs = []
    for index, raw in types.items():
        if raw['kind'] != '0x1005':
            continue
        row = describe(index)
        if row['name'] not in ('s_audpart', 's_audbank', 's_audmode'):
            continue
        if not row['forward_reference']:
            field = types[int(row['fieldlist'], 16)]
            if field['kind'] != '0x1203':
                raise ValueError('invalid struct fieldlist')
            body, pos, fields = bytes.fromhex(field['payload']), 0, []
            while pos < len(body):
                if body[pos] >= 0xf0:
                    padding = body[pos] & 0xf
                    if not padding or pos+padding > len(body):
                        raise ValueError('invalid field padding')
                    pos += padding
                    continue
                kind, attr, typ, offset = unpack('<HHIH', body, pos)
                if kind != 0x1405 or offset >= 0x8000:
                    raise ValueError('unmodelled field kind/numeric')
                name = pstr(body, pos+10)
                fields.append(dict(name=name, offset=offset, attributes=attr, type=describe(typ)))
                pos += 11+len(name)
            if len(fields) != row['member_count']:
                raise ValueError('field count differs')
            row.update(fieldlist_record=field, fields=fields)
        structs.append(row)
    return dict(status='RECOVERED', **header, modules=modules, structs=structs,
                scope='Xbox CodeView records only; no GC identity or absent-source-local assertion',
                reference='https://github.com/microsoft/microsoft-pdb/blob/master/include/cvinfo.h')


def inspect_pdb(path):
    path = Path(path)
    data = path.read_bytes()
    try:
        result = inspect_streams(pdb_streams(data))
    except (struct.error, IndexError, KeyError) as error:
        raise ValueError('malformed or unsupported PDB: '+str(error)) from error
    if path.read_bytes() != data:
        raise ValueError('PDB changed during reading')
    return dict(result, pdb_name=path.name, pdb_sha256=hashlib.sha256(data).hexdigest(), pdb_size=len(data))


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('pdb', nargs='?', type=Path, default=ROOT/'research/xbox_symbols/shell3D.pdb')
    ap.add_argument('--out', type=Path, default=ROOT/'build/r75_pdb_signatures.json')
    args = ap.parse_args(argv)
    out = args.out.resolve()
    if not out.is_relative_to((ROOT/'build').resolve()) or not out.name.startswith('r75_pdb_') or out.suffix != '.json':
        ap.error('output must be build/r75_pdb_*.json')
    result = inspect_pdb(args.pdb)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
    print(result['status'], out)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
