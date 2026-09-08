# Copyright 2026 Intrinsic Innovation LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import argparse
import json
import os
import platform
import shutil
import subprocess
import sys
import urllib.request

def _get_resource_file(filename):
    try:
        from ament_index_python.packages import get_package_share_directory
        share_dir = get_package_share_directory('intrinsic_sdk_bundle_library_py')
        resource_path = os.path.join(share_dir, 'resource', filename)
        if os.path.exists(resource_path):
            return resource_path
    except (ImportError, Exception):
        pass
    local_path = os.path.abspath(
        os.path.join(os.path.dirname(__file__), '..', 'resource', filename)
    )
    if os.path.exists(local_path):
        return local_path
    return None

# Shared argument definitions for command-line parser and colcon verb
COMMON_ARGUMENTS = {
    'ros_distro': {
        'flags': ('--ros-distro', '--ros_distro'),
        'kwargs': {
            'default': 'jazzy',
            'help': 'ROS distro to use (default: jazzy)'
        }
    },
    'bundle_dir': {
        'flags': ('--bundle-dir', '--bundle_dir', '--images-dir', '--images_dir'),
        'kwargs': {
            'default': './intrinsic_asset_bundles',
            'help': 'Directory to store built bundles (default: ./intrinsic_asset_bundles)'
        }
    },
    'builder_name': {
        'flags': ('--builder-name', '--builder_name'),
        'kwargs': {
            'default': 'container-builder',
            'help': 'Name of the buildx builder to use'
        }
    },
    'no_cache': {
        'flags': ('--no-cache',),
        'kwargs': {
            'action': 'store_true',
            'help': 'Do not use cache when building the image'
        }
    },
    'keep_builder': {
        'flags': ('--keep-builder',),
        'kwargs': {
            'action': 'store_true',
            'help': 'Do not stop the buildx builder after build'
        }
    },
    'default_config': {
        'flags': ('--default-config', '--default_config'),
        'kwargs': {
            'help': 'Default configuration file for the bundle'
        }
    }
}


def add_common_argument(parser, name, **kwargs_overrides):
    """Add a common argument to the parser, with optional overrides."""
    spec = COMMON_ARGUMENTS[name]
    kwargs = spec['kwargs'].copy()
    kwargs.update(kwargs_overrides)
    parser.add_argument(*spec['flags'], **kwargs)


def run_command(cmd, check=True):
    assert isinstance(cmd, list), 'cmd must be a list'
    print(f"Running: {' '.join(cmd)}")
    return subprocess.run(cmd, check=check)


def get_sdk_version():
    version_file = _get_resource_file('sdk_version.json')
    if not version_file or not os.path.exists(version_file):
        print('Error: Could not find sdk_version.json')
        return None

    try:
        with open(version_file, 'r') as f:
            data = json.load(f)
            return data.get('sdk_version')
    except Exception as e:
        print(f'Error reading SDK version from {version_file}: {e}')
        return None


def _resolve_latest_version(sdk_version, binary_name):
    # Parse version string
    parts = sdk_version.split('.')
    if len(parts) == 3:
        base_version = sdk_version
        start_patch = 0
    elif len(parts) == 4:
        base_version = '.'.join(parts[:3])
        try:
            start_patch = int(parts[3])
        except ValueError:
            return sdk_version
    else:
        return sdk_version

    best_version = None
    consecutive_failures = 0
    current_patch = start_patch

    while True:
        if current_patch - start_patch >= 3:
            break

        if current_patch == 0:
            version_to_test = base_version
        else:
            version_to_test = f'{base_version}.{current_patch}'

        url = (
            f'https://github.com/intrinsic-ai/sdk/releases/download/'
            f'{version_to_test}/{binary_name}'
        )

        req = urllib.request.Request(url, method='HEAD')
        try:
            with urllib.request.urlopen(req):
                best_version = version_to_test
                consecutive_failures = 0
        except Exception:
            consecutive_failures += 1
            if best_version is not None or consecutive_failures >= 3:
                break

        current_patch += 1

    return best_version if best_version is not None else sdk_version


def download_inbuild(sdk_version, dest_dir='.'):
    system = platform.system().lower()
    machine = platform.machine().lower()

    # Map machine to expected URL format
    if machine in ['x86_64', 'amd64']:
        arch = 'amd64'
    elif machine in ['aarch64', 'arm64']:
        arch = 'arm64'
    else:
        arch = machine  # fallback

    # Handle Windows extension
    ext = '.exe' if system == 'windows' else ''

    # Construct filename
    binary_name = f'inbuild-{system}-{arch}{ext}'

    download_version = _resolve_latest_version(sdk_version, binary_name)

    inbuild_path = os.path.join(dest_dir, 'inbuild' + ext)

    if not os.path.exists(inbuild_path):
        url = (
            f'https://github.com/intrinsic-ai/sdk/releases/download/'
            f'{download_version}/{binary_name}'
        )
        print(f'Downloading inbuild from {url} to {inbuild_path}...')
        try:
            urllib.request.urlretrieve(url, inbuild_path)
            os.chmod(inbuild_path, 0o755)
        except Exception as e:
            print(f'Failed to download inbuild from {url}: {e}')
            print(
                f'This could be because a binary for your system ({system}) '
                f'and architecture ({arch}) is not available in the releases.'
            )
            sys.exit(1)
    else:
        print(f'inbuild already exists at {inbuild_path}')
    return inbuild_path


def build_container(args):
    print('Building container...')
    tag = None
    dockerfile = None
    name = None
    package = None

    if args.service_name and args.service_package:
        name = args.service_name
        package = args.service_package
        dockerfile = args.dockerfile or _get_resource_file('service.Dockerfile')
    elif args.skill_name and args.skill_package:
        name = args.skill_name
        package = args.skill_package
        dockerfile = args.dockerfile or _get_resource_file('skill.Dockerfile')
    else:
        print('Error: Must specify either service or skill name and package.')
        return

    tag = f'{package}:{name}'
    bundle_dir = (
        getattr(args, 'bundle_dir', None)
        or getattr(args, 'images_dir', None)
        or './intrinsic_asset_bundles'
    )
    dockerfile = os.path.realpath(dockerfile)

    # Ensure .dockerignore exists to avoid sending large directories to build context
    dockerignore_path = '.dockerignore'
    if not os.path.exists(dockerignore_path):
        print(f'Creating {dockerignore_path}...')
        try:
            with open(dockerignore_path, 'w') as f:
                f.write('images\nbuild\nlog\ninstall\n')
        except Exception as e:
            print(f'Warning: Could not create {dockerignore_path}: {e}')

    # Ensure builder exists
    builder_name = args.builder_name or 'container-builder'
    try:
        run_command(['docker', 'buildx', 'inspect', '--builder', builder_name])
    except Exception:
        print(f'Builder {builder_name} not found. Creating it...')
        run_command([
            'docker', 'buildx', 'create', '--name', builder_name,
            '--driver', 'docker-container'
        ])
        run_command(['docker', 'buildx', 'use', builder_name])

    tar_dir = os.path.join(bundle_dir, name)
    os.makedirs(tar_dir, exist_ok=True)
    tar_path = os.path.join(tar_dir, f'{name}.tar')

    try:
        # Docker buildx build
        cmd = ['docker', 'buildx', 'build', '-t', tag, '-f', dockerfile, '--builder', builder_name]
        if args.no_cache:
            cmd.append('--no-cache')

        # Output flag
        output_arg = (
            f'type=docker,'
            f'dest={tar_path},'
            f'compression=zstd,'
            f'push=false,'
            f'name={tag}'
        )
        cmd.extend(['--output', output_arg])

        # Build args
        cmd.extend(['--build-arg', f'ROS_DISTRO={args.ros_distro}'])
        cmd.extend(['--build-arg', f'SKILL_TYPE={args.skill_type}'])
        if args.service_name:
            cmd.extend([
                '--build-arg', f'SERVICE_PACKAGE={package}',
                '--build-arg', f'SERVICE_NAME={name}',
                '--build-arg', f'SERVICE_EXECUTABLE_NAME={name}_main'
            ])
        else:
            cmd.extend([
                '--build-arg', f'SKILL_PACKAGE={package}',
                '--build-arg', f'SKILL_NAME={name}',
            ])
            skill_executable = args.skill_executable or f'lib/{package}/{name}_main'
            cmd.extend(['--build-arg', f'SKILL_EXECUTABLE={skill_executable}'])

            skill_config = args.skill_config or f'share/{package}/{name}_config.pbbin'
            cmd.extend(['--build-arg', f'SKILL_CONFIG={skill_config}'])

            if args.skill_asset_id_org:
                cmd.extend(['--build-arg', f'SKILL_ASSET_ID_ORG={args.skill_asset_id_org}'])

        if args.dependencies:
            cmd.extend(['--build-arg', f'DEPENDENCIES={args.dependencies}'])
        if args.source_dir:
            cmd.extend(['--build-arg', f'SOURCE_DIR={args.source_dir}'])
        if args.overlay_source:
            cmd.extend(['--build-arg', f'OVERLAY_SOURCE={args.overlay_source}'])

        cmd.append('.')

        run_command(cmd)
        print(f'Saved compressed image to {tar_path}')
    finally:
        if not args.keep_builder:
            print(f'Stopping builder {builder_name}...')
            run_command(['docker', 'buildx', 'stop', builder_name])


def build_bundle(args):
    print('Building bundle...')
    name = args.service_name or args.skill_name
    package = args.service_package or args.skill_package

    if not name or not package:
        print('Error: Must specify name and package.')
        return

    if not args.manifest_path:
        print('Error: Must specify --manifest_path.')
        return

    bundle_dir = (
        getattr(args, 'bundle_dir', None)
        or getattr(args, 'images_dir', None)
        or './intrinsic_asset_bundles'
    )
    tar_path = os.path.join(bundle_dir, name, f'{name}.tar')

    if not os.path.exists(tar_path):
        print(f'Error: Image tar not found at {tar_path}. Run build-container first.')
        return

    # Load image (Podman with Docker fallback)
    runtime = 'podman'
    if not shutil.which('podman'):
        runtime = 'docker'
    else:
        try:
            run_command(['podman', 'load', '-i', tar_path])
        except subprocess.CalledProcessError:
            print('podman load failed, falling back to docker...')
            runtime = 'docker'

    if runtime == 'docker':
        run_command(['docker', 'load', '-i', tar_path])

    # Extract descriptor
    container_name = f'temp_container_{name}'
    if runtime == 'podman':
        run_command(['podman', 'create', '--replace', '--name', container_name, f'{package}:{name}'])
    else:
        subprocess.run(['docker', 'rm', '-f', container_name], capture_output=True)
        run_command(['docker', 'create', '--name', container_name, f'{package}:{name}'])

    desc_path = os.path.join(bundle_dir, name, f'{name}_protos.desc')
    if args.service_name:
        paths_to_try = [
            f'/opt/ros/overlay/install/share/{package}/{name}_protos.desc',
            f'/opt/ros/overlay/install/share/{name}/{name}_protos.desc'
        ]
        success = False
        for src_path in paths_to_try:
            try:
                run_command([runtime, 'cp', f'{container_name}:{src_path}', desc_path])
                success = True
                break
            except subprocess.CalledProcessError:
                print(f'Failed to copy from {src_path}, trying next path...')
                continue
        if not success:
            raise subprocess.CalledProcessError(
                1,
                'Failed to copy descriptor file from any of the expected paths.'
            )

    else:
        src_path = f'/opt/{name}_workspace/install/share/{package}/{name}_protos.desc'
        run_command([runtime, 'cp', f'{container_name}:{src_path}', desc_path])

    run_command([runtime, 'rm', '-f', container_name])

    # Check if inbuild is in PATH or current directory
    inbuild_path = shutil.which('inbuild') or './inbuild'

    if inbuild_path == './inbuild' and not os.path.exists('./inbuild'):
        sdk_version = get_sdk_version()
        if not sdk_version:
            print('Error: Could not determine SDK version.')
            return

        inbuild_path = download_inbuild(sdk_version)

    # Build bundle
    inbuild_cmd = [inbuild_path]
    if args.service_name:
        inbuild_cmd.extend(['service', 'bundle'])
    else:
        inbuild_cmd.extend(['skill', 'bundle'])

    inbuild_cmd.extend([
        '--file_descriptor_set', desc_path,
        '--manifest', args.manifest_path,
        '--oci_image', tar_path,
        '--output', os.path.join(bundle_dir, name, f'{name}.bundle.tar')
    ])

    if args.default_config:
        inbuild_cmd.extend(['--default_config', args.default_config])

    run_command(inbuild_cmd)


def main():
    parser = argparse.ArgumentParser(description='Build container and bundle for skills/services.')
    subparsers = parser.add_subparsers(dest='command', help='Command to run')

    # Build container parser
    parser_container = subparsers.add_parser('container', help='Build container')
    add_common_argument(parser_container, 'bundle_dir')
    add_common_argument(parser_container, 'builder_name')
    parser_container.add_argument('--service_name')
    parser_container.add_argument('--service_package')
    parser_container.add_argument('--skill_name')
    parser_container.add_argument('--skill_package')
    parser_container.add_argument('--dockerfile')
    parser_container.add_argument('--dependencies')
    add_common_argument(parser_container, 'ros_distro')
    parser_container.add_argument('--skill_executable')
    parser_container.add_argument('--skill_config')
    parser_container.add_argument('--skill_asset_id_org')
    parser_container.add_argument('--skill_type', choices=['cpp', 'python'], default='cpp')
    add_common_argument(parser_container, 'no_cache')
    add_common_argument(parser_container, 'keep_builder')
    parser_container.add_argument(
        '--source-dir',
        help='Override SOURCE_DIR build arg in service.Dockerfile'
    )
    parser_container.add_argument(
        '--overlay-source',
        help='Override OVERLAY_SOURCE build arg in service.Dockerfile'
    )

    # Build bundle parser
    parser_bundle = subparsers.add_parser('bundle', help='Build bundle')
    add_common_argument(parser_bundle, 'bundle_dir')
    add_common_argument(
        parser_bundle, 'builder_name',
        help='Ignored, for backward compatibility with scripts'
    )
    parser_bundle.add_argument('--service_name')
    parser_bundle.add_argument('--service_package')
    parser_bundle.add_argument('--skill_name')
    parser_bundle.add_argument('--skill_package')
    parser_bundle.add_argument('--manifest_path', required=True)
    add_common_argument(parser_bundle, 'default_config')

    args = parser.parse_args()

    if args.command == 'container':
        build_container(args)
    elif args.command == 'bundle':
        build_bundle(args)
    else:
        parser.print_help()


if __name__ == '__main__':
    main()
