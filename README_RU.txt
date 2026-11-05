BURBULI DASHBOARD v0.4.1_TALLDIGITS_FIX1 / PB616W

Single purpose: fix GitHub Actions compile/link error:
  DrawEllipse() is not declared/available in this InkView SDK.

Only program change: both degree rings now use draw_degree_ring()
implemented with supported DrawLine(). Clock/data layout unchanged.
Version tag at lower right updated to v0.4.1.

Demo test: real clock/date, synthetic temperatures/voltage.

ADD NEW FILES to the existing repo (do not remove older revisions):
  src/pb616_dashboard_v041_fix1.c
  .github/workflows/dashboard_v041_fix1.yml

Actions -> Build BURBULI DASHBOARD v0.4.1 -> Run workflow.
Output artifact: BURBULI_DASHBOARD_v0.4.1_TALLDIGITS_FIX1_APP
Inside artifact: BURBULI_DASHBOARD_v0.4.1_TALLDIGITS_FIX1.app
Install under PocketBook Application alongside older .app files.

Exit: 5x PAGE_PREV within 5 seconds, or automatic exit 15min.

This archive is validated structurally; GitHub cross-compilation is not verified here.
