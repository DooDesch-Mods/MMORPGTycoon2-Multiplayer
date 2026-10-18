"""Reject compile results when their production inputs changed during the build."""
import hashlib
import os


def source_paths(root, framework_sdk=None):
    paths = [root / 'CMakeLists.txt']
    paths.extend((root / 'cmake').rglob('*.cmake'))
    for directory in ('src', 'include'):
        paths.extend(path for path in (root / directory).rglob('*')
                     if path.is_file() and path.suffix in ('.cpp', '.c', '.h', '.hpp', '.inc', '.asm'))
    result = {str(path.relative_to(root)): path for path in paths if path.is_file()}
    if framework_sdk is not None:
        for relative in ('include/mt2fw/plugin.h', 'adapters/mt2/sdk/native_services.h',
                         'adapters/mt2/sdk/native_routes.h', 'adapters/mt2/sdk/native_ownership.h'):
            result['framework-sdk/' + relative] = framework_sdk / relative
    return result


def inputs(root, framework_sdk=None):
    return {name: hashlib.sha256(path.read_bytes()).hexdigest()
            for name, path in source_paths(root, framework_sdk).items()}


def require_unchanged(root, before, framework_sdk=None):
    after = inputs(root, framework_sdk)
    changed = sorted(name for name in before.keys() | after.keys() if before.get(name) != after.get(name))
    if changed:
        # Compiler outputs can be newer than a mid-build edit. Force the next build to reread those inputs.
        paths = source_paths(root, framework_sdk)
        for name in changed:
            path = paths.get(name, root / name)
            if path.exists():
                os.utime(path, None)
        raise RuntimeError('build inputs changed during compilation; candidate invalid: ' + ', '.join(changed))
    return after
