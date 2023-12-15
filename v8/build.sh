#!/bin/bash

python_bin=${PYTHON_BIN:-"python3.11"}
basedir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
ninja_jobs=${NINJA_JOBS:-"6"}

patch_v8_code() {
  sed -i 's/std::is_pod/std::is_standard_layout/g' $basedir/v8/src/base/vector.h
  sed -i 's/std::back_insert_iterator(snapshots)/std::back_insert_iterator<base::SmallVector<Snapshot, 8>>(snapshots)/g' $basedir/v8/src/compiler/turboshaft/wasm-gc-type-reducer.cc
}

build_v8() {
  $python_bin -m venv venv
  source venv/bin/activate
  echo "Building V8 ..."
  PATH=$basedir/depot_tools:$PATH
  cd $basedir
  fetch v8
  cd v8
  git checkout branch-heads/11.9
  args=$(cat <<EOF
dcheck_always_on = false
is_component_build = false
is_debug = false
target_cpu = "x64"
use_custom_libcxx = false
v8_enable_sandbox = true
v8_monolithic = true
v8_use_external_startup_data = false
EOF
)
  patch_v8_code

  gn gen out/x86.release --args="${args}"
  ninja -j$ninja_jobs -C out/x86.release v8_monolith || exit 1
}

clean_v8() {
  echo "Cleaning v8 build ..."
  rm -rf $basedir/v8
  rm -rf $basedir/.cipd
  rm -rf $basedir/.gclient
  rm -rf $basedir/.gclient_entries
  rm -rf $basedir/.gclient_previous_sync_commits
}

build_bridge_v8() {
  echo "Building bridge_v8 library ..."
  mkdir -p $basedir/build && cd $basedir/build && cmake ../bridge && make bridge_v8
}

clean_bridge_v8() {
  echo "Cleaning bridge_v8 library build ..."
  rm -rf $basedir/build
}

error() {
  echo "error: $*" >/dev/stderr
}

cmd="${1}"
shift 1

echo "Invoked with basedir: ${basedir}"
echo

case ${cmd} in
  build_v8)
    build_v8 || exit 1
    ;;
  clean_v8)
    clean_v8 || exit 1
    ;;
  build_bridge_v8)
    build_bridge_v8 || exit 1
    ;;
  clean_bridge_v8)
    clean_bridge_v8 || exit 1
    ;;
  *)
    error "unknown command '${cmd}'"
    exit 1
    ;;
esac
