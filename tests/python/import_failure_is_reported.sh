#!/bin/bash
# Regression: importing blue without Python's lib-dynload directory must
# report the missing extension. It must not terminate with a signal.
set -u
prefix="${PYTHON_PREFIX:?}"
build="${BLUE_BUILD_DIR:?}"
exe="${EXEFILE:?}"
export LD_LIBRARY_PATH="${build}:${prefix}/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export PYTHONPATH="${build}:${prefix}/lib/python3.12:${prefix}/bin/python"
export BUILDFLAVOR="${BUILDFLAVOR:-release}"
out="$(mktemp)"
set +e
"$exe" /nocrashreportupload /inherit /buildflavor=release /py -c 'import blue' >"$out" 2>&1
code=$?
set -e
cat "$out"
if [ "$code" -eq 0 ]; then
	echo "import blue succeeded without lib-dynload"
	exit 1
fi
if [ "$code" -ge 128 ]; then
	echo "import blue died from a signal (${code})"
	exit 1
fi
grep -E '_struct|ModuleNotFoundError|InitBasicModuleSupport|PyCapsule_Import|Blue startup failed' "$out"
