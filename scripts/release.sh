#!/usr/bin/env bash
#
# Cuts a release by tagging main and pushing the tag; the release workflow does
# everything else (build, Linux packages, DMG, notes, GitHub release).
#
# Usage: scripts/release.sh beta|stable|major|minor|patch
#   beta    next beta: v1.0.0-beta.3 -> v1.0.0-beta.4, v1.0.0 -> v1.0.1-beta.1
#   stable  promotes the current beta as is: v1.0.0-beta.4 -> v1.0.0
#   major|minor|patch  bumps a stable release: v1.0.0 -> v2.0.0, v1.1.0 or v1.0.1
#
# Versions are never skipped: a bump is refused while the latest tag is a prerelease,
# since v1.0.0-beta.3 -> v1.0.1 would leave 1.0.0 unreleased.
#
#   SNIM_RELEASE_LATEST_TAG=<tag>  overrides the detected latest tag (for testing).
#   SNIM_RELEASE_DRY_RUN=1         prints "<latest> -> <next>" and exits, touching nothing.

set -euo pipefail

BUMP="${1:?usage: release.sh beta|stable|major|minor|patch}"

case "$BUMP" in
    beta|stable|major|minor|patch) ;;
    *)
        echo "error: expected beta, stable, major, minor or patch (got '$BUMP')" >&2
        exit 1
        ;;
esac

# Prereleases must sort below their release, which git only does when the suffix is known.
latest_release_tag() {
    if [ -n "${SNIM_RELEASE_LATEST_TAG-}" ]; then
        printf '%s\n' "$SNIM_RELEASE_LATEST_TAG"
        return
    fi
    git -c versionsort.suffix=-beta. tag --list 'v*' --sort=-v:refname | head -n1
}

# next_tag <mode> <latest tag, empty when the repo has none>
next_tag() {
    local MODE="$1"
    local LATEST="${2-}"
    local VERSION BASE PRE="" MAJOR MINOR PATCH

    VERSION="${LATEST#v}"
    BASE="${VERSION%%-*}"
    case "$VERSION" in
        *-beta.*) PRE="${VERSION#*-beta.}" ;;
        *-*)
            echo "error: cannot count on from '$LATEST', which is not a beta" >&2
            return 1
            ;;
    esac
    if [ -n "$PRE" ]; then
        case "$PRE" in
            ''|*[!0-9]*)
                echo "error: cannot count on from the prerelease in '$LATEST'" >&2
                return 1
                ;;
        esac
    fi

    IFS='.' read -r MAJOR MINOR PATCH <<<"${BASE:-0.0.0}"
    : "${MAJOR:=0}" "${MINOR:=0}" "${PATCH:=0}"

    case "$MODE" in
        beta)
            if [ -n "$PRE" ]; then
                printf 'v%s.%s.%s-beta.%s\n' "$MAJOR" "$MINOR" "$PATCH" "$((PRE + 1))"
            elif [ -z "$LATEST" ]; then
                printf 'v1.0.0-beta.1\n'
            else
                printf 'v%s.%s.%s-beta.1\n' "$MAJOR" "$MINOR" "$((PATCH + 1))"
            fi
            ;;
        stable)
            if [ -z "$PRE" ]; then
                if [ -z "$LATEST" ]; then
                    echo "error: there is no prerelease to promote; use beta first" >&2
                else
                    echo "error: $LATEST is not a prerelease; use major, minor or patch" >&2
                fi
                return 1
            fi
            # Promotion only: the beta was built from this very tree.
            printf 'v%s.%s.%s\n' "$MAJOR" "$MINOR" "$PATCH"
            ;;
        major|minor|patch)
            if [ -n "$PRE" ]; then
                echo "error: $LATEST is a prerelease and $MAJOR.$MINOR.$PATCH is not out yet;" \
                    "promote it with stable or cut another beta" >&2
                return 1
            fi
            case "$MODE" in
                major) printf 'v%s.0.0\n' "$((MAJOR + 1))" ;;
                minor) printf 'v%s.%s.0\n' "$MAJOR" "$((MINOR + 1))" ;;
                patch) printf 'v%s.%s.%s\n' "$MAJOR" "$MINOR" "$((PATCH + 1))" ;;
            esac
            ;;
    esac
}

if [ -n "${SNIM_RELEASE_DRY_RUN-}" ]; then
    DRY_LATEST="$(latest_release_tag)"
    DRY_NEXT="$(next_tag "$BUMP" "$DRY_LATEST")"
    printf '%s -> %s\n' "${DRY_LATEST:-(no tags)}" "$DRY_NEXT"
    exit 0
fi

BRANCH="$(git rev-parse --abbrev-ref HEAD)"
if [ "$BRANCH" != "main" ]; then
    echo "error: releases are cut from main, not '$BRANCH'" >&2
    exit 1
fi

# Untracked files cannot reach a tag, and this repo keeps some on purpose.
if [ -n "$(git status --porcelain --untracked-files=no)" ]; then
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

LATEST_TAG="$(latest_release_tag)"
NEW_TAG="$(next_tag "$BUMP" "$LATEST_TAG")"
NEW_VERSION="${NEW_TAG#v}"
if git rev-parse --quiet --verify "refs/tags/$NEW_TAG" > /dev/null; then
    echo "error: $NEW_TAG already exists" >&2
    exit 1
fi

echo
echo "  ${LATEST_TAG:-(no tags)} -> $NEW_TAG"
echo
if [ -z "$LATEST_TAG" ]; then
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
