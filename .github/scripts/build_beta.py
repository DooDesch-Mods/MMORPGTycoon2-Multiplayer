"""Package the tagged build and check the tester ZIP before it can be published."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import zipfile

NOTICES = {
    'LICENSE.md', 'THIRD_PARTY_NOTICES.md', 'licenses/MinHook.txt',
    'licenses/DearImGui.txt', 'licenses/nlohmann-json.txt', 'licenses/SDL3.txt',
    'licenses/Python.txt', 'licenses/PyInstaller.txt',
}
PAYLOAD = {
    'MT2_Multiplayer.dll', 'steam_api64.dll', 'mt2mp_sidecar.exe', 'MT2MP-Start.exe',
    'README.txt', 'mt2mp_ui/entry.win', 'mt2mp_ui/lobby.win', 'mt2mp_ui/browser.win',
    'mt2mp_ui/multiplayer.png', 'mt2mp_ui/materials/mt2mp_desktop_multiplayer.mat',
}


def digest(data):
    return hashlib.sha256(data).hexdigest()


def validate(archive, candidate):
    with zipfile.ZipFile(archive) as zipped:
        names = zipped.namelist()
        required = NOTICES | PAYLOAD | {'package.json', '_internal/python313.dll', '_internal/base_library.zip'}
        missing = required - set(names)
        if missing:
            raise ValueError('beta package missing: ' + ', '.join(sorted(missing)))
        if len(names) != len(set(names)):
            raise ValueError('duplicate ZIP entry')
        for name in names:
            path = Path(name)
            if (name.startswith(('/', '\\')) or '\\' in name or ':' in name or '..' in path.parts
                    or name not in NOTICES | PAYLOAD | {'package.json'} and not name.startswith('_internal/')):
                raise ValueError('unexpected beta payload: ' + name)
            if path.suffix.lower() in {'.vrz', '.vrr', '.vrs', '.log', '.pdb'} or path.name.casefold() in {'mt2.exe', 'steam_api64_original.dll', '.env'}:
                raise ValueError('private/game payload rejected: ' + name)
            if not zipped.read(name):
                raise ValueError('empty beta payload: ' + name)
        manifest = json.loads(zipped.read('package.json'))
        if manifest['candidate'] != candidate:
            raise ValueError('package candidate differs from tag and source commit')
        if manifest['version'] != candidate.split('@', 1)[0][1:]:
            raise ValueError('package version differs from beta tag')
        if manifest.get('source_commit') != candidate.split('@', 1)[1]:
            raise ValueError('package source commit differs')
        if manifest.get('manual_test_package') is not True or manifest.get('ready_for_manual_testing') is not True:
            raise ValueError('portable tester package not confirmed')
        if set(manifest['files']) != set(names) - {'package.json'}:
            raise ValueError('package manifest membership differs')
        if manifest.get('online_acceptance_passed') is not False:
            raise ValueError('CI cannot claim native online acceptance')
        for name, record in manifest['files'].items():
            data = zipped.read(name)
            if record['size'] != len(data) or record['sha256'] != digest(data):
                raise ValueError('package content differs: ' + name)
        return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tag', required=True)
    parser.add_argument('--commit', required=True)
    parser.add_argument('--build-only', action='store_true')
    args = parser.parse_args()
    match = re.fullmatch(r'v(\d+\.\d+\.\d+)-beta\.([1-9]\d*)', args.tag)
    if not match or not re.fullmatch(r'[0-9a-f]{40}', args.commit):
        raise ValueError('expected vX.Y.Z-beta.N and full source commit')
    root = Path(__file__).resolve().parents[2]
    project = (root / 'CMakeLists.txt').read_text(encoding='utf8')
    if not re.search(r'project\(MT2_Multiplayer VERSION ' + re.escape(match[1]) + r'\b', project):
        raise ValueError('tag version differs from CMake project version')
    if args.build_only:
        subprocess.run(['cmake', '-S', str(root), '-B', str(root / '.ci-build/mod'), '-A', 'x64',
                        '-DMT2MP_BUILD_TESTS=OFF', '-DCMAKE_POLICY_VERSION_MINIMUM=3.5',
                        '-DMT2MP_BUILD_VERSION=' + args.tag[1:]], check=True, cwd=root)
        subprocess.run(['cmake', '--build', str(root / '.ci-build/mod'), '--config', 'Release', '--target',
                        'MT2_Multiplayer', 'steam_api64', 'mt2mp_sidecar', '--parallel', '2'], check=True, cwd=root)
        return
    output = root / 'artifacts/beta'
    output.mkdir(parents=True, exist_ok=True)
    candidate = args.tag + '@' + args.commit
    build = root / '.ci-build/mod/bin/Release'
    pin = output / 'freeze.json'
    pin.write_text(json.dumps({'candidate': candidate, 'source_commit': args.commit,
                              'dll_sha256': digest((build / 'MT2_Multiplayer.dll').read_bytes())}) + '\n')
    archive = output / ('MMORPGTycoon2-Multiplayer-' + args.tag + '-windows-x64.zip')
    subprocess.run(['python', str(root / 'tools/package.py'), '--freeze', str(pin), '--build', str(build),
                    '--with-sidecar', '--version', args.tag[1:], '--launcher', str(root / '.ci-build/launcher/MT2MP-Start/MT2MP-Start.exe'),
                    '--output', str(archive)], check=True, cwd=root)
    validate(archive, candidate)
    checksum = archive.with_suffix('.zip.sha256')
    checksum.write_text(digest(archive.read_bytes()) + '  ' + archive.name + '\n', encoding='ascii')
    print(json.dumps({'status': 'beta-package-checked', 'candidate': candidate, 'archive': str(archive)}))


if __name__ == '__main__':
    main()
