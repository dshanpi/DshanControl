"""Board-independent application payload assembly; system files are excluded."""
import shutil
import json
from pathlib import Path
from check_hmi_resources import verify as verify_hmi_resources


COMPONENT_PATHS = {'desktop_service': 'bin/dshancontrol-desktop-service',
                   'media_worker': 'bin/dshancontrol-media-worker',
                   'imageformats': 'bin/imageformats/libdshanimageformats.so'}


def component_inputs(desktop_service=None, media_worker=None, imageformats=None, system_imageformats=False):
    values = dict(desktop_service=desktop_service, media_worker=media_worker,
                  imageformats=imageformats)
    if any(value is not None for value in values.values()):
        if not all(value is not None or (name == 'imageformats' and system_imageformats)
                   for name,value in values.items()):
            raise ValueError('desktop service, media worker and image codecs must be supplied together')
        for name, value in values.items():
            if value is None: continue
            path = Path(value)
            if path.is_symlink() or not path.is_file() or path.stat().st_size == 0:
                raise ValueError('component must be a nonempty regular artifact: ' + name)
            if name != 'imageformats' and not path.stat().st_mode & 0o111:
                raise ValueError('component must be executable: ' + name)
        return {COMPONENT_PATHS[name]: Path(path) for name, path in values.items() if path is not None}
    return {}



def component_licenses(source, components):
    if 'bin/imageformats/libdshanimageformats.so' not in components:
        return {}
    directory = Path(source) / 'components/imageformats'
    names = ['LICENSE.GPL2', 'LICENSE.GPL3', 'LICENSE.LGPL3', 'SOURCE.json']
    files = {name: directory / 'third_party/qt-5.11.3' / name for name in names}
    files['NOTICE'] = directory / 'NOTICE'
    for name, path in files.items():
        if path.is_symlink() or not path.is_file() or path.stat().st_size == 0:
            raise ValueError('missing or invalid image codec license input: ' + name)
    return files


def payload(source, destination, hmi, desktop_service=None, media_worker=None, imageformats=None, system_imageformats=False, native_api=None):
    components = component_inputs(desktop_service, media_worker, imageformats, system_imageformats)
    licenses = component_licenses(source, components)
    verify_hmi_resources(source)
    mapping = [('components/core/src/omnigate_core', 'lib/core/omnigate_core'),
               ('components/web/app.py', 'lib/web/app.py'),
               ('components/web/hmi_api.py', 'lib/web/hmi_api.py'),
               ('components/web/assets', 'share/web'),
               ('components/hmi/src/fonts/OFL.txt', 'share/licenses/DshanSans-OFL.txt'),
               ('components/hmi/src/fonts/manifest.json', 'share/licenses/DshanSans-manifest.json'),
               ('components/supervisor/supervisor.py', 'lib/supervisor/supervisor.py')]
    if native_api is not None:
        native_api = Path(native_api)
        if native_api.is_symlink() or not native_api.is_file() or not native_api.stat().st_mode & 0o111:
            raise ValueError('native API must be a regular executable artifact')
        if not desktop_service or not media_worker:
            raise ValueError('native layout requires desktop and media worker')
        mapping = [(src, dst) for src, dst in mapping if not dst.startswith('lib/')]
        components['bin/dshancontrol-api'] = native_api
        marker = destination / 'share/dshancontrol/application-layout.json'
        marker.parent.mkdir(parents=True, exist_ok=True)
        marker.write_text(json.dumps({'schema': 1, 'layout': 'native-cpp-v1',
                                     'board_id': '100ask-imx6ull-pro'}) + '\n')
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
    if licenses:
        target = destination / 'share/licenses/qt-imageformats-5.11.3'
        target.mkdir(parents=True)
        for name, artifact in licenses.items():
            shutil.copyfile(str(artifact), str(target / name))
