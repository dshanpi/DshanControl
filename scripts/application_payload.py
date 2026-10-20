"""Board-independent application payload assembly; system files are excluded."""
import shutil
from pathlib import Path
from check_hmi_resources import verify as verify_hmi_resources


COMPONENT_PATHS = {'desktop_service': 'bin/dshancontrol-desktop-service',
                   'media_worker': 'bin/dshancontrol-media-worker',
                   'imageformats': 'bin/imageformats/libdshanimageformats.so'}


def component_inputs(desktop_service=None, media_worker=None, imageformats=None):
    values = dict(desktop_service=desktop_service, media_worker=media_worker,
                  imageformats=imageformats)
    if any(value is not None for value in values.values()):
        if not all(value is not None for value in values.values()):
            raise ValueError('desktop service, media worker and image codecs must be supplied together')
        for name, value in values.items():
            path = Path(value)
            if path.is_symlink() or not path.is_file() or path.stat().st_size == 0:
                raise ValueError('component must be a nonempty regular artifact: ' + name)
            if name != 'imageformats' and not path.stat().st_mode & 0o111:
                raise ValueError('component must be executable: ' + name)
        return {COMPONENT_PATHS[name]: Path(path) for name, path in values.items()}
    return {}



def component_licenses(source, components):
    if not components:
        return {}
    directory = Path(source) / 'components/imageformats'
    names = ['LICENSE.GPL2', 'LICENSE.GPL3', 'LICENSE.LGPL3', 'SOURCE.json']
    files = {name: directory / 'third_party/qt-5.11.3' / name for name in names}
    files['NOTICE'] = directory / 'NOTICE'
    for name, path in files.items():
        if path.is_symlink() or not path.is_file() or path.stat().st_size == 0:
            raise ValueError('missing or invalid image codec license input: ' + name)
    return files


def payload(source, destination, hmi, desktop_service=None, media_worker=None, imageformats=None):
    components = component_inputs(desktop_service, media_worker, imageformats)
    licenses = component_licenses(source, components)
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
    for relative, artifact in components.items():
        target = destination / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(str(artifact), str(target))
        target.chmod(0o644 if relative.endswith('.so') else 0o755)
    if components:
        target = destination / 'share/licenses/qt-imageformats-5.11.3'
        target.mkdir(parents=True)
        for name, artifact in licenses.items():
            shutil.copyfile(str(artifact), str(target / name))
