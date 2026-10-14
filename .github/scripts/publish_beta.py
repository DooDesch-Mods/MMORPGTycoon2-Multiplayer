"""Publish exact beta assets without overwriting a different existing asset."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import urllib.error
import urllib.parse
import urllib.request
from build_beta import validate

REPOSITORY = 'DooDesch/MMORPGTycoon2-Multiplayer'


def api(method, url, payload=None, content_type='application/json', missing=False):
    data = json.dumps(payload).encode() if isinstance(payload, dict) else payload
    request = urllib.request.Request(url, data=data, method=method, headers={
        'Authorization': 'Bearer ' + os.environ['GH_TOKEN'], 'Accept': 'application/vnd.github+json',
        'Content-Type': content_type, 'X-GitHub-Api-Version': '2022-11-28', 'User-Agent': 'mt2mp-beta-release',
    })
    try:
        with urllib.request.urlopen(request, timeout=120) as response:
            return json.load(response)
    except urllib.error.HTTPError as error:
        if missing and error.code == 404:
            return None
        detail = error.read(4096).decode('utf8', errors='replace')
        raise RuntimeError(f'GitHub {method} failed HTTP {error.code}: {detail}') from error


def publish(tag, commit, directory):
    if not re.fullmatch(r'v\d+\.\d+\.\d+-beta\.[1-9]\d*', tag) or not re.fullmatch(r'[0-9a-f]{40}', commit):
        raise ValueError('invalid beta tag or source commit')
    base = 'https://api.github.com/repos/' + REPOSITORY
    if api('GET', base).get('private') is not True:
        raise ValueError('beta publishing requires the private repository')
    if api('GET', base + '/commits/' + tag)['sha'] != commit:
        raise ValueError('remote tag differs from build commit')
    archive = directory / ('MMORPGTycoon2-Multiplayer-' + tag + '-windows-x64.zip')
    checksum = archive.with_suffix('.zip.sha256')
    expected = hashlib.sha256(archive.read_bytes()).hexdigest() + '  ' + archive.name + '\n'
    if checksum.read_text(encoding='ascii') != expected:
        raise ValueError('release ZIP checksum differs')
    validate(archive, tag + '@' + commit)
    release = api('GET', base + '/releases/tags/' + tag, missing=True)
    if release is None:
        notes = (Path(__file__).resolve().parents[2] / 'docs/BETA_RELEASE_NOTES.md').read_text(encoding='utf8')
        release = api('POST', base + '/releases', {'tag_name': tag, 'target_commitish': commit,
                      'name': tag, 'body': notes, 'draft': True, 'prerelease': True, 'make_latest': 'false'})
    if release['tag_name'] != tag or release['target_commitish'] != commit or release['prerelease'] is not True:
        raise ValueError('existing release identity or prerelease flag differs')
    assets_url = base + '/releases/' + str(release['id']) + '/assets'
    assets = api('GET', assets_url)
    expected_names = {archive.name, checksum.name}
    if len(assets) != len({asset['name'] for asset in assets}) or {asset['name'] for asset in assets} - expected_names:
        raise ValueError('unexpected or duplicate existing beta assets')
    for path in [archive, checksum]:
        data = path.read_bytes()
        digest = 'sha256:' + hashlib.sha256(data).hexdigest()
        matches = [asset for asset in assets if asset['name'] == path.name]
        if matches:
            asset = matches[0]
            if asset['size'] != len(data) or asset.get('digest') != digest or asset.get('state') != 'uploaded':
                raise ValueError('existing asset differs; preserved without overwrite: ' + path.name)
            print('info: identical asset retained: ' + path.name)
            continue
        if release['draft'] is not True:
            raise ValueError('published release is incomplete; existing release preserved')
        url = release['upload_url'].split('{', 1)[0] + '?name=' + urllib.parse.quote(path.name)
        if not url.startswith('https://uploads.github.com/repos/' + REPOSITORY + '/releases/'):
            raise ValueError('unexpected release upload endpoint')
        uploaded = api('POST', url, data, 'application/zip' if path == archive else 'text/plain')
        if uploaded.get('size') != len(data) or uploaded.get('digest') != digest or uploaded.get('state') != 'uploaded':
            raise ValueError('uploaded asset acknowledgement differs: ' + path.name)
    release = api('GET', base + '/releases/' + str(release['id']))
    if {asset['name'] for asset in release['assets']} != expected_names:
        raise ValueError('final release asset membership differs')
    if release['draft'] is True:
        release = api('PATCH', base + '/releases/' + str(release['id']),
                      {'draft': False, 'prerelease': True, 'make_latest': 'false'})
    if release['draft'] is not False or release['prerelease'] is not True:
        raise ValueError('prerelease publication acknowledgement differs')
    print('info: beta published: ' + release['html_url'])


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tag', required=True)
    parser.add_argument('--commit', required=True)
    parser.add_argument('--directory', required=True, type=Path)
    args = parser.parse_args()
    publish(args.tag, args.commit, args.directory)
