# MediaPipe Android telemetry modification

The Maven artifact `com.google.mediapipe:tasks-core:0.10.32` is Apache-2.0 licensed.
Its published `TasksStatsLoggerFactory.create(Context, String, String)` selects
`TasksStatsProtoLogger`, which sends app ID/version and usage/performance events
through Google DataTransport. This differs from a naive assumption that the
presence of the upstream `TasksStatsDummyLogger` means the package uses no telemetry.

**SpriteFace modifies `com/google/mediapipe/tasks/core/logging/TasksStatsLoggerFactory.class`
at build time.** The AGP ASM transformation in `app/build.gradle.kts` replaces the
body of its static `create` method with a call to the existing
`TasksStatsDummyLogger.create`. The dummy implementation's methods are no-ops.
Inference code, native libraries, models, and camera frames are unchanged.

This change applies to both debug and release APKs. The factory's expected
descriptor and static access are checked; an unexpected signature, duplicate
entry point, or missing method fails instrumentation. `tasks-core` remains pinned
to 0.10.32 through the pinned `tasks-vision` dependency. Any dependency upgrade
requires a new artifact and output-bytecode audit, including verifying that this
factory still exists and the dummy logger still does nothing. We retain the
upstream license files and this modification notice.

The `verifyMediaPipeLogger` prerequisite additionally checks the resolved artifact
version, both required class entries, the original factory signature, and that
the dummy logger's reporting methods only return without calling methods or
accessing fields. This detects a factory class disappearing before instrumentation.

Google DataTransport remains a transitive dependency for binary compatibility.
The app does not initialize or call it. Camera images are processed on the phone;
SpriteFace sends only the normalized facial parameters to the paired PC.
Build-time bytecode inspection verifies the factory substitution; this does not
replace a device-level traffic capture when qualifying a public release.
The captured output of the current factory is in
[`mediapipe-factory-bytecode.txt`](mediapipe-factory-bytecode.txt).

To inspect AGP's instrumented output after a build, locate the transformed JAR
containing `TasksStatsLoggerFactory.class` under Gradle's `caches/.../transforms/`
and disassemble it with JDK `javap -c`. The factory must call
`TasksStatsDummyLogger.create`, with no call to `TasksStatsProtoLogger.create`.
The original Maven AAR in `caches/modules-2/files-2.1/` remains unmodified.
