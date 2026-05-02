#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# scripts/_gen_m3_corpus.sh — one-shot generator for the M5 T106 /
# FR-030 M3-corpus extension. Iterates every test/sema/*/pass.nsl
# fixture (the M3 Sema pass-case corpus), runs `nslc -emit=mlir`, and
# either:
#   (a) captures the raw stdout as test/Lower/m3_corpus/<sn>/pass.expected.mlir
#       and writes a paired pass.test that diffs it against fresh
#       output (FR-030 regression guard), OR
#   (b) writes an XFAIL pass.test citing the visitor STUB / Sema
#       blocker that prevents clean lowering at the M5-frozen
#       visitor surface.
#
# Run from the repo root inside the dev container:
#   bash scripts/_gen_m3_corpus.sh
#
# This is a build-time tool, not a CI step. The committed
# test/Lower/m3_corpus/* tree is the regression artifact.

set -e

NSLC=${NSLC:-./build-noasan/bin/nslc}
if [ ! -x "$NSLC" ]; then
    echo "error: $NSLC not found; build nslc first" >&2
    exit 1
fi

# Per-Sn deferral rationales (for XFAIL .test files). Source-of-truth
# for these annotations is the M5-frozen visitor: any future Sn whose
# rationale shifts requires a paired update here.
declare -A REASON
REASON[s07]="visitor STUB — SeqStmt at module-scope; nsl.seq HasParent verifier rejects non-func parent (lang.ebnf §8 + S7)"
REASON[s08]="visitor STUB — SeqStmt at module-scope; nsl.seq HasParent verifier rejects non-func parent (lang.ebnf §8 + S8)"
REASON[s09]="visitor STUB — SeqStmt at module-scope; nsl.seq HasParent verifier rejects non-func parent (lang.ebnf §8 + S9)"
REASON[s12]="visitor STUB — type-inference incomplete on partial-assignment LHS (S12)"
REASON[s16]="S16 Sema constraint surfaces during lowering — pure-NSL with parameter-bind shape correctly errors per pass-pipeline.contract.md §3"
REASON[s19]="visitor STUB — SeqStmt at module-scope; nsl.seq HasParent verifier rejects non-func parent (S19)"
REASON[s25]="visitor STUB — SeqStmt at module-scope; nsl.seq HasParent verifier rejects non-func parent (S25)"
REASON[s28]="visitor STUB — state-name resolution in nsl.first_state target attr (S28)"

PASS=0
FAIL=0

mkdir -p test/Lower/m3_corpus

for f in test/sema/*/pass.nsl; do
    sn=$(basename $(dirname "$f"))
    outdir="test/Lower/m3_corpus/$sn"
    mkdir -p "$outdir"
    expected="$outdir/pass.expected.mlir"
    testfile="$outdir/pass.test"

    if "$NSLC" -emit=mlir "$f" > "$expected" 2>/dev/null; then
        PASS=$((PASS+1))
        cat > "$testfile" <<EOT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// test/Lower/m3_corpus/$sn/pass.test — M5 T106 / FR-030 / SC-003.
// Auto-generated golden-diff harness paired with pass.expected.mlir.
// Asserts \`nslc -emit=mlir\` on the M3 Sema pass-case fixture for
// constraint $sn produces byte-identical output to the captured
// golden. Regression-guard: any visitor change that silently mutates
// lowering output for an M3 fixture flips this test red.
//
// RUN: %nslc -emit=mlir %S/../../../sema/$sn/pass.nsl > %t.actual.mlir
// RUN: diff %t.actual.mlir %S/pass.expected.mlir
EOT
    else
        FAIL=$((FAIL+1))
        rm -f "$expected"
        why="${REASON[$sn]}"
        cat > "$testfile" <<EOT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// test/Lower/m3_corpus/$sn/pass.test — M5 T106 / FR-030 / SC-003.
// XFAIL'd at HEAD: $why
//
// XFAIL: *
// RUN: %nslc -emit=mlir %S/../../../sema/$sn/pass.nsl > %t.actual.mlir
EOT
    fi
done

echo "M3 corpus extension: $PASS goldens authored, $FAIL XFAIL'd."
echo "Coverage: $PASS / $((PASS+FAIL)) = $((100*PASS/(PASS+FAIL)))%"
