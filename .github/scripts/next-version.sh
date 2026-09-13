#!/usr/bin/env bash
#
# next-version.sh — works out the next semantic version from Conventional Commits.
#
# Looks at every commit since the most recent v* tag and decides:
#   feat!: / fix!: / "BREAKING CHANGE" in body  -> major bump
#   feat:                                       -> minor bump
#   fix: / perf:                                -> patch bump
#   anything else (chore, docs, ci, refactor…)  -> no bump on its own
#
# Writes GitHub Actions outputs (version, tag, previous_tag, bump) and a
# markdown changelog to $NOTES_FILE.
#
# Env:
#   FIRST_VERSION  version to use when the repo has no v* tag yet (default 0.1.0)
#   FORCE_VERSION  skip the analysis and use this version instead (e.g. 1.4.0)
#   NOTES_FILE     where to write the changelog (default release-notes.md)

set -euo pipefail

FIRST_VERSION="${FIRST_VERSION:-0.1.0}"
FORCE_VERSION="${FORCE_VERSION:-}"
NOTES_FILE="${NOTES_FILE:-release-notes.md}"

emit() {
  printf '%s=%s\n' "$1" "$2"
  if [ -n "${GITHUB_OUTPUT:-}" ]; then
    printf '%s=%s\n' "$1" "$2" >> "$GITHUB_OUTPUT"
  fi
}

rank() {
  case "$1" in
    major) echo 3 ;;
    minor) echo 2 ;;
    patch) echo 1 ;;
    *)     echo 0 ;;
  esac
}

promote() {
  if [ "$(rank "$1")" -gt "$(rank "$bump")" ]; then
    bump="$1"
  fi
}

# ---------------------------------------------------------------- previous tag

previous_tag="$(git tag --list 'v[0-9]*' --sort=-v:refname | head -n1 || true)"

if [ -n "$previous_tag" ]; then
  prev_version="${previous_tag#v}"
  IFS='.' read -r major minor patch <<< "$prev_version"
  major="${major:-0}"; minor="${minor:-0}"; patch="${patch:-0}"
else
  prev_version=""
  major=0; minor=0; patch=0
fi

# ------------------------------------------------------------ commit analysis

bump=none
breaks=""
feats=""
fixes=""
others=""

mapfile -t shas < <(git rev-list --no-merges HEAD ${previous_tag:+^$previous_tag})

for sha in "${shas[@]}"; do
  [ -n "$sha" ] || continue

  subject="$(git show -s --format=%s "$sha")"
  body="$(git show -s --format=%b "$sha")"
  short="${sha:0:7}"

  type=""
  breaking=no

  if [[ "$subject" =~ ^([a-zA-Z]+)(\(([^\)]*)\))?(!)?:[[:space:]]*(.*)$ ]]; then
    type="${BASH_REMATCH[1]}"
    scope="${BASH_REMATCH[3]}"
    desc="${BASH_REMATCH[5]}"
    if [ -n "${BASH_REMATCH[4]}" ]; then
      breaking=yes
    fi
  else
    scope=""
    desc="$subject"
  fi

  if [[ "$body" == *"BREAKING CHANGE"* ]]; then
    breaking=yes
  fi

  line="- "
  if [ -n "$scope" ]; then
    line+="**${scope}:** "
  fi
  line+="${desc} (\`${short}\`)"

  if [ "$breaking" = yes ]; then
    promote major
    breaks+="${line}"$'\n'
    continue
  fi

  case "$type" in
    feat)
      promote minor
      feats+="${line}"$'\n'
      ;;
    fix|perf)
      promote patch
      fixes+="${line}"$'\n'
      ;;
    *)
      others+="${line}"$'\n'
      ;;
  esac
done

# ------------------------------------------------------------------- new version

if [ -n "$FORCE_VERSION" ]; then
  version="${FORCE_VERSION#v}"
  bump=forced
elif [ -z "$previous_tag" ]; then
  # First ever release: start from FIRST_VERSION unless something breaking landed.
  if [ "$bump" = major ]; then
    version="1.0.0"
  else
    version="$FIRST_VERSION"
  fi
  # An initial release always happens, even if no commit is a feat/fix.
  if [ "$bump" = none ]; then
    bump=initial
  fi
else
  case "$bump" in
    major) version="$((major + 1)).0.0" ;;
    minor) version="${major}.$((minor + 1)).0" ;;
    patch) version="${major}.${minor}.$((patch + 1))" ;;
    none)  version="$prev_version" ;;
  esac
fi

# ---------------------------------------------------------------------- notes

{
  if [ -n "$breaks" ]; then
    printf '### Breaking changes\n\n%s\n' "$breaks"
  fi
  if [ -n "$feats" ]; then
    printf '### Features\n\n%s\n' "$feats"
  fi
  if [ -n "$fixes" ]; then
    printf '### Fixes\n\n%s\n' "$fixes"
  fi
  if [ -n "$others" ]; then
    printf '<details>\n<summary>Other changes</summary>\n\n%s\n</details>\n' "$others"
  fi
  if [ -n "$previous_tag" ]; then
    printf '\n**Full changelog:** %s...v%s\n' "$previous_tag" "$version"
  fi
} > "$NOTES_FILE"

# -------------------------------------------------------------------- outputs

emit version      "$version"
emit tag          "v${version}"
emit previous_tag "$previous_tag"
emit bump         "$bump"
emit notes_file   "$NOTES_FILE"

echo "---- $NOTES_FILE ----"
cat "$NOTES_FILE"
