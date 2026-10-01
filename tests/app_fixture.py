"""Create isolated runtime data beside a test executable or inside a macOS bundle."""
from pathlib import Path
import shutil


def create_test_app(binary: Path, root: Path):
    binary = binary.resolve()
    root = root.resolve()
    if binary.parent.name == 'MacOS' and binary.parent.parent.name == 'Contents':
        source_contents = binary.parent.parent
        resources = source_contents / 'Resources'
        contents = root / 'zt.app' / 'Contents'
        executable = contents / 'MacOS' / binary.name
        executable.parent.mkdir(parents=True)
        # Copy instead of linking so executable-path detection selects the
        # scratch bundle's Resources, not the original application's config.
        shutil.copy2(binary, executable)
        for name in ('Info.plist', 'PkgInfo'):
            if (source_contents / name).is_file():
                shutil.copy2(source_contents / name, contents / name)
        app = contents / 'Resources'
        app.mkdir()
        frameworks = source_contents / 'Frameworks'
        if frameworks.is_dir():
            (contents / 'Frameworks').symlink_to(frameworks, target_is_directory=True)
    else:
        resources = binary.parent
        app = root
        executable = app / 'zt'
        executable.symlink_to(binary)

    (app / 'skins').symlink_to(resources / 'skins', target_is_directory=True)
    (app / 'syx').mkdir()
    return app, executable
