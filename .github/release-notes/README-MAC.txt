zTracker Prime — macOS
=======================

WHY DOES IT SAY "zt.app IS DAMAGED AND CAN'T BE OPENED"?
--------------------------------------------------------

It is not damaged. macOS quarantines anything downloaded from the
internet that is not signed by an Apple Developer ID. zTracker Prime
is currently distributed unsigned (no $99/yr Apple Developer Program
account), so Gatekeeper refuses to launch it on first run.


HOW TO RUN IT
-------------

1. Drag zt.app out of this DMG to wherever you want it (e.g. /Applications,
   ~/Desktop, ~/Downloads — anywhere you have write access).

2. Open Terminal (Applications > Utilities > Terminal, or press Cmd+Space
   and type "Terminal").

3. Run:

       xattr -cr /path/to/zt.app

   ...replacing /path/to/ with wherever you put it. Examples:

       xattr -cr /Applications/zt.app
       xattr -cr ~/Desktop/zt.app
       xattr -cr ~/Downloads/zt.app

   Tip: you can drag zt.app onto the Terminal window and it will paste
   the full path for you.

4. Double-click zt.app. It launches normally from now on.


ALTERNATIVE (sometimes works)
-----------------------------

Right-click zt.app, choose Open, then click Open in the dialog that
appears. On some macOS versions this bypasses the quarantine without
needing Terminal. If it does not work and you still see "damaged",
fall back to the xattr -cr method above.


WHY NOT JUST SIGN AND NOTARIZE THE APP?
---------------------------------------

That requires the paid Apple Developer Program ($99/yr) plus the
Developer ID Application certificate and notarytool credentials wired
into CI. It is a planned follow-up; until then, the xattr -cr step
above is a one-time fix per copy of the app.


WHAT'S IN THIS DMG?
-------------------

  zt.app           the application (skins, docs, default config are
                   embedded inside the bundle)
  README-MAC.txt   this file
