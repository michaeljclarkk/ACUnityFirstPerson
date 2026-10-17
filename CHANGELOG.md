# Changelog

Reverse-chronological development notes. Newest first.

## 0.8.6 — AC Unity 1.5.1 compatibility candidate

Add head left / right yaw limit. Configured in the GUI as its own option. 180* allows full rotation. 
This Version also workds on the camera following the rolls during combat. 

## 0.8.5 — AC Unity 1.5.1 compatibility candidate

Separate fingerprint-selected 1.5.0 and 1.5.1 address profiles cover camera/menu
singletons, skeleton and visual identities, renderer refresh, ray queries, combat,
ground rolls and Leap of Faith. Unknown executables remain rejected. Renderer
refresh and leap-getter mappings were checked against loaded code from both games;
the remaining mappings follow the antr1x ACUFixes 1.5.1 port. Existing configuration
is preserved on install, with a backup of the previous DLL. Regression tests cover
profile isolation, fingerprint rejection, head tracking and the 0.9.4 loader ABI.
Live 1.5.1 startup confirmed profile selection, successful initialization and local
player camera attachment. Full gameplay validation remains pending; this is not
a published release.

## 0.8.4 RESTORE PRE-REGRESSION DIVE / DIAGNOSTICS
Default toggle key is now period (.) instead of V, which collided with Eagle Vision.
Period is added to the bindable-key allowlist; existing INIs keep their saved key.
Restores 0.8.2 dive rotation, eye offset and head tracking after the user reported
0.8.3 locks the view sideways. Keeps user-confirmed ordinary parkour smoothing.
Remaining dive shake is NOT fixed. Captures up to 600 detailed dive samples per
camera thread per game session, at most one every 16 ms, for diagnosis.
Record one manual and one automatic dive after restarting; no special toggle.

0.8.3 LEAP OF FAITH VIBRATION TEST FIX
Smooth the dive orientation in world space so native-camera heading flips near
straight down cannot shake the resulting view. Dive eye clearance uses the
resulting view frame, with a short blend into/out of the normal level offset.
Leap head position now uses the same bounded smoothing as ordinary parkour;
only ground rolls retain the close-pose override. Shadow handling is unchanged.
All eight test groups pass, including alternating 180-degree native headings
at 30/60/144 Hz. In-game confirmation pending for both manual and automatic dives.

0.8.2 PARKOUR VIBRATION TEST FIX
Ordinary head tracking now honors the animation smoothing/speed limit instead
of snapping within 8 cm of every raw pose. Verified ground rolls and Leap of
Faith retain close-pose tracking. World movement still follows immediately.
A previously valid head pose may bridge up to 150 ms of missing skeleton reads;
sustained loss still restores the native camera. No input bindings change.
Regression tests cover alternating poses at 30/60/144 Hz and tracking dropouts.
In-game verification pending: hold Space through long jumps, climbing and dive
preparation, then check the Leap of Faith and fast crouch/low-clearance movement.
Smoothing may allow more temporary head/body offset during fast ordinary poses.

0.8.1 LEAP OF FAITH CAMERA
Follow Leap of Faith defaults on, independently of combat/ground-roll options.
Uses the captured safe-airborne state plus its specific action getter (type 70h),
not general falling/jumping. During the leap the view follows the animated torso
heading/pitch rather than the native cinematic orbit; landing eases back to normal.
Head tracking, collision clearance and the user-confirmed head-shadow fix remain.
Native distance-based camera rejection is bypassed only for the identified leap.
Menu checkbox: Follow Leap of Faith. INI: FollowLeapOfFaith=0 to disable.
Live state detection was observed in a user-performed leap; on 2026-10-13 the
user confirmed the installed dive camera feels clean. Test takeoff, dive, haystack landing and
exit, then ordinary jumps/combat to verify isolation.

0.8.0 EXPERIMENTAL SELECTIVE HEAD HIDING
Replaces global head/hood visibility changes with a renderer submission flag.
Known head visuals set Visual+54 bit 1 and clear bit 2. Global hidden bit 0,
shadow/reflection candidate bits and every unrelated flag are preserved.
Original bits 1/2 are restored when first person/head hiding is disabled.
Only the verified renderer refresh methods 21E9550/21E9480 are called.
Unknown renderer implementations are left unchanged and logged.

Static runtime trace shows primary queue exclusion with separate shadow and
reflection queue gates. On 2026-10-12 the user confirmed the head shadow works
and supplied a sunlit-wall screenshot. Reflections remain unverified; other
outfits/light types still need testing.
Test a sunlit wall/ground shadow in third person then first person, turn/look
up/down, disable first person and check head restoration. Test reflective
surfaces separately: screen-space reflections may still lack hidden geometry.
Outfit changes, cutscenes and respawns need regression checks. Turn off Hide head
in main view in the menu to restore original flags if the experiment misbehaves.
No camera framing changes are included in this version.

0.7.1 COMBAT STABILIZATION
Removes animation-driven orientation from ordinary combat and deletes the fast
root-spin heuristic. These caused large attack bends and extra yaw to rotate the
view into Arno's torso in 0.7.0. Normal attacks/spins now preserve native aim.
The filtered head anchor is used throughout; the raw-head bypass is removed.
Combat framing/FOV settings are retained.

Follow recognized ground rolls (legacy INI key FollowCombatMotion) now applies
only when the reference GroundRoll Enter callback is present. There is no combat
orientation hold after that state ends; rotation eases upright immediately.
Unrecognized combat dodges stay upright. This deliberately narrows motion support
until individual dodge/attack states can be validated. Full spin following is
withdrawn. Disable the option for a fully upright camera.

Regression tests cover ordinary combat with extreme torso bends/spins producing
no camera rotation, roll gating, and smooth recovery. Gameplay testing is still
needed for clipping, individual dodge coverage and comfort.

0.7.0 EXPERIMENTAL COMBAT BODY MOTION
Follow combat rolls and spins defaults on, independently of Combat framing.
The camera follows a torso frame reconstructed from animated hips and clavicles;
eye offsets and collision-probe axes rotate with it. Full motion follows the head
without the usual positional smoothing lag. Missing torso data eases upright.
Combat-state transitions retain the existing short hold and smooth entry/exit.
The reference GroundRoll callback also enables motion outside combat.

Torso-local turns are followed directly. Fast entity turns not matched by native
view rotation contribute additional yaw, then settle to the normal view. This is
a heuristic, not individually identified dodge/attack states: other fast body
turns can also activate it. Walking/parkour retain upright view except for the
known GroundRoll state. Disable Follow combat rolls and spins in the menu and
Save & apply to restore 0.6.0's upright orientation. INI: FollowCombatMotion=0.

Tests cover synthetic mapped bones, forward/side tumbles and yaw spins through
720 degrees, inverted poses, eye offset rotation, normal matched turns, mouse-only
look and pose loss. Live animation axes and timing still need gameplay validation.
Test forward/back/side dodges, spinning attacks, normal camera turns, recovery,
combat exit and rolling next to walls. Logs add motion, torso and spin diagnostics.

0.6.0 EXPERIMENTAL COMBAT FRAMING
Combat framing defaults on while first person is enabled. Known combat state
callbacks select a lower eye (-0.06m from the tracked head), less forward offset
(0.02m) and 80-degree vertical FOV. With default exploration settings this is
18cm lower and 14cm back. Wider existing FOVs are preserved. Position and FOV
blend together, holding through 0.45-second state gaps before easing out.
The existing head tracking, native aim direction and wall clearance still apply.
This adjusts the camera, not arm bones, attack animations, reach or hit detection.

Open the First Person plugin menu: Combat framing enables/disables the feature;
Combat view exposes its three settings and last sampled combat state. Save & apply
persists them. Existing INIs use defaults for missing keys and retain other settings.
INI keys (Camera section): CombatFraming, CombatEyeHeight, CombatForward,
CombatVerticalFOV. Restart after installing the DLL or editing the INI externally.

Detection uses read-only primary state receivers and known Enter callbacks from
external/ACUFixes/ACUFixes/src/ParkourDebugging/LoggingTheHumanStates.cpp,
with layouts from HumanStatesHolder.h. Unknown/unreadable states decay back to
exploration framing. No new game hooks or skeleton writes are introduced.
Automated tests exercise detection with synthetic memory, transition timing,
dropouts, settings persistence and representative hand projection. These do not
verify live combat detection or visibility across outfits and weapon types.

IN-GAME CHECK: enable first person, enter a fight, attack/parry/dodge/quickshot,
then leave combat. Check arms/weapon visibility, smooth transitions and aiming;
repeat beside a wall and check exploration/parkour afterwards. If framing clips
through an outfit, increase Combat forward adjustment or disable Combat framing.
The log reports combat=0 (unavailable), 1 (clear), 2 (active), plus combatBlend.
Live gameplay validation remains required.

0.5.0 IN-GAME SETTINGS MENU
Open ACUFixes Plugin Loader (Insert by default), select ACUnityFirstPerson.dll's
Toggle menu, then use the First Person panel. Change toggle key captures a new
keyboard press; Escape cancels. Insert, mouse buttons and modifier-only keys are
reserved/not offered. Default V remains, with game/other-mod conflicts left to
the user. Rebinding does not change Unity's own controls.
Configure native/custom vertical FOV, eye height/forward offsets, smoothing,
head hiding, wall clearance, automatic third-person fallback, wall padding and
probe radius. Save & apply persists changes and applies them without restart.
Reset defaults fills the form; Save & apply commits them. Enabled is an immediate,
session-only toggle. A camera fault still requires restart to recover.
Save uses a temporary INI and replacement, preserving unknown keys. Failed saves
are reported and do not apply the draft. Camera writes and menu updates share a
lock. Hotkey activation is suppressed during menu input and immediately afterward.
Inactive legacy turn-limit options are omitted. Animated-head tracking remains
required; the old TrackAnimatedHead=0 option is not offered as a working mode.
Five tests pass, including actual menu draw generation in a shared ImGui context
and configuration persistence/preservation. Live in-game menu remains unverified.
Dear ImGui 1.92.0 WIP (19195) matches the installed loader version string. Its
MIT source and license are vendored in third_party/imgui; the installer includes
the license beside the DLL. No ACUFixes implementation is compiled into the mod.

0.4.5 PERSISTENT FIRST-PERSON TEST BUILD
AutoThirdPerson defaults to 0: successful local collision corrections and rear
looks no longer trigger a mode switch. Native view alignment and bounded head
tracking remain. V still switches manually. Missing head tracking and failed
collision queries still retain native camera behavior and restore head visibility.
Set AutoThirdPerson=1 to restore 0.4.4's distance/angle thresholds.
This changes fallback policy, not collision accuracy or body-follow behavior.
Extreme turns/downward views and large local corrections can still expose torso
geometry. Climbing limits and deliberate body-follow turning remain unfinished.
Four automated tests pass, including rear look plus 0.608m local correction and
tracking-loss recovery. Live testing still needed for continuous parkour.

0.4.4 LOCAL CLEARANCE EXPERIMENT
0.4.3 logs show repeated fallback/recovery within seconds during 2-3.6m
collision pullbacks. Replaced the distant native-camera ray origin with an
upper-torso origin following root XY and head height. This avoids treating
obstacles along the unrelated third-person boom as eye obstructions.
Five-ray approximation remains experimental: it cannot prove the origin is
outside geometry, nor cover all gaps between rays. Test walls/corners/landings.
Clearance must now remain good for 0.75 seconds before first person resumes.
Fallback logs include collision/rear-look/missing-head flags and exact values
at each transition. Rearward look still intentionally triggers third person;
body-follow and climbing limits are still unfinished. Movement yaw stays native.
All four automated tests pass. Native collision behavior needs live testing.

0.4.3 REGRESSION RECOVERY
Logs from 0.4.2 show persistent view/native yaw mismatch, collision pullback up
to 5.556m and head lag up to 1.393m. Removed accumulated/clamped view rotation:
yaw and pitch now match the pre-FX native view; roll remains removed.
Head smoothing is bounded to 0.08m from a valid animated head. Fast pose changes
may therefore produce more motion, but cannot accumulate metres of head lag.
If collision pullback exceeds 0.25m, view exceeds 85 degrees from body forward,
or the head is unavailable, retain native third-person output and restore head
visibility through the frame callback. Resume after 0.25s of clearance below
0.10m and yaw below 70 degrees. Hysteresis prevents boundary flicker.
This is temporary recovery behaviour, not completed first-person collision or
body-follow controls. Hard climbing limits are disabled; old MaxLook keys are
inactive. Game testing remains necessary. Existing settings are preserved.
All four automated tests pass, including native yaw alignment over repeated
revolutions, head-lag bound and fallback hysteresis. Test walking/turning first,
then looking behind, climbing and wall landings. V still toggles the mode.

0.4.2 DUPLICATE-CLAMP FIX
Corrects a missed removal in 0.4.1: ApplyEye still called the old absolute
LimitLook before the incremental controller. That reintroduced the wrap flip
and made rawYaw diagnostics report an already-clamped value. Removed both the
call and obsolete helper. The controller now receives the pre-FX orientation.
Four tests pass, including multi-revolution stops and immediate reversal.
This still needs live validation. Body-follow turning is not newly implemented.
Diagnostics now include per-interval maximum yaw step (degrees), collision
pullback (metres), and filtered-head lag (metres), sampled across every applied
frame. These distinguish possible sources of the reported partial detach.
Next test: both turn stops and reverse while standing/crouching, then climbing
and landing near a wall. Existing physics and head smoothing are unchanged.

0.4.1 TURN-LIMIT HOTFIX
Removed writes to Unity camera current/target yaw: the previous correction
could fight the engine camera. Look limits now consume wrapped angular increments,
discard excess input at the stop, and respond immediately when direction reverses.
Regression tests cover multiple revolutions, both stops, reversal and body wrapping.
Still experimental: in-game stability is unverified. Limits currently apply in all
movement states. Standing/crouching body-follow turning is NOT implemented yet.
The log records navigation receiver RVA, raw/body yaw, relative yaw, pitch and limit
state once per second while active. Unknown receiver values are not treated as ground.
Next test: turn beyond both stops and reverse while standing, crouching and hanging.

## 0.4.0 FIRST-PERSON PROTOTYPE

V toggles; starts in third person. Rebind Eagle Vision away from V.

STATUS
The user confirmed substantial improvement in 0.2 and 0.3, then reported
360-degree torso views, violent jump vibration and seeing through walls.
0.4 addresses these with a body-relative look cone, filtered animation offsets,
pre-camera-FX viewing direction, and experimental native collision rays.
All four automated tests pass. Native raycast integration and actual gameplay
behavior still require live playtesting. This is not a Nexus-ready release.

WHAT CHANGED IN 0.4
- Look limited to 80 degrees left/right, 60 down, 70 up relative to the body.
  Camera roll is removed; yaw corrections also reset accumulated game orbit.
  This does not implement independent turn-in-place body steering yet.
- Smooths animated head offset relative to the root, without delaying world
  movement. Missing pose samples hold briefly then ease toward fallback height.
  Toggle, long pause, respawn and large teleport reset the filter.
- Uses orientation from before native camera FX to reduce landing shake.
- Casts five parallel rays (centre and corners) from the native camera toward
  the requested eye position, skipping player-owned hits. Pulls back immediately
  at obstructions and eases forward as clearance returns.
  This approximates volume clearance; it is not a true swept sphere.
- Query failure preserves the native camera for the rest of the session and logs
  the error. Set CollisionGuard=0 and restart to isolate collision issues.

PRESERVED FEATURES / LIMITATIONS
Animated head anchoring and selective face, eye/teeth, eyelash, hair and Arnaud V1
hood hiding. Body meshes stay enabled. Previously visible head meshes are
restored when first person is off or suspended.
Open neck seams and third-person geometry can still appear at extreme poses.
Hiding affects shadows/reflections too; other hoods/outfits are not fully covered.
The wall guard may retract around nearby NPCs, may miss thin geometry between
rays, and has not been validated in all physics contexts.
Aiming consistency, body turning, exact cutscene detection and broader outfit
coverage remain. CamnKaboom and other camera/appearance mods may interact.

BUILD AND TEST
Visual Studio 2026 C++ desktop tools, Windows SDK, CMake 4.3+, Windows x64.
powershell -ExecutionPolicy Bypass -File scripts/build.ps1
Tests: camera geometry, plugin export/loader ABI/rejected host, synthetic
skeleton memory and camera comfort/collision math. They do not exercise the
game's physics engine or prove that in-game bugs are fixed.

INSTALL / REMOVE (game closed)
powershell -ExecutionPolicy Bypass -File scripts/install.ps1
powershell -ExecutionPolicy Bypass -File scripts/uninstall.ps1
Installer verifies executable SHA256, backs up the previous DLL and verifies
installed bytes. Settings are preserved; missing keys use compiled defaults.
Restart to update or remove. Do not hot-unload this resident prototype.

SETTINGS
See ACUnityFirstPerson.ini for documented defaults. Installed settings live in
the game's ACUFixes/plugins folder. Restart after changing values.
MaxLookYaw/Down/Up are degrees. HeadSmoothingHalfLife is seconds.
CollisionRadius and CollisionPadding are metres.
EyeHeight is fallback height; ForwardOffset is retained for older configs but
0.4 uses HeadForward for both filtered and fallback anchors.

NEXT PLAYTEST
1. Stand in open space, toggle V, look left/right/down and reverse direction.
2. Walk, sprint and make a small jump; then test stride jumps and landing.
3. Approach a wall slowly; then test wall landings, corners and windows.
4. Toggle back to third person and confirm the head returns.
Report any frozen look direction, reverse turning, remaining shake, sudden
camera pullback, frame-rate drops, crash or continued wall penetration.
The log should say "Native collision rays completed successfully."
A query-failure entry means collision disabled the first-person override.
V remains the manual third-person toggle.

IMPLEMENTATION
Independent loader ABI; no ACUFixes implementation is compiled or distributed.
Per-instance vtable clone, native visibility calls, and documented native ray ABI.
No forge repack or persistent executable patch. Compatibility gates remain.
Reference checkouts are ignored under external/.
