#!/usr/bin/env bash
# Quick local smoke test: M5 round-trip on every Phase 3 US1 fixture.
# Phase 3 helper; not committed long-term.
set -e
cd /workspace
for f in test/Lower/module/empty_module_emit_mlir.nsl \
         test/Lower/decl/regdecl_emit_mlir.nsl \
         test/Lower/decl/wiredecl_emit_mlir.nsl \
         test/Lower/decl/memdecl_emit_mlir.nsl \
         test/Lower/decl/procdefn_emit_mlir.nsl \
         test/Lower/decl/funcdefn_emit_mlir.nsl \
         test/Lower/decl/firststate_emit_mlir.nsl \
         test/Lower/action/parallelblock_emit_mlir.nsl \
         test/Lower/stmt/barefinishstmt_emit_mlir.nsl \
         test/Lower/stmt/systemtaskstmt_finish_emit_mlir.nsl \
         test/Lower/stmt/systemtaskstmt_display_emit_mlir.nsl \
         test/Lower/stmt/systemtaskstmt_init_emit_mlir.nsl \
         test/Lower/stmt/systemtaskstmt_delay_emit_mlir.nsl; do
  echo "=== $f ==="
  build-noasan/bin/nslc -emit=mlir "$f" > /tmp/a.mlir 2>&1 || { echo "  nslc FAIL"; continue; }
  build-noasan/bin/nsl-opt /tmp/a.mlir > /tmp/b.mlir 2>&1 || { echo "  nsl-opt FAIL: $(cat /tmp/b.mlir)"; continue; }
  if diff -q /tmp/a.mlir /tmp/b.mlir > /dev/null; then
    echo "  ROUND-TRIP PASS"
  else
    echo "  ROUND-TRIP DIFF:"
    diff /tmp/a.mlir /tmp/b.mlir | head -10
  fi
done
