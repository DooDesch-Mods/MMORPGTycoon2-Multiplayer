"""Reject compile results when their production inputs changed during the build."""
import hashlib
import os


def inputs(root):
    paths = [root / 'CMakeLists.txt']
    for directory in ('src', 'include'):
        paths.extend(path for path in (root / directory).rglob('*')
                     if path.is_file() and path.suffix in ('.cpp', '.c', '.h', '.hpp', '.inc'))
    return {str(path.relative_to(root)): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in paths if path.exists()}


def require_unchanged(root, before):
    after = inputs(root)
    changed = sorted(name for name in before.keys() | after.keys() if before.get(name) != after.get(name))
    if changed:
        # Compiler outputs can be newer than a mid-build edit. Force the next build to reread those inputs.
        for name in changed:
            path = root / name
            if path.exists():
                os.utime(path, None)
        raise RuntimeError('build inputs changed during compilation; candidate invalid: ' + ', '.join(changed))
    return after
