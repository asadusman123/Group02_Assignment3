#!/usr/bin/env bash
# writes GIT_HISTORY.md in the repo root from the repository's own git log
set -euo pipefail
cd "$(git rev-parse --show-toplevel)"
OUT=GIT_HISTORY.md
{
  echo "# Git history — $(basename "$PWD")"
  echo
  echo "- Repository: https://github.com/asadusman123/Group02_Assignment3"
  echo "- Generated: $(date '+%Y-%m-%d %H:%M') from \`git log\`"
  echo "- Commits: $(git rev-list --count HEAD), from $(git log --reverse --format=%ad --date=short | head -1) to $(git log -1 --format=%ad --date=short)"
  echo
  echo "## All commits (oldest first)"
  echo
  echo "| # | date | commit | author | message | files | +lines | −lines |"
  echo "|---|---|---|---|---|---|---|---|"
  n=0
  for h in $(git log --reverse --format=%H); do
    n=$((n+1))
    meta=$(git log -1 --date=format:'%Y-%m-%d %H:%M' --format='%ad%x09%h%x09%an%x09%s' "$h")
    IFS=$'\t' read -r d sh au msg <<< "$meta"
    msg=${msg//|/\\|}
    read -r f a r < <(git show --numstat --format= "$h" | awk '{f++; if($1!="-")a+=$1; if($2!="-")r+=$2} END{print f+0, a+0, r+0}')
    echo "| $n | $d | \`$sh\` | $au | $msg | $f | $a | $r |"
  done
  echo
  echo "## Files changed per commit"
  for h in $(git log --reverse --format=%H); do
    echo
    git log -1 --date=format:'%Y-%m-%d %H:%M' --format='### `%h` %s%n%n%ad · %an%n' "$h"
    echo '```text'
    git show --stat=100 --format= "$h" | sed '/^$/d'
    echo '```'
  done
} > "$OUT"
echo "wrote $OUT ($(git rev-list --count HEAD) commits)"
