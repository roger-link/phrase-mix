#!/bin/sh
set -eu

APP_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/build-make/PhraseMix_artefacts/Phrase Mix.app"
open "$APP_DIR"
