# Dynamics calibration v17

V17 limits the combined normalized elevator-stick and trim demand to [-1, 1]. Previously full stick plus trim could exceed the model's nominal maximum control authority. Small inputs retain the previous behavior. This is a correction to the aggregate control model; it is not a mechanical simulation of Cirrus trim tabs, control forces or asymmetric surface travel.

The installed Laminar definitions confirm that comparisons need matched conditions. The SR22 ACF specifies a 3600-lb maximum, whereas Canyon's normally aspirated SR22 baseline remains 3400 lb. Both have nominal 310-hp engines. The SF50 ACF engine cap is interpreted as 1950 lbf versus Canyon's approximately 1846 lbf. Neither cap alone defines climb performance, an installed thrust deck or the MCT schedule. Raw zero unit-mass inertia fields are not usable physical inertias. No X-Plane aircraft assets are included in this update.

## Test coverage

The compiled ARM phase suite now contains 55 scenarios and 185 checks, with zero failures:

- The previous 31 dynamic phase and ground-handling scenarios remain in place.
- A further 24 static cases cover both aircraft, two masses, sea level/2000/6000 ft and ISA/ISA+20 C. SR22 masses are 3400/3600 lb; SF50 masses are 5550/6000 lb.
- Checks verify finite forces, positive thrust, lower hot-day thrust at the same true airspeed and engine setting, mass-dependent stall scaling, and the ideal coordinated 60-degree-bank stall-speed relationship. These are model consistency checks, not independent performance measurements.
- Elevator checks exercise positive/negative stops, both aircraft and unsaturated trim authority.

The static mass/temperature cases do not simulate complete takeoffs or climb trajectories. The product retains its existing mass and weather controls; the matrix does not introduce a loading menu or hot-day preset. Selected SR22 POH climb and glide checks still pass. SF50 MCT calibration, actual inertias, loading-dependent CG, trim mechanics and runtime reference measurements remain outstanding.

## Flight logs and matched testing

Existing controls are unchanged: ZL+SELECT selects/restarts a phase, ZL+Y starts/stops recording; stopping pauses and saves the log. See PHASES_V16.md for the complete workflow.

V17 CSVs append six columns after the previous 34: `isa_offset_c`, `thrust_n`, `p_rad_s`, `q_rad_s`, `r_rad_s`, `effective_elevator`. Body rates use x forward, y right, z down. Effective elevator is the saturated stick-plus-trim demand; it is normalized, not degrees. Thrust is modeled Newtons; engine remains a normalized lagged command, not measured N1 or shaft power. Logged dynamics diagnostics can precede the final integrated state by one physics step. The model-version field is 17. The analyzer accepts both v16 and v17 files.

The recorder remains bounded at 4096 samples and 5 Hz. The extra fields add 96 KiB to its static RAM allocation. Sampling does not write to SD. Recorder tests check the actual CSV formatter and new values, plus capacity and save failures; the sample CSV is a synthetic fixture, not a flight capture.

For X-Plane comparison, match actual total weight, altitude, ISA deviation, wind, gear/flap configuration and engine rating first. Record steady trim/climb/glide, then control doublets and stall recovery. Do not fit SF50 maximum takeoff thrust against MCT tables. No X-Plane runtime trace or New 3DS frame-rate measurement was obtained for this release.

## Install

Copy the contents of `sdmc/` from the v17 update onto the existing SD card, replacing the app and SMDH. It reuses the v15/v16 scenery. Keep the scenery directory installed. The previous v16 release archives are retained unchanged.
