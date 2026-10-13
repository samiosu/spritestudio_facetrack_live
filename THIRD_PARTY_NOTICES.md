# Third-party notices

SpriteFace's original code is licensed under the root [MIT license](LICENSE).
Third-party code, fonts, models and tools keep their own licenses and copyrights.
Distribute this file together with the complete [`licenses/`](licenses/) directory
and the root `LICENSE` when distributing SpriteFace binaries. Do not replace
upstream licenses with SpriteFace's MIT license.

The exact sources, reviewed versions, copyright excerpts and SHA-256 hashes of
the retained license files are recorded in [`licenses/manifest.json`](licenses/manifest.json).
The manifest covers reviewed direct dependencies and retained notices. It is not
a complete SBOM for a platform release. A hash of a source license or header is
not a hash of the complete package or executable.

## Desktop runtime

| Component | Version | License / retained text |
| --- | --- | --- |
| SpriteStudio6-SDK core | 2.3.1, commit `8e5ea72d21eae5f143474691fbca05dacc45feb6` | [BSD 3-clause-style upstream license](licenses/SpriteStudio6-SDK-LICENSE.txt), Copyright (c) CRI Middleware Co., Ltd. Preserve the upstream wording, including its Web Technology Corp. endorsement clause. |
| TinyXML2, SDK copy | SDK 2.3.1 | [zlib notice](licenses/TinyXML2-LICENSE.txt), original code by Lee Thomason. |
| MersenneTwister, SDK copy | SDK 2.3.1 | [MIT notice](licenses/MersenneTwister-LICENSE.txt), Copyright (C) 1997–2002 Makoto Matsumoto and Takuji Nishimura. |
| GLEW, SDK copy | 2.2.0 | [Complete upstream license file](licenses/GLEW-LICENSE.txt), including GLEW BSD-style terms and Mesa / Khronos MIT notices. Copyright Milan Ikits, Marcelo E. Magallon, Lev Povalahev, Brian Paul and The Khronos Group Inc. |
| GLFW | 3.4 | [zlib license](licenses/GLFW-LICENSE.txt), Copyright (c) 2002–2006 Marcus Geelnard; Copyright (c) 2006–2024 Camilla Löwy. |
| Dear ImGui | 1.91.9b | [MIT license](licenses/DearImGui-LICENSE.txt), Copyright (c) 2014–2025 Omar Cornut. |
| Dear ImGui's modified stb headers | ImGui 1.91.9b copies | MIT alternative selected; complete [rect packing](licenses/ImGui-imstb_rectpack.h-LICENSE.txt), [font rasterizer](licenses/ImGui-imstb_truetype.h-LICENSE.txt), and [text editor](licenses/ImGui-imstb_textedit.h-LICENSE.txt) license blocks retained. Copyright (c) 2017 Sean Barrett. |
| Embedded ProggyClean.ttf | ImGui 1.91.9b default font | [MIT license](licenses/ProggyClean-LICENSE.txt), Copyright (c) 2004, 2005 Tristan Grimmer. No separately downloaded Japanese UI font is included. |
| nlohmann/json | 3.11.3 | [MIT license](licenses/NlohmannJson-LICENSE.txt) and [embedded contributor notices](licenses/NlohmannJson-NOTICES.txt). Preserve the credits for Niels Lohmann, Evan Nemerson, The Abseil Authors, Björn Hoehrmann and Florian Loitsch. |
| Abseil utility excerpt in nlohmann/json | upstream commit `10cb35e459f5ecca5b2ff107635da0bfa41011b4` | [Apache-2.0 license](licenses/Abseil-LICENSE.txt) retained with json's provenance comments. The C++14-or-newer build uses the corresponding standard-library helpers. |
| Nayuki QR Code generator, C++ | 1.8.0 | [MIT license block](licenses/QRCodeGenerator-LICENSE.txt), Copyright (c) Project Nayuki. |
| stb_image | commit `2c980bb59875b0d32144a71867fbdebb2f77cd20` | [MIT / public-domain alternatives](licenses/STB-LICENSE.txt); MIT selected. Copyright (c) 2017 Sean Barrett. The SDK's older stb implementation is not compiled. |

The SDK is copied into the build directory before applying SpriteFace's face-value
hook and desktop compatibility changes. Its `getFullPath` helper is also replaced
with UTF-8 `std::filesystem` directory resolution, removing fixed-size path
buffers and process working-directory changes while retaining its final
directory separator. Those are modified SDK sources; the
upstream notice stays in place and the patch/build scripts accompany the source.
The SDK's JUCE-based Viewer and converters are not part of the SpriteFace build.
SpriteStudio's proprietary editor, SDK sample artwork and users' character assets
are not redistributed by this repository.

## Mobile runtime and face model

| Component | Version | License / retained text |
| --- | --- | --- |
| MediaPipe Tasks Vision, Android | 0.10.32 | [Complete upstream license](licenses/MediaPipe-Android-LICENSE.txt), including its additional upstream notice; [source copyright header](licenses/MediaPipe-Android-NOTICE.txt), Copyright 2023 The MediaPipe Authors. |
| MediaPipeTasksVision / MediaPipeTasksCommon, iOS | 0.10.21 | [Complete upstream license](licenses/MediaPipe-iOS-LICENSE.txt); [source copyright header](licenses/MediaPipe-iOS-NOTICE.txt), Copyright 2023 The MediaPipe Authors. |
| CameraX core / camera2 / lifecycle / view | 1.4.2 | [Apache-2.0 license from the versioned source artifact](licenses/CameraX-LICENSE.txt) and [copyright header](licenses/CameraX-NOTICE.txt), The Android Open Source Project. |
| AndroidX activity-ktx / activity | 1.10.1 | [Apache-2.0 license from the versioned source artifact](licenses/AndroidX-Activity-LICENSE.txt) and [copyright header](licenses/AndroidX-Activity-NOTICE.txt), The Android Open Source Project. |
| Kotlin standard library | resolved 2.1.20; JDK compatibility artifacts 1.8.20 | [Apache-2.0 license](licenses/Kotlin-LICENSE.txt). The release dependency graph records each resolved runtime version. |
| ZXing core | 3.5.3 | [Apache-2.0 license](licenses/ZXing-LICENSE.txt), [core copyright header](licenses/ZXing-core-NOTICE.txt), and complete [upstream NOTICE](licenses/ZXing-NOTICE.txt). Copyright 2007 ZXing authors; upstream NOTICE also credits Barcode4J and JCommander. |
| Face Landmarker model bundle | official `float16/1` artifact, downloaded separately | Apache-2.0 according to the official component model cards: [BlazeFace](licenses/ModelCard-BlazeFace.txt), [Face Mesh V2](licenses/ModelCard-FaceMeshV2.txt), [Blendshape V2](licenses/ModelCard-BlendshapeV2.txt). The downloader pins the model file's SHA-256. |

Model cards are complete text extractions of the official PDFs; their diagrams
are omitted. Original PDF URLs and SHA-256 hashes are retained in the manifest.
Use the model's Apache-2.0 grant, not the application code's MIT license.

The resolved Android release dependency graph is retained in
[`mobile/android/docs/dependencies.txt`](mobile/android/docs/dependencies.txt).
Original embedded AAR license files are retained in
[`licenses/android-embedded/`](licenses/android-embedded/), including the Dagger
Apache-2.0 notice embedded in transport-runtime 3.1.0. Further reviewed runtime
dependencies include:

| Component | Resolved version | Retained license / attribution |
| --- | --- | --- |
| protobuf-javalite | 4.26.1 | [BSD-3-Clause](licenses/Protobuf-LICENSE.txt), Copyright 2008 Google Inc. |
| Guava / failureaccess | 27.0.1-android / 1.0.1 | [Apache-2.0](licenses/Guava-LICENSE.txt), [Guava notices](licenses/android-transitive/Guava-NOTICES.txt), [failureaccess notices](licenses/android-transitive/GuavaFailureAccess-NOTICES.txt). |
| Checker compatibility qualifier annotations | 2.5.3 | MIT for annotations and cleanroom annotation aliases, as specifically granted in the [full upstream license](licenses/CheckerFramework-LICENSE.txt). The rest of the Checker Framework's GPL is not the license chosen for this annotation artifact. |
| JSR305 | 3.0.2 | Maven POM declares Apache-2.0, but four `javax.annotation.concurrent` source files explicitly use [CC-BY-2.5](licenses/CC-BY-2.5-legalcode.html). Preserve [Copyright (c) 2005 Brian Goetz and source notices](licenses/android-transitive/JSR305-NOTICES.txt), the license link, and the official home [jcip.net](http://www.jcip.net). Those annotation classes are included without source modification. |
| Animal Sniffer annotations | 1.17 | [MIT source notice](licenses/android-transitive/AnimalSnifferAnnotations-NOTICES.txt), Copyright (c) 2008 Kohsuke Kawaguchi and codehaus.org. |
| Kotlin coroutines Android / core-jvm | 1.7.3 | [Apache-2.0](licenses/KotlinCoroutines-LICENSE.txt). |
| JetBrains annotations | 23.0.0 | Apache-2.0 and [source notices](licenses/android-transitive/JetBrainsAnnotations-NOTICES.txt). |
| Error Prone annotations | 2.2.0 | Apache-2.0 and [source notices](licenses/android-transitive/ErrorProneAnnotations-NOTICES.txt). |
| J2ObjC annotations | 1.1 | Apache-2.0 and [source notices](licenses/android-transitive/J2ObjCAnnotations-NOTICES.txt). |
| AutoValue annotations | 1.6.3 | Apache-2.0 and [source notices](licenses/android-transitive/AutoValueAnnotations-NOTICES.txt). |
| Flogger / system backend | 0.6 | Apache-2.0 and [source notices](licenses/android-transitive/Flogger-NOTICES.txt). |
| javax.inject | 1 | Apache-2.0 and [source notices](licenses/android-transitive/JavaxInject-NOTICES.txt). |
| Google datatransport and Firebase encoders | transport-api 3.0.0; runtime/backend-cct 3.1.0; encoders 17.0.0; proto 16.0.0; json 18.0.0 | Apache-2.0 as declared in their resolved Maven metadata; embedded notices retained in `licenses/android-embedded/`. |

SpriteFace modifies MediaPipe tasks-core 0.10.32's
`TasksStatsLoggerFactory.class` during the Android build to select the bundled
dummy statistics logger. This disables the SDK's telemetry logging. The upstream
artifact is unchanged in the dependency cache; the application contains modified
bytecode. See the [specific change and verification](mobile/android/docs/mediapipe-telemetry.md).
Retain this modification notice with the mobile distribution.

Keep each mobile artifact's own embedded licenses and generated dependency
acknowledgements. MediaPipe packages include inference and other transitive
components; the tables above are not a claim that every transitive component is
Apache-2.0. MediaPipe's reviewed Android AAR contains JNI binaries without an
embedded native third-party license inventory; CameraX core also contains native
binaries. Complete attribution for the libraries compiled into those native
binaries remains a release prerequisite. Preserve and review their original
notices in the actual AARs, frameworks and final application before publishing
a mobile binary.

Apple system frameworks and the Android platform SDK are system prerequisites,
not OSS libraries copied into this repository.

## Development tools and tests

The vendored Gradle 8.11.1 wrapper is Apache-2.0. Its original script copyright
headers and the jar's [complete embedded license](licenses/GradleWrapper-LICENSE.txt)
are retained. The Gradle development distribution has its own [complete license
and dependency notices](licenses/Gradle-LICENSE.txt); it is downloaded for building
and is not part of the application runtime.

The Kotlin compiler / Gradle plugin 2.1.20 is Apache-2.0. Android Gradle Plugin
8.9.2 is an external development dependency. JUnit 4.13.2 is used only by Android
JVM tests and retains its [EPL-1.0 license](licenses/JUnit-LICENSE.txt). CMake,
Python, CocoaPods and XcodeGen are externally installed build tools; they are not
redistributed as part of SpriteFace. Tool dependencies must be audited separately
if a future release starts bundling build environments.

## External applications

OpenSeeFace is installed separately and communicates with SpriteFace's Python
adapter over a local UDP interface. No OpenSeeFace code, models, Python runtime,
camera DLL or inference binaries are bundled. Its [BSD-2-Clause license](licenses/OpenSeeFace-LICENSE.txt)
is retained for reference, at commit
`50c09741897010faad721a218b2bbe14ecad6251`. The models are described by upstream
as BSD-2-Clause; this does not cover every file in an OpenSeeFace distribution.
Bundling it later requires separate review of libdshowcapture
(LGPL-2.1-or-later), the selected OpenCV / FFmpeg build, ONNX Runtime, NumPy,
Pillow, Python and their transitive dependencies. Keep the corresponding sources
and replacement / relinking facilities wherever the applicable LGPL requires them.

OBS Studio is a separately installed GPL-2.0-or-later application used for normal
window capture. SpriteFace neither links OBS libraries nor redistributes OBS.
Any future OBS plugin or bundled OBS distribution must be evaluated under its
own license and supplied with the required corresponding source.

GPL and LGPL components are permitted project choices. If future code is linked
or combined with such a component, choose a compatible license for the combined
distribution and provide the exact corresponding source and build materials
required by that version. Publishing unrelated source on GitHub alone is not
the compliance procedure. Apache-2.0 is compatible with GPLv3, but not
GPLv2-only; evaluate the specific combination rather than relabeling upstream
files.
