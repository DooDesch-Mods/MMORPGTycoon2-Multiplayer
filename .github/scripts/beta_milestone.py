"""Select an explicit beta milestone and bind its tag to the built source."""
import argparse
import json
from pathlib import Path
import re

RECORD = Path('.github/beta-milestone.json')
VERSION = re.compile(r'\d+\.\d+\.\d+-beta\.[1-9]\d*')


def exact_object(pairs):
    result = {}
    for name, value in pairs:
        if name in result:
            raise ValueError('duplicate milestone field: ' + name)
        result[name] = value
    return result


def read_record(path):
    if path.stat().st_size > 8192:
        raise ValueError('milestone record exceeds 8192 bytes')
    record = json.loads(path.read_text(encoding='utf8'), object_pairs_hook=exact_object)
    if not isinstance(record, dict) or set(record) != {'schema', 'version', 'summary', 'native_evidence'}:
        raise ValueError('expected exact milestone schema/version/summary/native_evidence fields')
    if type(record['schema']) is not int or record['schema'] != 1:
        raise ValueError('unsupported milestone schema')
    if not isinstance(record['version'], str) or not VERSION.fullmatch(record['version']):
        raise ValueError('milestone version must be X.Y.Z-beta.N')
    for name, maximum in [('summary', 300), ('native_evidence', 1024)]:
        value = record[name]
        if not isinstance(value, str) or not value.strip() or len(value) > maximum or any(ord(c) < 32 for c in value):
            raise ValueError('invalid milestone ' + name)
    return record


def resolve(root, ref):
    if ref == 'refs/heads/main':
        return 'v' + read_record(root / RECORD)['version'], True
    if ref.startswith('refs/tags/v') and VERSION.fullmatch(ref[len('refs/tags/v'):]):
        return ref[len('refs/tags/'):], False
    raise ValueError('beta source must be main milestone or explicit beta tag')


def ensure_tag(tag, commit, call=None):
    from publish_beta import api, REPOSITORY
    if call is None:
        call = api
    if not tag.startswith('v') or not VERSION.fullmatch(tag[1:]) or not re.fullmatch(r'[0-9a-f]{40}', commit):
        raise ValueError('invalid milestone tag or source commit')
    base = 'https://api.github.com/repos/' + REPOSITORY
    if call('GET', base).get('full_name') != REPOSITORY:
        raise ValueError('milestone repository identity differs')
    reference = call('GET', base + '/git/ref/tags/' + tag, missing=True)
    if reference is None:
        if call('GET', base + '/git/commits/' + commit).get('sha') != commit:
            raise ValueError('built source commit not confirmed remotely')
        reference = call('POST', base + '/git/refs', {'ref': 'refs/tags/' + tag, 'sha': commit})
        target = reference.get('object', {})
        if reference.get('ref') != 'refs/tags/' + tag or target.get('type') != 'commit' or target.get('sha') != commit:
            raise ValueError('milestone tag creation acknowledgement differs; inspect before retry')
    if reference.get('ref') != 'refs/tags/' + tag or call('GET', base + '/commits/' + tag).get('sha') != commit:
        raise ValueError('existing milestone tag differs from built source; preserved without overwrite')
    print('info: milestone tag confirmed: ' + tag + '@' + commit)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    select = commands.add_parser('resolve')
    select.add_argument('--ref', required=True)
    select.add_argument('--output', type=Path, required=True)
    tag = commands.add_parser('ensure-tag')
    tag.add_argument('--tag', required=True)
    tag.add_argument('--commit', required=True)
    args = parser.parse_args()
    if args.command == 'resolve':
        selected, milestone = resolve(Path(__file__).resolve().parents[2], args.ref)
        with args.output.open('a', encoding='utf8') as output:
            output.write('tag=' + selected + '\nmilestone=' + str(milestone).lower() + '\n')
        print('info: selected beta: ' + selected)
    else:
        ensure_tag(args.tag, args.commit)


if __name__ == '__main__':
    main()
