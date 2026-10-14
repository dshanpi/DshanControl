"""Board-independent application payload assembly; system files are excluded."""
import shutil
from check_hmi_resources import verify as verify_hmi_resources


def payload(source, destination, hmi):
    verify_hmi_resources(source)
    mapping = [('components/core/src/omnigate_core', 'lib/core/omnigate_core'),
               ('components/web/app.py', 'lib/web/app.py'),
               ('components/web/hmi_api.py', 'lib/web/hmi_api.py'),
               ('components/web/assets', 'share/web'),
               ('components/hmi/src/fonts/OFL.txt', 'share/licenses/DshanSans-OFL.txt'),
               ('components/hmi/src/fonts/manifest.json', 'share/licenses/DshanSans-manifest.json'),
               ('components/supervisor/supervisor.py', 'lib/supervisor/supervisor.py')]
    for src, dst in mapping:
        target = destination / dst
        target.parent.mkdir(parents=True, exist_ok=True)
        if (source / src).is_dir():
            shutil.copytree(str(source / src), str(target), ignore=shutil.ignore_patterns('__pycache__', '*.pyc'))
        else:
            shutil.copy2(str(source / src), str(target))
    (destination / 'bin').mkdir(exist_ok=True)
    shutil.copy2(str(hmi), str(destination / 'bin/omnigate-hmi'))
    (destination / 'bin/omnigate-hmi').chmod(0o755)
