#!/usr/bin/env bash
#
# Cuts a release by tagging main and pushing the tag; the release workflow does
# everything else (build, AppImage, DMG, notes, GitHub release).
#
# Usage: scripts/release.sh major|minor|patch

set -euo pipefail

BUMP="${1:?usage: release.sh major|minor|patch}"

case "$BUMP" in
    major|minor|patch) ;;
    *)
        echo "error: expected major, minor or patch (got '$BUMP')" >&2
        exit 1
        ;;
esac

BRANCH="$(git rev-parse --abbrev-ref HEAD)"
if [ "$BRANCH" != "main" ]; then
    echo "error: releases are cut from main, not '$BRANCH'" >&2
    exit 1
fi

if [ -n "$(git status --porcelain)" ]; then
    echo "error: working tree is not clean" >&2
    exit 1
fi

echo "==> Fetching origin"
git fetch --quiet --tags origin

HEAD_SHA="$(git rev-parse HEAD)"
if [ "$HEAD_SHA" != "$(git rev-parse origin/main)" ]; then
    echo "error: HEAD and origin/main differ; push or pull first" >&2
    exit 1
fi

echo "==> Checking CI on $HEAD_SHA"
CI_RUNS="$(gh run list --commit "$HEAD_SHA" --workflow CI --limit 20 \
    --json status,conclusion --jq '.[] | "\(.status) \(.conclusion)"')"
if [ -z "$CI_RUNS" ]; then
    echo "error: no CI run found for $HEAD_SHA" >&2
    exit 1
fi

while read -r status conclusion; do
    [ -n "$status" ] || continue
    if [ "$status" != "completed" ]; then
        echo "error: CI is still running on $HEAD_SHA (status: $status)" >&2
        exit 1
    fi
    if [ "$conclusion" != "success" ] && [ "$conclusion" != "skipped" ]; then
        echo "error: CI is not green on $HEAD_SHA (conclusion: $conclusion)" >&2
        exit 1
    fi
done <<EOF
$CI_RUNS
EOF

LATEST_TAG="$(git tag --list 'v*' --sort=-v:refname | head -n1)"
: "${LATEST_TAG:=v0.0.0}"

IFS='.' read -r MAJOR MINOR PATCH <<<"${LATEST_TAG#v}"
case "$BUMP" in
    major) MAJOR=$((MAJOR + 1)); MINOR=0; PATCH=0 ;;
    minor) MINOR=$((MINOR + 1)); PATCH=0 ;;
    patch) PATCH=$((PATCH + 1)) ;;
esac
NEW_VERSION="$MAJOR.$MINOR.$PATCH"
NEW_TAG="v$NEW_VERSION"

echo
echo "  $LATEST_TAG -> $NEW_TAG"
echo
if [ "$LATEST_TAG" = "v0.0.0" ]; then
    echo "  (first release)"
else
    echo "  Commits since $LATEST_TAG:"
    git log --no-merges --format='    %s' "$LATEST_TAG..HEAD" | head -n 15
fi
echo

read -r -p "Tag and push $NEW_TAG? [y/N] " REPLY
case "$REPLY" in
    y|Y) ;;
    *)
        echo "aborted"
        exit 1
        ;;
esac

git tag -a "$NEW_TAG" -m "Snim $NEW_VERSION"
git push origin "$NEW_TAG"

echo "==> Pushed $NEW_TAG; the release workflow takes it from here"
