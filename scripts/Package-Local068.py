from pathlib import Path, PurePosixPath
from datetime import datetime
import hashlib
import json
import re
import subprocess
import zipfile
import shutil
import os

root = Path(__file__).resolve().parents[2]
source = root / 'source'
container = root / 'release/v0.6.8-local'
runtime = container / 'Magpie-Experimental-x64'
target = container / 'Magpie-Experimental-x64.zip'
pending = target.with_suffix('.zip.building')
assert not pending.exists(), f'Pending archive already exists: {pending}'
if target.exists():
    backup = root / 'backups' / ('068-release-zip-' + datetime.now().strftime('%Y%m%d-%H%M%S'))
    backup.mkdir(parents=True, exist_ok=False)
    for old in [target, target.with_suffix('.zip.sha256'), container / 'release-zip-verification.json']:
        if old.exists():
            copied = backup / old.name
            shutil.copy2(old, copied)
            assert hashlib.sha256(old.read_bytes()).digest() == hashlib.sha256(copied.read_bytes()).digest()

deployed = json.loads((runtime / 'build-manifest.json').read_text(encoding='utf-8-sig'))
state = json.loads((container / 'deployment-state.json').read_text(encoding='utf-8-sig'))
commit = subprocess.check_output(['git', '-C', str(source), 'rev-parse', 'HEAD'], text=True).strip()
assert state['status'] == 'deployed' and state['commit'] == deployed['commit'] == commit
assert not deployed['sourceDirty']
stamp = subprocess.check_output(['git', '-C', str(source), 'show', '-s', '--format=%cI', commit], text=True).strip()
date = datetime.fromisoformat(stamp)
from datetime import timezone
date = date.astimezone(timezone.utc)
timestamp = date.timetuple()[:6]
payload = {}
for item in deployed['files']:
    name = item['path']
    rel = PurePosixPath(name)
    assert not rel.is_absolute() and '..' not in rel.parts
    path = runtime / name
    assert path.stat().st_size == item['bytes'], name
    assert hashlib.sha256(path.read_bytes()).hexdigest().upper() == item['sha256'].upper(), name
    if path.suffix.lower() in {'.pdb', '.map', '.lib', '.exp'} or path.name == 'Magpie.next.exe':
        continue
    assert not {'config', 'logs', 'cache', 'PortableAppData'}.intersection(rel.parts), name
    payload[name] = path

readme = (source / 'docs/README-EXPERIMENTAL-RELEASE.txt').read_text(encoding='utf-8-sig')
readme = re.sub(r'^Magpie Experimental v[^ ]+ x64', 'Magpie Experimental v0.6.8-local x64', readme)
payload['README-Experimental.txt'] = readme.encode('utf-8')
payload['THIRD-PARTY-NOTICES.md'] = source / 'docs/THIRD_PARTY_AND_REDISTRIBUTION.md'
payload['FRAME_SYNC_GUIDE.md'] = source / 'docs/FRAME_SYNC_GUIDE.md'
payload['RELEASE-NOTES.md'] = f'''# Magpie Experimental 0.6.8-local

Source commit: `{commit}`

- One XeSS_FrameGeneration effect, defaulting to 2× / AMD optical flow / Quality.
- Fix 3×/4× slow startup by keeping fixed SDK-domain burst deadlines instead of appending a full interval to expired slots; improve startup period estimation.
- Fix non-Intel 3×/4× resetting interpolation history every frame: capture session IDs remain constant during normal capture. Use session changes for resets and frame IDs for skipped submissions.
- Legacy x2/MFG effects migrate to the unified effect at 2× while preserving optical-flow settings. Existing unified settings remain unchanged.
- Non-Intel 3×/4× automatically enables verified runtime compatibility and frame pacing; no extra user toggle. NVIDIA optical flow remains available at 2×.
- Compatibility uses the original bundled DLL with verified in-memory changes, restoring it after context destruction. Unknown runtime builds fail explicitly.
- The four DLSSNR anti-flicker routes and the profile focus option in Advanced remain available.

See XESSFG.md for validation details and XESSFG-COMPATIBILITY-NOTICE.md for source attribution. Timing and SDK regression tests passed on RTX 5070 Ti. With RTSS injected from the first frame, fullscreen WGC + AMD Quality 4× reached its first 120 submissions in 2.941 seconds (old controls: 10.176/11.917 seconds); the GDI fixture runs at approximately 40 accepted frames per second. An independent RTSS/NVAPI initialization crash still reproduces during repeated 2×/3×/4× switching. This build fixes the demonstrated startup pacing feedback, not all RTSS compatibility issues. Provider submission statistics are not display events. Real-game, HDR, VRR, long-duration and image-quality validation remains open.

This archive contains the verified deployed runtime, bundled effects, documentation and licenses. Local configurations, caches, logs, debug symbols and linker artifacts are excluded.
'''.encode('utf-8')

def content(value):
    return value.read_bytes() if isinstance(value, Path) else value

records = []
for name, value in sorted(payload.items()):
    data = content(value)
    records.append({'path': name, 'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest().upper()})
manifest = {
    'schemaVersion': 1, 'package': 'Magpie-Experimental-x64', 'version': deployed['version'],
    'commit': commit, 'sourceDirty': False, 'sourceDateUtc': date.isoformat(),
    'configuration': deployed['configuration'], 'platform': deployed['platform'], 'files': records,
}
payload['build-manifest.json'] = json.dumps(manifest, ensure_ascii=False, indent=2).encode('utf-8')
print(f'Packaging {len(payload)} files from deployed commit {commit[:12]}', flush=True)
with zipfile.ZipFile(pending, 'x', compression=zipfile.ZIP_DEFLATED, compresslevel=6, allowZip64=True) as archive:
    for name, value in sorted(payload.items()):
        info = zipfile.ZipInfo('Magpie-Experimental-x64/' + name, timestamp)
        info.compress_type = zipfile.ZIP_DEFLATED
        info.external_attr = 0o100644 << 16
        archive.writestr(info, content(value))
print('Archive created; checking every archived file against the package manifest.', flush=True)
with zipfile.ZipFile(pending) as archive:
    assert archive.testzip() is None
    assert len(archive.namelist()) == len(payload)
    for item in records:
        data = archive.read('Magpie-Experimental-x64/' + item['path'])
        assert len(data) == item['bytes']
        assert hashlib.sha256(data).hexdigest().upper() == item['sha256']
    assert json.loads(archive.read('Magpie-Experimental-x64/build-manifest.json')) == manifest
os.replace(pending, target)
digest = hashlib.sha256(target.read_bytes()).hexdigest().upper()
target.with_suffix('.zip.sha256').write_text(f'{digest}  {target.name}\n', encoding='ascii')
report = {'path': str(target), 'bytes': target.stat().st_size, 'sha256': digest, 'filesVerified': len(payload), 'commit': commit}
(container / 'release-zip-verification.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps(report), flush=True)
