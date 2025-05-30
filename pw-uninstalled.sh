#!/usr/bin/env bash

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

while getopts ":b:v:" opt; do
	case ${opt} in
		b)
			BUILDDIR=${OPTARG}
			;;
		v)
			VERSION=${OPTARG}
			echo "Version: ${VERSION}"
			;;
		\?)
			echo "Invalid option: -${OPTARG}"
			exit 1
			;;
		:)
			echo "Option -${OPTARG} requires an argument"
			exit 1
			;;
	esac
done

if [ -z "${BUILDDIR}" ]; then
	BUILDDIR=${SCRIPT_DIR}/builddir
	echo "Using default build directory: ${BUILDDIR}"
fi

if [ ! -d "${BUILDDIR}" ]; then
	echo "Invalid build directory: ${BUILDDIR}"
	exit 1
fi

# the config file read by the daemon
export PIPEWIRE_CONFIG_DIR="${BUILDDIR}/src/daemon"
# the directory with SPA plugins
export SPA_PLUGIN_DIR="${BUILDDIR}/spa/plugins"
export SPA_DATA_DIR="${SCRIPT_DIR}/spa/plugins"
# the directory with pipewire modules
export PIPEWIRE_MODULE_DIR="${BUILDDIR}/src/modules"
export PATH="${BUILDDIR}/src/daemon:${BUILDDIR}/src/tools:${BUILDDIR}/src/media-session:${BUILDDIR}/src/examples:${BUILDDIR}/pipewire-v4l2/src:${PATH}"
export LD_LIBRARY_PATH="${BUILDDIR}/src/pipewire/:${BUILDDIR}/pipewire-jack/src/${LD_LIBRARY_PATH+":$LD_LIBRARY_PATH"}"
export GST_PLUGIN_PATH="${BUILDDIR}/src/gst/${GST_PLUGIN_PATH+":${GST_PLUGIN_PATH}"}"
# the directory with card profiles and paths
export ACP_PATHS_DIR="${SCRIPT_DIR}/spa/plugins/alsa/mixer/paths"
export ACP_PROFILES_DIR="${SCRIPT_DIR}/spa/plugins/alsa/mixer/profile-sets"
# ALSA plugin directory
export ALSA_PLUGIN_DIR="${BUILDDIR}/pipewire-alsa/alsa-plugins"

export PW_BUILDDIR=$BUILDDIR
export PW_UNINSTALLED=1
export PKG_CONFIG_PATH="${BUILDDIR}/meson-uninstalled/:${PKG_CONFIG_PATH}"
export PIPEWIRE_LOG_SYSTEMD=false

if [ -d "${BUILDDIR}/subprojects/wireplumber" ]; then
	# FIXME: find a nice, shell-neutral way to specify a prompt
	"${SCRIPT_DIR}"/subprojects/wireplumber/wp-uninstalled.sh -b"${BUILDDIR}"/subprojects/wireplumber "${SHELL}"
elif [ -d "${BUILDDIR}/subprojects/media-session" ]; then
	# FIXME: find a nice, shell-neutral way to specify a prompt
	"${SCRIPT_DIR}"/subprojects/media-session/media-session-uninstalled.sh -b"${BUILDDIR}"/subprojects/media-session "${SHELL}"
else
	# FIXME: find a nice, shell-neutral way to specify a prompt
	${SHELL}
fi
