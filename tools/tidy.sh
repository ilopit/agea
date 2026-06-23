#!/usr/bin/env bash
# Run clang-tidy on changed C/C++ files using the curated .clang-tidy at repo root.
#
# clang-tidy on this MSVC/Windows project is special: it needs the MSVC INCLUDE
# env (system headers aren't in compile_commands' -I flags), so we run it THROUGH
# vcvars64.bat. The compile DB comes from the `host-tidy` Ninja preset, which also
# needs cl.exe on PATH — so its configure runs through vcvars too.
#
# Usage:
#   tools/tidy.sh                # changed-since-HEAD files, report only
#   tools/tidy.sh --fix          # apply auto-fixable diagnostics
#   tools/tidy.sh --force        # like --fix, but applies fixes even when a TU has a
#                                # COMPILE error (--fix-errors). Needed for libs whose
#                                # every TU transitively includes a thirdparty header
#                                # clang can't parse (e.g. sol2's optional<T&> in core).
#                                # Fixes still come from the correctly-parsed code; the
#                                # broken part is thirdparty. ALWAYS build-gate after.
#   tools/tidy.sh --all          # whole tree (slow)
#   tools/tidy.sh --configure    # (re)generate build_tidy/compile_commands.json, then run
#   tools/tidy.sh [--fix] <path>...  # scope to path(s), e.g. libs/core. Header fixes are
#                                    # restricted to the scope, so parallel per-lib --fix
#                                    # runs never touch the same header. Skips changed/all.
#
# Toolchain (clang-tidy, vcvars64.bat) is auto-discovered via vswhere — works across
# VS versions/editions/drives. Override with CLANG_TIDY=... and/or VCVARS=... if needed.
# Windows + MSVC only (uses cmd.exe + vcvars); that's inherent to the tidy compile DB.
set -euo pipefail
cd "$(git rev-parse --show-toplevel)"

DB_DIR="build_tidy"

# --- Toolchain discovery -----------------------------------------------------
# No hardcoded VS version/edition/drive. vswhere.exe lives at a fixed,
# version-independent location and reports the install path; everything derives
# from that. CLANG_TIDY / VCVARS env vars override discovery entirely.
find_vswhere() {
  local pf86
  pf86="$(cygpath -u "$(printenv 'ProgramFiles(x86)' 2>/dev/null)" 2>/dev/null || true)"
  [[ -n "$pf86" ]] || pf86="/c/Program Files (x86)"
  local vw="$pf86/Microsoft Visual Studio/Installer/vswhere.exe"
  [[ -x "$vw" ]] && { printf '%s\n' "$vw"; return 0; }
  command -v vswhere.exe 2>/dev/null
}

VS_INSTALL=""
vswhere_exe="$(find_vswhere || true)"
if [[ -n "$vswhere_exe" ]]; then
  VS_INSTALL="$("$vswhere_exe" -latest -products '*' \
      -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 \
      -property installationPath 2>/dev/null | tr -d '\r')"
fi

# vcvars64.bat — kept as a Windows path (consumed inside the cmd batch).
if [[ -z "${VCVARS:-}" ]]; then
  [[ -n "$VS_INSTALL" ]] || { echo "ERROR: no VS+MSVC install found via vswhere. Set VCVARS=." >&2; exit 1; }
  VCVARS="$VS_INSTALL\\VC\\Auxiliary\\Build\\vcvars64.bat"
fi

# clang-tidy — PATH first (standalone LLVM), else the copy bundled with the VS install.
if [[ -z "${CLANG_TIDY:-}" ]]; then
  CLANG_TIDY="$(command -v clang-tidy 2>/dev/null || true)"
  if [[ -z "$CLANG_TIDY" && -n "$VS_INSTALL" ]]; then
    cand="$(cygpath -u "$VS_INSTALL")/VC/Tools/Llvm/x64/bin/clang-tidy.exe"
    [[ -x "$cand" ]] && CLANG_TIDY="$cand"
  fi
fi
[[ -n "$CLANG_TIDY" ]] || { echo "ERROR: clang-tidy not found. Set CLANG_TIDY= or install the VS 'C++ Clang tools'." >&2; exit 1; }

FIX="" ; ALL=0 ; CONFIGURE=0 ; SCOPES=()
for arg in "$@"; do
  case "$arg" in
    --fix) [[ "$FIX" == "--fix-errors" ]] || FIX="--fix" ;;
    --force) FIX="--fix-errors" ;;   # apply fixes despite thirdparty compile errors
    --all) ALL=1 ;;
    --configure) CONFIGURE=1 ;;
    --*) echo "unknown flag: $arg" >&2; exit 2 ;;
    *) SCOPES+=("$arg") ;;   # path scope(s), e.g. libs/core — see collection below
  esac
done

# Windows path of vcvars for cmd; tolerate it being given in either slash style.
WIN_VCVARS="${VCVARS//\//\\}"

run_in_vcvars() {  # runs "$@" as a single command line inside the VS x64 env
  local bat; bat="$(mktemp --suffix=.bat)"
  # NOTE: no '|| exit' guard on vcvars — it leaves a nonzero errorlevel from a
  # harmless vswhere warning even on success, which would abort the batch.
  { echo '@echo off'
    echo "call \"$WIN_VCVARS\" >nul"
    echo "$@"
  } > "$bat"
  cmd.exe //C "$(cygpath -w "$bat")"
  local rc=$?; rm -f "$bat"; return $rc
}

if [[ "$CONFIGURE" -eq 1 || ! -f "$DB_DIR/compile_commands.json" ]]; then
  if [[ "$CONFIGURE" -eq 0 ]]; then
    echo "error: $DB_DIR/compile_commands.json missing. Re-run with --configure." >&2
    exit 1
  fi
  echo "Configuring '$DB_DIR' (Ninja, host-tidy preset)..."
  run_in_vcvars "cmake --preset host-tidy" || { echo "configure failed" >&2; exit 1; }
fi

# Collect targets; drop build artifacts, thirdparty, and argen-generated TUs.
# HF overrides the config's HeaderFilterRegex so a scoped run only FIXES headers
# inside that scope — this is what makes parallel per-lib --fix safe (each scope
# owns its own headers; no two runs write the same header).
HF=""
if [[ ${#SCOPES[@]} -gt 0 ]]; then
  # Scoped: every .cpp TU under the given path(s). Headers fixed via HF below.
  mapfile -t FILES < <(git ls-files -- "${SCOPES[@]}" | grep -E '\.cpp$' || true)
  parts=()
  for s in "${SCOPES[@]}"; do s="${s%/}"; parts+=("${s//\//[/\\\\]}"); done
  IFS='|'; HF=".*(${parts[*]})[/\\\\].*"; unset IFS
elif [[ "$ALL" -eq 1 ]]; then
  mapfile -t FILES < <(git ls-files -- '*.h' '*.cpp')
else
  mapfile -t FILES < <( { git diff --name-only HEAD -- '*.h' '*.cpp'; \
                          git ls-files --others --exclude-standard -- '*.h' '*.cpp'; } | sort -u )
fi
FILTERED=()
for f in "${FILES[@]}"; do
  [[ "$f" == build/* || "$f" == build_tidy/* || "$f" == thirdparty/* || "$f" == *.ar.h || "$f" == *.ar.cpp ]] && continue
  FILTERED+=("$f")
done

if [[ ${#FILTERED[@]} -eq 0 ]]; then
  echo "No C/C++ files to check."
  exit 0
fi

echo "clang-tidy: ${#FILTERED[@]} file(s)${FIX:+ (--fix)}"
# Build the clang-tidy command line for the in-vcvars cmd run. The exe needs a real
# Windows path (cygpath) — a naive /->\ swap yields '\c\Program Files\...' which cmd
# can't find. File args stay relative (clang-tidy accepts forward slashes fine).
CT_WIN="$(cygpath -w "$CLANG_TIDY")"
# Force our root .clang-tidy for every file so clang-tidy never walks into a nested
# thirdparty .clang-tidy (e.g. spdlog's stale 'AnalyzeTemporaryDtors' key, which
# errors out and blocks fixes for any TU that includes it).
CFG_WIN="$(cygpath -w "$PWD/.clang-tidy")"
CMD="\"$CT_WIN\" -p $DB_DIR --config-file=\"$CFG_WIN\" ${FIX}"
[[ -n "$HF" ]] && CMD+=" --header-filter=\"$HF\""
# These checks produce BUILD-BREAKING auto-fixes and must never be applied via --fix:
#  - identifier-naming renames third-party symbols we forward-declare (JPH::BodyID,
#    Json::Value, YAML::Node) and GTest contract methods (SetUp/TearDown/PrintTo).
#  - macro-parentheses parenthesizes macro *return types* (KRG_gen_getter), invalid C++.
# They stay active as DIAGNOSTICS in report mode (no --fix); only suppressed when fixing.
#  - use-ranges rewrites remove_if/unique to std::ranges:: which return a subrange,
#    breaking the erase-remove idiom (erase(it,end) no longer compiles).
# NOTE: even with these off, --fix is heuristic and CAN break the build — always
# build-verify after fixing (ideally per-lib), and revert the lib if it fails.
#  - macro-to-enum converts #define groups to anonymous enums; in the shared
#    gpu_types/* headers GLSL then chokes ("'enum': Reserved word"). The C++ build
#    can't catch this — only the shader cook does — so it must never auto-apply.
#  - move-const-arg strips std::move on trivially-copyable handles (e.g. VkImageView),
#    but the move is what selects an rvalue-ref overload — removing it breaks overload
#    resolution (create_shared -> create(VkImageView&&)).
[[ -n "$FIX" ]] && CMD+=" --checks=-readability-identifier-naming,-bugprone-macro-parentheses,-modernize-use-ranges,-modernize-macro-to-enum,-cppcoreguidelines-macro-to-enum,-performance-move-const-arg,-hicpp-move-const-arg"
for f in "${FILTERED[@]}"; do CMD+=" \"$f\""; done
run_in_vcvars "$CMD"
