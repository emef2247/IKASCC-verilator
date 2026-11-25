#!/usr/bin/env python3
"""Prepare the pinned, unmodified upstream RTL locally; never vendor it."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import urllib.request

ROOT=Path(__file__).resolve().parents[1]
MANIFEST=json.loads((ROOT/'docs/rtl_provenance.json').read_text())
REPO='https://github.com/ika-musume/IKASCC'


def digest(data):return hashlib.sha256(data).hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    source=parser.add_mutually_exclusive_group()
    source.add_argument('--from-dir',type=Path,help='upstream checkout root, containing LICENSE and src/')
    source.add_argument('--fetch-raw',metavar='URL',help='upstream repository URL (the recorded revision is used)')
    parser.add_argument('--force',action='store_true',help='replace differing local files with verified upstream bytes')
    parser.add_argument('--check',action='store_true',help='verify local RTL and license; never download')
    args=parser.parse_args()
    if args.check and (args.from_dir or args.fetch_raw or args.force):parser.error('--check cannot modify files')
    if args.fetch_raw:
        permitted=(REPO,REPO+'/',REPO+'/tree/main',REPO+'/blob/main',
            REPO+'/tree/'+MANIFEST['revision'],REPO+'/blob/'+MANIFEST['revision'],
            'https://raw.githubusercontent.com/ika-musume/IKASCC/'+MANIFEST['revision'])
        if args.fetch_raw.rstrip('/') not in permitted:parser.error('URL must name the recorded upstream repository/revision')
    expected=dict(MANIFEST['files']);expected['LICENSE']=MANIFEST['license_sha256']
    staged=[]
    for name,sha in expected.items():
        dest=ROOT/'rtl'/('IKASCC_LICENSE' if name=='LICENSE' else name)
        if dest.is_file() and digest(dest.read_bytes())==sha:continue
        if args.check:raise ValueError('Missing or modified RTL/license: '+str(dest))
        if dest.exists() and not args.force:raise ValueError('Preserving differing local file; use --force deliberately: '+str(dest))
        if args.from_dir:
            data=(args.from_dir/name).read_bytes()
        else:
            url='https://raw.githubusercontent.com/ika-musume/IKASCC/'+MANIFEST['revision']+'/'+name
            with urllib.request.urlopen(url,timeout=45) as response:data=response.read()
        if digest(data)!=sha:raise ValueError('Upstream bytes differ from pinned manifest: '+name)
        staged.append((dest,data))
    # Validate all downloads before replacing any destination.
    for dest,data in staged:
        dest.parent.mkdir(parents=True,exist_ok=True)
        temporary=dest.with_name(dest.name+'.download')
        temporary.write_bytes(data);temporary.replace(dest)
    print('IKASCC RTL verified:',MANIFEST['revision'],'('+str(len(expected)-1)+' source files + license)')
    return 0


if __name__=='__main__':
    try:sys.exit(main())
    except (OSError,ValueError) as error:
        print('RTL preparation failed:',error,file=sys.stderr);sys.exit(1)
