#!/bin/sh
# Publish the Emscripten build to GitHub Pages.
#
# Usage: _tools/web/deploy.sh
#
# Copies build-emscripten/ into a temporary folder, adds the bundled demos,
# an index.html and this folder's README.md, and force-pushes it all as a
# single commit to the gh-pages branch of SITE_REPO, so the site repository
# doesn't grow with every deploy.
#
# Only the game folders listed as demos in dists/emscripten/assets/scummvm.ini
# are published, so other games in _games/ can't be published by accident.

set -e

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
SITE_REPO=${SITE_REPO:-git@github.com:rjimenezda/scummvm-learn.git}
BUILD="$ROOT/build-emscripten"
INI="$ROOT/dists/emscripten/assets/scummvm.ini"

if [ ! -f "$BUILD/scummvm.wasm" ]; then
	echo "No build in $BUILD, run ./dists/emscripten/build.sh build first" >&2
	exit 1
fi

STAGE=$(mktemp -d)
trap 'rm -rf "$STAGE"' EXIT

# Leave out games from the build folder; the demos are added below
rsync -a --exclude /data/games/ "$BUILD/" "$STAGE/"
mkdir -p "$STAGE/data/games"
for demo in $(sed -n 's|^path=/data/games/||p' "$INI"); do
	if [ ! -d "$ROOT/_games/$demo" ]; then
		echo "Demo $demo from scummvm.ini is missing in _games/" >&2
		exit 1
	fi
	rsync -a --exclude '.*' "$ROOT/_games/$demo" "$STAGE/data/games/"
done
python3 "$ROOT/dists/emscripten/build-make_http_index.py" "$STAGE/data" >/dev/null

cp "$BUILD/scummvm.html" "$STAGE/index.html"
cp "$ROOT/_tools/web/README.md" "$STAGE/README.md"
# Serve the files as they are, without GitHub's Jekyll processing
touch "$STAGE/.nojekyll"

REV=$(git -C "$ROOT" rev-parse --short HEAD)
cd "$STAGE"
git init -q -b gh-pages
git add -A
git commit -q -m "Deploy he-web $REV"
git push -f "$SITE_REPO" gh-pages
