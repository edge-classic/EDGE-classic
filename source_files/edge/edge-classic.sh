#!/bin/sh
# Needed to make symlinks/shortcuts work.
# the binaries must run with correct working directory
exec "/usr/local/bin/edge-classic.bin" "$@" "-game" "/usr/local/share/edge-classic"