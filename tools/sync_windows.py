#!/usr/bin/env python3
"""Stage this project's source on Windows. Never overwrites runtime profiles.

Use SSH keys/agent, or SSHPASS with --password-env (sshpass -e). Credentials
and another project's sync script are never copied or executed.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import subprocess
import tempfile
import uuid
import zipfile

DEFAULT_DESTINATION = 'D:/Projects/Mini-Window-Browser'
EXCLUDED_DIRS = {'.git', '.dart_tool', '.vs', '.superpowers', 'build', 'work',
                 'node_modules', '__pycache__', 'webview-profile', '.ssh', '.aws',
                 'debug', 'release', 'x64', 'x86', 'obj', 'bin', 'artifacts',
                 'ephemeral', 'packages', 'data', 'logs', 'generated files', '.codex', '.agents'}


def allowed_path(name):
    parts = PurePosixPath(name).parts
    if not parts or '..' in parts or PurePosixPath(name).is_absolute():
        return False
    if any(p.lower() in EXCLUDED_DIRS or p.lower().endswith('.webview2') for p in parts[:-1]):
        return False
    if parts[0] not in {'src','assets','tests','winui','tools','docs'} and len(parts)!=1:
        return False
    leaf = parts[-1].lower()
    return not (leaf in {'.ds_store', '.env', 'credentials.json', 'id_rsa', 'id_ed25519'}
                or leaf.startswith(('._', '.env.', 'config.ini', 'config.winui.ini', 'credentials', 'secrets')) or leaf in {'cookies','history','login data','web data','preferences','local state'}
                or leaf.endswith(('.pem', '.key', '.pfx', '.p12', '.zip', '.exe', '.dll', '.pdb')))


def collect_files(root):
    root = Path(root).resolve()
    result = subprocess.run(['git', '-C', str(root), 'ls-files', '-z', '--cached',
                             '--others', '--exclude-standard'], check=True, capture_output=True)
    files = []
    for raw in set(result.stdout.split(b'\0')):
        if not raw:
            continue
        name = os.fsdecode(raw)
        path = root / name
        # Never send a symlink or a file through a symlinked parent.
        if allowed_path(name) and path.is_file():
            if any((root / Path(*Path(name).parts[:i])).is_symlink()
                   for i in range(1, len(Path(name).parts) + 1)):
                continue
            files.append(name)
    return sorted(files)


def create_archive(root, destination):
    root = Path(root)
    digest = hashlib.sha256()
    with zipfile.ZipFile(destination, 'w', zipfile.ZIP_DEFLATED) as archive:
        for name in collect_files(root):
            data = (root / name).read_bytes()
            digest.update(name.encode('utf-8') + b'\0')
            digest.update(len(data).to_bytes(8, 'big'))
            digest.update(data)
            archive.writestr(name, data)
    return digest.hexdigest()


def validate_destination(value):
    normalized = value.replace('\\', '/').rstrip('/')
    # Deliberately narrow: the only writable remote project root for this tool.
    if normalized.lower() != DEFAULT_DESTINATION.lower():
        raise ValueError(f'Destination must be {DEFAULT_DESTINATION}')
    return DEFAULT_DESTINATION


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--host', help='SSH user@hostname; required for actual sync')
    parser.add_argument('--destination', default=DEFAULT_DESTINATION)
    parser.add_argument('--dry-run', action='store_true')
    parser.add_argument('--password-env', action='store_true', help='Use SSHPASS via sshpass -e')
    args = parser.parse_args(argv)
    destination = validate_destination(args.destination)
    files = collect_files(args.source)
    if args.dry_run:
        print(json.dumps({'destination': destination, 'files': files}, ensure_ascii=False, indent=2))
        return 0
    if not args.host or not re.fullmatch(r'[A-Za-z0-9_.-]+@[A-Za-z0-9_.-]+', args.host):
        parser.error('--host must be user@hostname')
    prefix = ['sshpass', '-e'] if args.password_env else []
    if args.password_env and not os.environ.get('SSHPASS'):
        parser.error('SSHPASS is not set')
    options = ['-o', 'StrictHostKeyChecking=yes', '-o', 'ConnectTimeout=15']

    def remote(script):
        # The office SSH shell is already PowerShell. Upload a task-owned script.
        with tempfile.TemporaryDirectory(prefix='mini-window-command-') as command_temp:
            command_file=Path(command_temp)/'sync-command.ps1'
            command_file.write_text(script,encoding='utf-8-sig')
            remote_script=f'{destination}/sync-{uuid.uuid4().hex}.ps1'
            subprocess.run(prefix+['scp',*options,str(command_file),f'{args.host}:{remote_script}'],check=True)
            invocation=f"try {{ & '{remote_script}' }} finally {{ Remove-Item -LiteralPath '{remote_script}' -ErrorAction SilentlyContinue }}"
            subprocess.run(prefix+['ssh',*options,args.host,invocation],check=True)

    # Fresh staging source per sync makes deleted files disappear without deleting
    # anything from a prior build or the user's profile/configuration.
    identifier = uuid.uuid4().hex
    stage = f'{destination}/sources/{identifier}'
    remote_zip = f'{destination}/incoming/{identifier}.zip'
    remote("$ErrorActionPreference='Stop'; New-Item -ItemType Directory -Force -Path '" +
           destination + "/incoming' | Out-Null")
    with tempfile.TemporaryDirectory(prefix='mini-window-sync-') as temp:
        local_zip = Path(temp) / 'source.zip'
        digest = create_archive(args.source, local_zip)
        subprocess.run(prefix + ['scp', *options, str(local_zip),
                                 f'{args.host}:{remote_zip}'], check=True)
        manifest=Path(temp)/'source-manifest.json'
        manifest.write_text(json.dumps({'sha256':digest,'files':files},ensure_ascii=False),encoding='utf-8')
        subprocess.run(prefix+['scp',*options,str(manifest),f'{args.host}:{destination}/incoming/{identifier}.json'],check=True)
        remote("$ErrorActionPreference='Stop'; Expand-Archive -LiteralPath '" + remote_zip +
               "' -DestinationPath '" + stage + "'; Remove-Item -LiteralPath '" + remote_zip +
               "'; Set-Content -LiteralPath '" + destination + "/current-source.txt' -Value '" +
               stage + "' -Encoding UTF8; Move-Item -LiteralPath '"+destination+"/incoming/"+identifier+".json' -Destination '"+stage+"/source-manifest.json'")
    print(json.dumps({'sourceDirectory': stage, 'sha256': digest, 'fileCount': len(files)}))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
