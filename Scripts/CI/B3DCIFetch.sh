#!/bin/bash
set -e

# Checks out the source tree a build runs against. The root commit ($GIT_COMMIT, or the head of
# origin/$GIT_BRANCH when unset) defines the whole tree: every submodule, recursively, is checked
# out at the commit its parent pins. Submodules never follow a branch here; when the pins on a
# branch are behind, BansheeForge offers to update them before the build is created.
#
# Works for both an editor checkout and a standalone Framework checkout.

echo "=== Fetching sources ==="
echo "Repository: $GIT_URL"
echo "Branch: $GIT_BRANCH"
echo "Commit: ${GIT_COMMIT:-<branch head>}"
echo "Workspace: $WORKSPACE"
echo "Clean Build: ${CLEAN_BUILD:-0}"

cd "$WORKSPACE"

# Initialize git repo if needed
if [ ! -d ".git" ]; then
	echo "Initializing git repository..."
	git init
	git remote add origin "$GIT_URL"
else
	echo "Using existing git repository (incremental fetch)"
	git remote set-url origin "$GIT_URL"
fi

# Discards local modifications in a repository, keeping build directories. Skipped when the tree
# is clean so file timestamps stay untouched for incremental builds.
reset_local_changes() {
	local repoDir="$1"
	if [ -n "$(git -C "$repoDir" status --porcelain --ignore-submodules)" ]; then
		echo "Resetting local changes in ${repoDir#$WORKSPACE/}..."
		git -C "$repoDir" reset --hard HEAD
		git -C "$repoDir" clean -fd -e Build/ -e build/
	fi
}

# Makes a commit available locally. A direct fetch of the commit is attempted first (GitHub allows
# it for reachable commits); otherwise the branch is fetched and the commit must be reachable from it.
fetch_commit() {
	local repoDir="$1" commit="$2" branch="$3"
	if git -C "$repoDir" cat-file -e "$commit^{commit}" 2>/dev/null; then
		return 0
	fi
	if ! git -C "$repoDir" fetch --depth=1 origin "$commit" 2>/dev/null; then
		echo "Direct fetch of $commit not permitted by remote, fetching $branch instead..."
		git -C "$repoDir" fetch --depth=50 origin "$branch"
		if ! git -C "$repoDir" cat-file -e "$commit^{commit}" 2>/dev/null; then
			git -C "$repoDir" fetch --unshallow origin "$branch" || git -C "$repoDir" fetch origin "$branch"
		fi
	fi
	git -C "$repoDir" cat-file -e "$commit^{commit}"
}

checkout_commit() {
	local repoDir="$1" commit="$2"
	local current
	current=$(git -C "$repoDir" rev-parse HEAD 2>/dev/null || echo "none")
	if [ "$current" != "$commit" ]; then
		echo "Checking out $commit in ${repoDir#$WORKSPACE/}..."
		git -C "$repoDir" checkout -f "$commit"
	else
		echo "${repoDir#$WORKSPACE/} already at $commit"
	fi
}

# Root: the requested commit, or the branch head.
if [ -n "${GIT_COMMIT:-}" ]; then
	fetch_commit "$WORKSPACE" "$GIT_COMMIT" "$GIT_BRANCH"
	ROOT_COMMIT="$GIT_COMMIT"
else
	echo "Fetching origin/$GIT_BRANCH..."
	git fetch --depth=1 origin "$GIT_BRANCH"
	ROOT_COMMIT=$(git rev-parse FETCH_HEAD)
fi
reset_local_changes "$WORKSPACE"
checkout_commit "$WORKSPACE" "$ROOT_COMMIT"

# Local modifications inside already-initialised submodules would make `submodule update` fail.
git submodule foreach --recursive --quiet 'echo "$toplevel/$sm_path"' | while IFS= read -r subDir; do
	reset_local_changes "$subDir"
done

# Submodules: exactly the pinned commits, recursively. A shallow update suffices when the remote
# serves the pinned commit directly; the fallback below fetches by branch for the ones it does not.
echo "Updating submodules to their pinned commits..."
if ! git submodule update --init --recursive --depth=1; then
	echo "Shallow submodule update incomplete, fetching the remaining pins by branch..."
	git submodule foreach --recursive --quiet 'echo "$name|$sm_path|$toplevel|$sha1"' | while IFS='|' read -r subName subPath subTop subSha; do
		subDir="$subTop/$subPath"
		[ -e "$subDir/.git" ] || continue
		if [ "$(git -C "$subDir" rev-parse HEAD 2>/dev/null)" = "$subSha" ]; then
			continue
		fi
		# The branch the .gitmodules entry names, else the build branch, is where the pin should be reachable from.
		subBranch=$(git config -f "$subTop/.gitmodules" "submodule.$subName.branch" || true)
		fetch_commit "$subDir" "$subSha" "${subBranch:-$GIT_BRANCH}"
		checkout_commit "$subDir" "$subSha"
	done
	git submodule update --init --recursive --depth=1
fi

# Verify the root before anything is built from it; the orchestrator refuses a build whose root differs.
ACTUAL=$(git rev-parse HEAD)
if [ "$ACTUAL" != "$ROOT_COMMIT" ]; then
	echo "::error::Root is at $ACTUAL, expected $ROOT_COMMIT"
	exit 1
fi
# foreach only visits checked-out submodules, so optional ones (update = none) are not reported.
DRIFTED=$(git submodule foreach --recursive --quiet 'test "$(git rev-parse HEAD)" = "$sha1" || echo "$displaypath"')
if [ -n "$DRIFTED" ]; then
	echo "::error::Submodules not at their pinned commit: $DRIFTED"
	exit 1
fi

echo "=== Fetch complete ==="
git log -1 --oneline
git submodule status --recursive || true
