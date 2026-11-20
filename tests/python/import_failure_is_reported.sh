#!/bin/bash
# Regression: importing blue without Python's lib-dynload directory must
# report the missing extension. It must not terminate with a signal.
set -u
prefix="${PYTHON_PREFIX:?}"
build="${BLUE_BUILD_DIR:?}"
exe="${EXEFILE:?}"
export LD_LIBRARY_PATH="${build}:${prefix}/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
# Omit the stdlib directory. Exefile adds lib-dynload beside a stdlib entry,
# so this path is what still has to fail without a signal.
export PYTHONPATH="${build}:${prefix}/bin/python"
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
if grep -E '_struct|ModuleNotFoundError|InitBasicModuleSupport|PyCapsule_Import|Blue startup failed|Could not find platform independent libraries' "$out"; then
	exit 0
fi
echo "import blue failed without a Python error report (${code})"
exit 1
