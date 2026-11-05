BURBULI DASHBOARD v0.2 — VISUAL MOCKUP ONLY
Target: PocketBook PB616W / InkView 5.17 / landscape.

Changes from v0.1: four equal zones; much larger, bolder and higher clocks;
larger voltage, indoor/outdoor temperature and battery icon.
Demo values only; NO Wi-Fi and NO live battery readings. Empty battery outline is NOT SOC.
Working v0.1 is NOT replaced.

Upload two new files to the existing GitHub repository:
  src/pb616_dashboard_v02.c
  .github/workflows/dashboard_v02.yml
Run action "Build BURBULI DASHBOARD v0.2" from Actions on main.
Download artifact, unzip BURBULI_DASHBOARD_v0.2.app.
Copy into PocketBook /Application without overwriting v0.1.

Controls: 5 short presses PAGE_PREV in 5 seconds = exit.
Automatic exit: 15 minutes. Uses local clock; system sleep may interrupt.

After installation send a photo of the landscape screen to evaluate text fit.
Version is uncompiled / untested on PocketBook until GitHub run succeeds.
