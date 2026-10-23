"""Fetches the third-party sources of the SDK into sdk/thirdparty.

sdk/thirdparty only carries the files this port changed (see THIRD_PARTY_NOTICES.md). Everything else comes from the
ReXGlue SDK release this port is based on and its submodules. This script clones that release with its submodules
into a temporary folder and copies every file that sdk/thirdparty does not have yet; files already there are kept.

Usage: python tools/fetch_thirdparty.py
Needs git.
"""
import os
import shutil
import stat
import subprocess
import sys
import tempfile

UPSTREAM = 'https://github.com/rexglue/rexglue-sdk.git'
COMMIT = 'c94f5ebdcb3c9d1a460ca48e04f9758448f8d518'  # v0.10.0


def run(*args, cwd=None):
    print('>', ' '.join(args))
    subprocess.run(args, cwd=cwd, check=True)


def remove_readonly(func, path, _):
    os.chmod(path, stat.S_IWRITE)
    func(path)


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    target = os.path.join(root, 'sdk', 'thirdparty')
    work = tempfile.mkdtemp(prefix='rexglue-sdk-')
    try:
        run('git', 'init', '-q', work)
        run('git', 'remote', 'add', 'origin', UPSTREAM, cwd=work)
        run('git', 'fetch', '-q', '--depth', '1', 'origin', COMMIT, cwd=work)
        run('git', 'checkout', '-q', 'FETCH_HEAD', cwd=work)
        run('git', 'submodule', 'update', '--init', '--recursive', cwd=work)
        source = os.path.join(work, 'thirdparty')
        # On Windows without symlink privileges, Git checks symlinks out as
        # regular files containing the link target. Flatten those files to
        # their target contents so CMake sees the source/header it expects.
        tree = subprocess.check_output(
            ['git', 'ls-tree', '-r', '-z', 'HEAD', 'thirdparty'], cwd=work
        )
        symlink_paths = set()
        for entry in tree.split(b'\0'):
            if not entry:
                continue
            metadata, path = entry.split(b'\t', 1)
            if metadata.split(b' ', 1)[0] == b'120000':
                relative_path = path.decode('utf-8')
                if relative_path.startswith('thirdparty/'):
                    relative_path = relative_path[len('thirdparty/'):]
                symlink_paths.add(os.path.normpath(relative_path))
        # Submodule contents have their own Git index (the SDK superproject
        # records only the gitlink), so collect their symlinks separately.
        for dirpath, dirnames, _ in os.walk(source):
            dirnames[:] = [directory for directory in dirnames if directory != '.git']
            git_marker = os.path.join(dirpath, '.git')
            if not (os.path.isdir(git_marker) or os.path.isfile(git_marker)):
                continue
            prefix = os.path.relpath(dirpath, source)
            index = subprocess.check_output(['git', 'ls-files', '--stage', '-z'], cwd=dirpath)
            for entry in index.split(b'\0'):
                if not entry:
                    continue
                metadata, path = entry.split(b'\t', 1)
                if metadata.split(b' ', 1)[0] == b'120000':
                    symlink_paths.add(
                        os.path.normpath(os.path.join(prefix, path.decode('utf-8')))
                    )
        copied = kept = 0
        for dirpath, dirnames, filenames in os.walk(source):
            dirnames[:] = [d for d in dirnames if d != '.git']
            for name in filenames:
                if name == '.git':
                    continue
                src = os.path.join(dirpath, name)
                if os.path.islink(src) and not os.path.exists(src):
                    continue  # dangling symlink (e.g. macOS-only MoltenVK tools on Linux)
                dst = os.path.join(target, os.path.relpath(src, source))
                relative_path = os.path.normpath(os.path.relpath(src, source))
                was_existing = os.path.exists(dst)
                resolved_target = None
                if relative_path in symlink_paths and not os.path.islink(src):
                    with open(src, encoding='utf-8') as link_file:
                        link_target = link_file.read().strip()
                    resolved_target = os.path.normpath(os.path.join(dirpath, link_target))
                    if not os.path.isfile(resolved_target):
                        raise FileNotFoundError(
                            f'Could not resolve vendored symlink {relative_path}: {link_target}'
                        )
                if os.path.exists(dst) and resolved_target is None:
                    kept += 1
                    continue
                os.makedirs(os.path.dirname(dst), exist_ok=True)
                if resolved_target is not None:
                    shutil.copy2(resolved_target, dst)
                    if was_existing:
                        kept += 1
                    else:
                        copied += 1
                else:
                    shutil.copy2(src, dst)
                    copied += 1
        print(f'{copied} files copied into sdk/thirdparty, {kept} files of this port kept')
    finally:
        if sys.version_info >= (3, 12):
            shutil.rmtree(work, onexc=remove_readonly)
        else:
            shutil.rmtree(work, onerror=remove_readonly)


if __name__ == '__main__':
    main()
