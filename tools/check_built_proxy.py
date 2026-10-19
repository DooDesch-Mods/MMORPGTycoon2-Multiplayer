"""Compare PE export tables without executing either DLL."""
from pathlib import Path
import hashlib
import json
import re
import struct

ROOT=Path(__file__).resolve().parents[1]


def exports(path):
    data=path.read_bytes()
    pe=struct.unpack_from('<I',data,60)[0]
    if data[pe:pe+4]!=b'PE\0\0' or struct.unpack_from('<H',data,pe+4)[0]!=0x8664:
        raise ValueError(f'Expected x64 PE: {path}')
    count=struct.unpack_from('<H',data,pe+6)[0]
    optional=pe+24
    size=struct.unpack_from('<H',data,pe+20)[0]
    if struct.unpack_from('<H',data,optional)[0]!=0x20b:
        raise ValueError('Expected PE32+ optional header')
    sections=[]
    for index in range(count):
        offset=optional+size+40*index
        virtual,rva,raw_size,raw=struct.unpack_from('<IIII',data,offset+8)
        sections.append((rva,max(virtual,raw_size),raw,raw_size))
    def at(rva,length=1):
        for base,extent,raw,raw_size in sections:
            delta=rva-base
            if 0<=delta and delta+length<=raw_size and delta+length<=extent:
                if raw+delta+length>len(data): raise ValueError('PE RVA exceeds file')
                return raw+delta
        raise ValueError(f'PE RVA outside backed sections: {rva:#x}')
    directory,directory_size=struct.unpack_from('<II',data,optional+112)
    offset=at(directory,40)
    ordinal_base,function_count,name_count,function_table,name_table,ordinal_table=struct.unpack_from('<IIIIII',data,offset+16)
    if name_count>20000 or function_count>20000: raise ValueError('PE export table exceeds fixture bounds')
    result={}
    for index in range(name_count):
        name_rva=struct.unpack_from('<I',data,at(name_table+4*index,4))[0]
        start=at(name_rva)
        end=data.find(b'\0',start,start+512)
        if end<0: raise ValueError('PE export name lacks bounded terminator')
        name=data[start:end].decode('ascii')
        slot=struct.unpack_from('<H',data,at(ordinal_table+2*index,2))[0]
        if slot>=function_count: raise ValueError('PE export ordinal exceeds function table')
        target=struct.unpack_from('<I',data,at(function_table+4*slot,4))[0]
        if not target: raise ValueError(f'PE named export has null target: {name}')
        if name in result: raise ValueError(f'PE duplicate export: {name}')
        result[name]={'ordinal':ordinal_base+slot,'rva':target,'forwarded':directory<=target<directory+directory_size}
    return result,hashlib.sha256(data).hexdigest()


def main():
    original=Path('F:/Launcher/SteamLibrary/steamapps/common/MMORPG Tycoon 2/steam_api64_original.dll')
    built=ROOT/'build/bin/Release/steam_api64.dll'
    source=ROOT/'proxy/steam_stubs_init.cpp'
    required=set(re.findall(r'GetProcAddress\(original, "([^\"]+)"\)',source.read_text()))
    original_exports,original_sha=exports(original)
    built_exports,built_sha=exports(built)
    missing_original=sorted(required-original_exports.keys())
    missing_built=sorted(required-built_exports.keys())
    record={'passed':not missing_original and not missing_built,'required':len(required),
        'original_export_count':len(original_exports),'built_export_count':len(built_exports),
        'missing_original':missing_original,'missing_built':missing_built,
        'original_sha256':original_sha,'built_sha256':built_sha,'shutdown_export':built_exports.get('SteamAPI_Shutdown'),
        'scope':'PE export presence only; no DLL load or native shutdown runtime claim'}
    (ROOT/'artifacts/built_proxy_exports.json').write_text(json.dumps(record,indent=2),encoding='utf-8')
    print(json.dumps(record))
    raise SystemExit(0 if record['passed'] else 1)


if __name__=='__main__': main()
