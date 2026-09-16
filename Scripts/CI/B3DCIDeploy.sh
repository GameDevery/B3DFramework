#!/bin/bash
set -e

# Deploys a successful, tested build. The build script copies this file into the build's deploy
# directory as deploy.sh, next to the tools it runs and the files it publishes; BansheeForge
# transfers that directory to the orchestrator and runs the script there with:
#
#   DEPLOY_DIR               The transferred directory (this script's home): build-info.txt,
#                            tools/B3DUploadBinaries.sh, dependencies/*.tar.gz, framework/*.zip
#   DEPLOY_OUTPUT_DIR        Files left here are kept with the deployment as downloadable artifacts
#   RESULTS_FILE             Lines of `item<TAB>status<TAB>message` shown as a table in the UI
#   DEPLOY_CREDENTIALS_FILE  (optional) key=value file: B3D_UPLOAD_BACKEND (rclone|ftp), B3D_FTP_*
#                            or B3D_R2_* for the package server
#   FRAMEWORK_VERSION        (optional deploy parameter, vX.Y.Z) names the framework archive
#   BUILD_ID, BUILD_NUMBER, PLATFORM, GIT_BRANCH, GIT_COMMIT, TARGET_BRANCH, ...
#
# What it does: uploads every packaged dependency archive the package server lacks (never
# overwriting, never bumping versions) and, given FRAMEWORK_VERSION, renames the framework
# archive the build produced to carry that version and leaves it in DEPLOY_OUTPUT_DIR. Branch
# promotion is not part of this script; the orchestrator performs it afterwards.

: "${DEPLOY_DIR:?DEPLOY_DIR must point at the transferred deploy directory}"
: "${DEPLOY_OUTPUT_DIR:?DEPLOY_OUTPUT_DIR must name the directory for artifacts}"
RESULTS_FILE="${RESULTS_FILE:-$DEPLOY_OUTPUT_DIR/results.txt}"

echo "=== Deploy ==="
echo "Deployment: ${DEPLOYMENT_ID:-<manual>}"
echo "Build: ${BUILD_ID:-<manual>} (#${BUILD_NUMBER:-?}, ${PLATFORM:-<unknown>})"
echo "Commit: ${GIT_COMMIT:-<unknown>} on ${GIT_BRANCH:-<unknown>}"
echo "Target branch: ${TARGET_BRANCH:-<none>}"

mkdir -p "$DEPLOY_OUTPUT_DIR"
: > "$RESULTS_FILE"

# Reads key=value entries the build script wrote into build-info.txt.
build_info() {
	grep -E "^$1=" "$DEPLOY_DIR/build-info.txt" 2>/dev/null | head -1 | cut -d= -f2- | tr -d '\r'
}

record() {
	printf '%s\t%s\t%s\n' "$1" "$2" "${3:-}" >> "$RESULTS_FILE"
}

echo "::phase::validate"
if [ ! -f "$DEPLOY_DIR/build-info.txt" ]; then
	echo "::error::build-info.txt is missing from $DEPLOY_DIR; the build predates deployment support"
	exit 1
fi
UPLOADER="$DEPLOY_DIR/tools/B3DUploadBinaries.sh"
if [ ! -f "$UPLOADER" ]; then
	echo "::error::tools/B3DUploadBinaries.sh is missing from $DEPLOY_DIR"
	exit 1
fi
echo "Checkout kind: $(build_info rootKind), built from $(build_info rootCommit)"

UPLOADER_ARGS=()
if [ -n "${DEPLOY_CREDENTIALS_FILE:-}" ]; then
	if [ ! -f "$DEPLOY_CREDENTIALS_FILE" ]; then
		echo "::error::Credentials file not found: $DEPLOY_CREDENTIALS_FILE"
		exit 1
	fi
	UPLOADER_ARGS+=(--credentials "$DEPLOY_CREDENTIALS_FILE")
fi

# ---------------------------------------------------------------------------
# Dependencies: publish what the package server lacks
# ---------------------------------------------------------------------------
echo "::phase::dependencies"
shopt -s nullglob
ARCHIVES=("$DEPLOY_DIR"/dependencies/*.tar.gz)
shopt -u nullglob
if [ ${#ARCHIVES[@]} -eq 0 ]; then
	echo "No source-built dependencies to publish."
fi
FAILED=0
for archivePath in "${ARCHIVES[@]}"; do
	archiveName=$(basename "$archivePath")
	echo "--- $archiveName"
	if [ -z "${DEPLOY_CREDENTIALS_FILE:-}" ]; then
		echo "::error::No credentials file is configured (Settings -> Deployment); cannot publish $archiveName"
		record "$archiveName" failed "no credentials file configured"
		FAILED=1
		continue
	fi
	set +e
	output=$(bash "$UPLOADER" --archive "$archivePath" --if-missing "${UPLOADER_ARGS[@]}" 2>&1)
	status=$?
	set -e
	echo "$output"
	if [ $status -ne 0 ]; then
		record "$archiveName" failed "$(echo "$output" | grep -m1 '\[Error\]' || echo "uploader exit $status")"
		FAILED=1
	elif echo "$output" | grep -q "already on server"; then
		record "$archiveName" exists "already on server"
	else
		record "$archiveName" uploaded ""
	fi
done
if [ $FAILED -ne 0 ]; then
	echo "::error::One or more dependency archives failed to publish"
	exit 1
fi

# ---------------------------------------------------------------------------
# Framework archive: name it with the requested version and keep it
# ---------------------------------------------------------------------------
shopt -s nullglob
FRAMEWORK_ARCHIVES=("$DEPLOY_DIR"/framework/*.zip)
shopt -u nullglob
if [ -n "${FRAMEWORK_VERSION:-}" ]; then
	echo "::phase::framework-archive"
	if ! [[ "$FRAMEWORK_VERSION" =~ ^v[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
		echo "::error::FRAMEWORK_VERSION must look like vX.Y.Z (got '$FRAMEWORK_VERSION')"
		exit 1
	fi
	if [ ${#FRAMEWORK_ARCHIVES[@]} -eq 0 ]; then
		echo "::error::FRAMEWORK_VERSION was given but the build produced no framework archive; build with PACKAGE_FRAMEWORK enabled"
		exit 1
	fi
	for archivePath in "${FRAMEWORK_ARCHIVES[@]}"; do
		# B3DFramework-<platform>-<arch>-<buildType>.zip -> B3DFramework-<version>-<platform>-<arch>-<buildType>.zip
		archiveName=$(basename "$archivePath")
		versioned="${archiveName/B3DFramework-/B3DFramework-$FRAMEWORK_VERSION-}"
		mv "$archivePath" "$DEPLOY_OUTPUT_DIR/$versioned"
		echo "Framework archive ready for release: $versioned ($(du -h "$DEPLOY_OUTPUT_DIR/$versioned" | cut -f1))"
		record "$versioned" ready "framework $FRAMEWORK_VERSION"
	done
elif [ ${#FRAMEWORK_ARCHIVES[@]} -gt 0 ]; then
	echo "Framework archive present but no FRAMEWORK_VERSION given; not kept."
fi

echo "=== Deploy complete ==="
