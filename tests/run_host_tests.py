#!/usr/bin/env python3
"""Build regression tests against the installed LVGL, with ASan and UBSan."""
import argparse
import concurrent.futures
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--lvgl', type=Path, default=Path.home() / 'Arduino/libraries/lvgl')
parser.add_argument('--build-dir', type=Path)
parser.add_argument('--sanitizer-lib-dir', type=Path, help='Optional directory containing sanitizer runtime libraries')
args = parser.parse_args()
lvgl = args.lvgl.resolve()
if not (lvgl / 'lvgl.h').is_file():
    parser.error('--lvgl must point to an LVGL 9.5 library directory')
build = args.build_dir or Path(tempfile.mkdtemp(prefix='webscreen-host-tests-'))
build.mkdir(parents=True, exist_ok=True)
flags = ['-O1', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
         '-fno-pie', '-DLV_CONF_INCLUDE_SIMPLE', '-I' + str(ROOT),
         '-I' + str(ROOT / 'webscreen'), '-I' + str(lvgl)]
sources = sorted((lvgl / 'src').rglob('*.c')) + [ROOT / 'webscreen/elk.c', ROOT / 'tests/host_regressions.cpp']
# Rebuild when any dependency changes, including compiler options above.
headers = list((lvgl / 'src').rglob('*.h')) + [ROOT / 'lv_conf.h', Path(__file__)]
stamp = max(path.stat().st_mtime for path in headers)

def compile_source(item):
    index, source = item
    output = build / f'{index}.o'
    source_stamp = max(stamp, source.stat().st_mtime)
    if source.suffix == '.cpp':
        source_stamp = max(source_stamp, *(p.stat().st_mtime for p in (ROOT / 'webscreen').glob('*.h')))
    if not output.exists() or output.stat().st_mtime < source_stamp:
        compiler = os.environ.get('CXX', 'g++') if source.suffix == '.cpp' else os.environ.get('CC', 'gcc')
        standard = '-std=c++17' if source.suffix == '.cpp' else '-std=gnu11'
        result = subprocess.run([compiler, standard, *flags, '-c', str(source), '-o', str(output)], capture_output=True, text=True)
        if result.returncode:
            raise RuntimeError(f'{source}\n{result.stderr}')
    return str(output)

print(f'Building LVGL regression tests in {build}', flush=True)
with concurrent.futures.ThreadPoolExecutor(max_workers=min(6, os.cpu_count() or 1)) as pool:
    objects = list(pool.map(compile_source, enumerate(sources)))
binary = build / 'host_regressions'
link_flags = []
if args.sanitizer_lib_dir:
    path = str(args.sanitizer_lib_dir.resolve())
    link_flags = ['-L' + path, '-Wl,-rpath,' + path]
result = subprocess.run([os.environ.get('CXX', 'g++'), *flags, *link_flags, '-no-pie', *objects, '-lm', '-o', str(binary)], capture_output=True, text=True)
if result.returncode:
    raise SystemExit(result.stderr)
env = dict(os.environ, ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
subprocess.run([str(binary)], check=True, env=env)
