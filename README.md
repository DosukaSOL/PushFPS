<div align="center">

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="assets/pushfps-logo-animated-dark.png">
  <source media="(prefers-color-scheme: light)" srcset="assets/pushfps-logo-animated-light.png">
  <img src="assets/pushfps-logo-animated-dark.png" alt="PushFPS: the logo slides in with a trail of generated frames while a counter climbs from 30 to 120 FPS" width="480">
</picture>

### Play 30 FPS console games at 60 or 120 FPS on your PC monitor.

PushFPS takes the picture from your capture card and adds AI-generated frames in real time.<br>
Nothing changes on the console or in the game: you need a capture card and a graphics card.

[![Download for Windows](https://img.shields.io/badge/Download-Windows%2011-89F52A?style=for-the-badge&logo=windows&logoColor=0D1A04&labelColor=89F52A)](https://github.com/DosukaSOL/PushFPS/releases)
&nbsp;
[![Linux and macOS preview](https://img.shields.io/badge/Linux%20%26%20macOS-preview-1b1b20?style=for-the-badge)](#linux-and-macos-preview)
&nbsp;
[![OBS Studio plugin](https://img.shields.io/badge/OBS%20Studio-plugin-1b1b20?style=for-the-badge)](#obs-studio-plugin)
&nbsp;
[![Website](https://img.shields.io/badge/Website-dosukasol.github.io%2FPushFPS-1b1b20?style=for-the-badge)](https://dosukasol.github.io/PushFPS/)
&nbsp;
[![Discord](https://img.shields.io/badge/Discord-join%20us-1b1b20?style=for-the-badge&logo=discord&logoColor=white)](https://discord.gg/N88znNpSgC)

[![Latest release](https://img.shields.io/github/v/release/DosukaSOL/PushFPS?include_prereleases&style=flat-square&color=89F52A&label=release)](https://github.com/DosukaSOL/PushFPS/releases)
[![Build](https://img.shields.io/github/actions/workflow/status/DosukaSOL/PushFPS/build-windows.yml?branch=main&event=push&style=flat-square&label=build)](https://github.com/DosukaSOL/PushFPS/actions/workflows/build-windows.yml)
[![License: GPL-3.0](https://img.shields.io/badge/license-GPL--3.0-89F52A?style=flat-square)](LICENSE)
[![Built on Veyra](https://img.shields.io/badge/built%20on-Veyra%20by%20Likely7-89F52A?style=flat-square)](https://github.com/Likely7/Veyra-NRVideo)

<sub>Built on <a href="https://github.com/Likely7/Veyra-NRVideo">Veyra</a> by Likely7 · ANR frame generation by <a href="https://apexcompute.com">Apex Compute</a></sub>

<sub>Questions, setups and clips: <a href="https://discord.gg/N88znNpSgC">join the PushFPS Discord</a></sub>

<br>

<img src="assets/screenshots/home.png" alt="PushFPS home screen: capture card, quick setup with console and game profiles" width="860">

</div>

---

## Why PushFPS

<table>
<tr>
<td width="33%" valign="top">

**Smoother console games**<br>
A 30 FPS game becomes 60 or 120 FPS, or matches your monitor. PushFPS finds the real frames inside the
60 Hz signal and generates the ones in between on your GPU.

</td>
<td width="33%" valign="top">

**Set up in a minute**<br>
Pick your console, your game's frame rate and the frame rate you want. Save it as a game profile and
switch back in one click.

</td>
<td width="33%" valign="top">

**Frame generation, with or without AI enhancement**<br>
Generate frames only, or also run every frame through AI picture enhancement for a sharper picture
(NVIDIA RTX and Radeon RX 7000 / 9000 cards, as an [optional extra](#what-you-need)).

</td>
</tr>
<tr>
<td valign="top">

**Any capture card, NVIDIA, AMD or Intel graphics**<br>
Works with capture cards that Windows sees as a camera, from 1080p60 USB sticks to 4K PCIe cards.
RTX 30, 40 and 50 series get NVIDIA's methods; every DirectX 12 card gets PushFlow (2× to 4×) and FSR (2×).

</td>
<td valign="top">

**Streams too**<br>
The OBS Studio plugin puts the PushFPS picture, generated frames included, straight into your scenes.

</td>
<td valign="top">

**Free and open source**<br>
GPL-3.0, no account needed. Built on Veyra, the open-source video enhancer by Likely7.

</td>
</tr>
</table>

## Only in PushFPS

PushFPS is built on [Veyra](https://github.com/Likely7/Veyra-NRVideo), a general video enhancer. On top of
that, PushFPS adds what console players need:

| Feature | What it does |
|---|---|
| **PushFlow** | PushFPS's own frame generation: the RIFE neural network on DirectML, on NVIDIA, AMD and Intel graphics cards, 2× to 4×, Fast / Balanced / Quality tiers ([more](#pushflow-our-own-frame-generation)) |
| **Frame quality test** | Holds back every second real frame, lets frame generation rebuild it and compares it with the original: accuracy (PSNR), pixel errors, HUD smearing, gain over a simple blend, whether generated frames are on time, and (since 3.2.7) the worst frame and the worst place, where an object missing from a generated frame shows. *Optimise for my PC* uses it |
| **PS5 latency test** | Presses the PS button over Remote Play, times the response on screen and splits the PS5, network and decoding part from PushFPS's own processing |
| **One-click signed updates** | New versions download and install from the app; every release is signed (RSA-3072, SHA-256) and the app refuses anything that doesn't match |
| **Frame input API** | Other programs can send video straight into PushFPS through shared memory ([docs/FRAME-INPUT-API.md](docs/FRAME-INPUT-API.md)) |
| **Optimise for my PC** | Tests every frame generation method your PC has, AI enhancement and the upscalers on your real game picture, measures frame rate, 1% low, latency and jitter, and keeps the best combination for your goal (balanced, lowest latency, smoothest or best picture), up to 144 or 240 Hz |
| **Streaming mode** | An OBS-style page: preview, PushFPS features, scenes (game scenes with a logo or text, *Starting soon* and *Be right back* cards) with webcam, window and video sources (cameras at up to 4K, shown live) and cut or fade transitions, a right-click menu to add, duplicate or delete scenes, a layout editor (move, resize and crop the game picture and every source), go live and record controls, an audio mixer for game sound, microphone, desktop audio, an extra input and monitor with level meters, and microphone filters (rumble filter, EQ, noise gate, compressor) |
| **Video playback mode** | Frame generation, super resolution, AI enhancement and SDR to HDR on any video on your PC: a browser window with YouTube, a video app, a screen or a file |
| **Ultrawide screens** | A console only sends 16:9. On a 21:9 or 32:9 screen choose black bars, **ambient fill** (a soft, dimmed extension of the picture in the bars, nothing cut or distorted), crop or stretch ([more](#ultrawide-screens)) |
| **Fine-tune AI picture enhancement** | Shape the change the enhancement makes: its strength, brightening and darkening, shadows, midtones and highlights, colour, hue, and limits per pixel ([more](#enhancement-look)) |
| **Crash notice** | If PushFPS ever closes unexpectedly, the next start says so and offers a summary to copy, with no user name, folder path or stream address in it |
| **Test my graphics card** | Measures how fast PushFlow runs on your card at 1080p and 4K and says what that means for 30 and 60 FPS games; the result can be copied and sent to us |
| **Lowest latency in one tap** | Every latency setting that costs no picture quality, switched on together; off restores your previous settings |
| **Go live** | Stream the boosted picture straight to Twitch, YouTube, Kick, TikTok, Facebook, Trovo or any RTMP server, to several platforms at once from one encode, at its own resolution (vertical for TikTok LIVE). Stream keys are stored encrypted |
| **Output presets** | Platform settings per resolution and frame rate for going live (Twitch, YouTube, Kick, Facebook, TikTok LIVE, Trovo) and for uploads (YouTube SDR and HDR, Shorts, TikTok, Instagram Reels, X, editing). Every value stays editable |
| **Frame Pacing Repair** | Evens out the uneven frame timing many 30 FPS console games have before frames are generated |
| **Keep the HUD sharp** | Optional, off by default: what stands still in front of a moving picture keeps its real pixels in generated frames. Experimental: it can add artifacts around the edges of moving things |
| **Live HUD Lock** | Finds still text, crosshairs and bars on every frame and keeps them sharp in generated frames, no boxes needed |
| **Lip sync by hand** | Move the sound earlier or later in steps of 10 ms on top of automatic sync, for the delay your capture card, screen or headphones add |
| **Choose before you record** | Click Record and pick horizontal or vertical, fill or fit, the size (4K, 1440p, 1080p) and the quality, and see the bitrate before it starts |
| **Adaptive multiplier** | Follows the game when its frame rate drops, so the output stays even |
| **Game presets** | GTA VI and other layouts in one click, with the minimap and HUD kept sharp. Save your own and share them as a code |
| **Automatic HUD detection** | Watches your game for a few seconds and finds the minimap, health and ammo, so they are never interpolated. Correct the boxes by dragging them |
| **Before/after comparison clips** | Records a clip of the console's own frame rate against PushFPS in four layouts: split screen, side by side, vertical 9:16 for TikTok, Shorts and Reels, or Reveal, where a line sweeps across the whole picture and uncovers PushFPS. From 10 seconds to 2 minutes or until you stop it, each side labelled with its frame rate; split and reveal clips at up to 4K |
| **No capture card needed** | PS5 Remote Play, Moonlight (stream another PC through Sunshine) and Xbox streaming, with the same frame generation, from Veyra |
| **Help on hard settings** | A small **?** beside settings that need explaining; hover it for a short summary |
| **Quick setup** | Console, game frame rate and target frame rate instead of hundreds of settings, with console and game profiles |
| **Latency on screen** | The measured PC latency, frame rate and 1% low over the picture |
| **NVIDIA Image Scaling** | Fast upscaling and sharpening on NVIDIA, AMD and Intel graphics |
| **Picture & sound editor** | Picture styles, gamma, sharpness and sound profiles (bass boost, footsteps, night mode and more) |
| **Share clips** | Saved recordings and instant replays go to Discord in one click |
| **OBS Studio plugin** | Your boosted picture as a source in OBS |
| **Tested capture cards** | Which cards are known to work, shown in the app |
| **14 languages** | English, 简体中文, 繁體中文, 日本語, Nederlands, Deutsch, Español, Français, Italiano, Português (Brasil), Türkçe, Polski, Русский, العربية |
| **Light and black modes** | The default dark grey, an almost-black mode for OLED screens, and a light mode |
| **Plug and play** | Connects to your capture card when PushFPS opens and goes fullscreen when the picture appears (both optional) |
| **Measure total delay** | A guided slow-motion phone test that shows the whole delay PushFPS adds over your capture card's passthrough |
| **Linux and macOS (preview)** | PushFPS's own frame generation, Frame Pacing Repair and Live HUD Lock on Vulkan, OpenGL or Metal, with no vendor components |

## How it works

```mermaid
flowchart LR
    A[Console<br/>30 FPS game] -- HDMI --> B[Capture card]
    B -- USB / PCIe --> C[PushFPS on your PC<br/>frame generation]
    C --> D[Monitor<br/>60 / 120 FPS]
    C -. optional .-> E[OBS Studio<br/>stream or record]
```

Your console sends its picture to a capture card. PushFPS reads every frame, notices that a 30 FPS game
repeats each picture twice inside the 60 Hz signal, keeps only the real frames, and fills the gaps with
generated frames on your GPU. You play on the PC monitor.

Generated frames make motion smoother. They don't make the game react faster: the controller still feels
like the original 30 FPS, plus a few milliseconds for the capture card and processing.
[More on latency](#latency).

## Screenshots

<table>
<tr>
<td width="50%"><img src="assets/screenshots/frame-generation.png" alt="Professional mode, frame generation tab"><br><sub><b>Frame generation.</b> Quick setup on top, every detail below.</sub></td>
<td width="50%"><img src="assets/screenshots/capture-card.png" alt="Capture card dialog"><br><sub><b>Capture card.</b> Device, format, frame rate, colour and audio in one dialog.</sub></td>
</tr>
<tr>
<td width="50%"><img src="assets/screenshots/picture.png" alt="Professional mode, picture quality tab"><br><sub><b>Picture.</b> Super resolution, AI enhancement layers, HDR.</sub></td>
<td width="50%"><img src="assets/screenshots/settings.png" alt="Settings"><br><sub><b>Settings.</b> Interface, playback, performance, shortcuts.</sub></td>
</tr>
<tr>
<td width="50%"><img src="assets/screenshots/picture-sound.png" alt="Home screen, picture and sound panel"><br><sub><b>Picture &amp; sound.</b> Picture styles, gamma, sharpness and sound profiles on the home screen.</sub></td>
<td width="50%"><img src="assets/screenshots/appearance-light.png" alt="PushFPS home screen in light mode"><br><sub><b>Light mode.</b> Settings → Appearance.</sub></td>
</tr>
<tr>
<td width="50%"><img src="assets/screenshots/optimize.png" alt="Optimise for my PC dialog"><br><sub><b>Optimise for my PC.</b> Tests every combination on your real picture.</sub></td>
<td width="50%"><img src="assets/screenshots/game-presets.png" alt="Game presets dialog with GTA VI"><br><sub><b>Game presets.</b> GTA VI and more, with the HUD kept sharp.</sub></td>
</tr>
<tr>
<td width="50%"><img src="assets/screenshots/appearance-black.png" alt="PushFPS home screen in black mode"><br><sub><b>Black mode.</b> Almost-black surfaces for OLED screens.</sub></td>
<td width="50%"><img src="assets/screenshots/anr.png" alt="Settings, Apex Neural Rendering install card"><br><sub><b>ANR by Apex Compute.</b> Download from Apex, install in one click.</sub></td>
</tr>
<tr>
<td width="50%"><img src="assets/screenshots/hud-protection.png" alt="HUD protection dialog with automatic detection and the box editor"><br><sub><b>HUD protection.</b> Automatic detection, boxes you can correct by hand.</sub></td>
<td width="50%"><img src="assets/screenshots/ps5-streaming.png" alt="PS5 streaming dialog"><br><sub><b>PS5 streaming.</b> No capture card needed: Remote Play over your home network.</sub></td>
</tr>
<tr>
<td width="50%"><img src="assets/screenshots/pc-streaming.png" alt="PC streaming dialog for Sunshine hosts"><br><sub><b>PC streaming.</b> Another PC through Sunshine (Moonlight protocol).</sub></td>
<td width="50%"><img src="assets/screenshots/recording-advanced.png" alt="Advanced encoding settings"><br><sub><b>Advanced encoding.</b> Rate control, quality, preset, keyframes, MP4 or MKV.</sub></td>
</tr>
</table>

<sub>Screenshots of the real app, taken automatically on a build machine without a graphics card, so
NVIDIA-only controls show as unavailable. On an RTX PC they are active.</sub>

## Download and install

1. Download the latest **PushFPS-…-win64.zip** from [Releases](https://github.com/DosukaSOL/PushFPS/releases)
   and extract it.
2. Double-click **Install-PushFPS.cmd**.

The installer picks the right package for your graphics card, installs PushFPS to your user folder (no
administrator rights needed), adds Start menu and desktop shortcuts, and starts it.

<details>
<summary>What the installer does, step by step</summary>

PushFPS runs on Veyra's official runtime package: Qt, FFmpeg and the NVIDIA, AMD and Intel components
that Veyra's author distributes. The installer downloads that package (about 750 MB) directly from
[Veyra's GitHub release](https://github.com/Likely7/Veyra-NRVideo/releases), checks its SHA-256 against
the value pinned when the PushFPS release was built, unpacks it, and adds PushFPS on top. PushFPS itself
never rehosts those files.

- **Experimental extras.** Veyra's package also has the files AI picture enhancement runs on for NVIDIA RTX
  and Radeon cards, built on NVIDIA files used outside the terms NVIDIA licenses them under. The installer
  asks whether you want them (default no); `Install-PushFPS.cmd -ExperimentalExtras Yes` installs them
  without asking, and running the installer again lets you change your answer. Details in
  [NOTICE.md](NOTICE.md).
- Install folder: `%LOCALAPPDATA%\Programs\PushFPS`
- Update: PushFPS offers new versions itself (signed, one click), or run the newer installer; your settings
  and profiles are kept either way
- Uninstall: `Install-PushFPS.cmd -Uninstall`, or delete the folder and shortcuts
- Windows may warn about an unknown publisher (PushFPS isn't code signed yet). Choose *More info*, then
  *Run anyway*, if you downloaded the zip from this repository.

</details>

**Portable install.** PushFPS keeps its settings, profiles and logs inside its own folder. To keep it out of
your user folder and Start menu, run `Install-PushFPS.cmd -InstallDir D:\PushFPS -NoShortcuts` (any folder);
moving or deleting that folder moves or removes PushFPS. Only the PS5, Moonlight and Xbox sign-ins are kept,
encrypted, in your Windows user profile.

**Checking the download.** Every release lists the SHA-256 of each file in `SHA256SUMS.txt` and the exact
pinned components in `PushFPS-<version>-components.txt`. In PowerShell: `Get-FileHash PushFPS-*.zip`.

> [!NOTE]
> Every release is built and start-tested automatically on Windows. Its release notes say plainly which parts
> were also tested by a person with a capture card and GPU. Reports from real setups are very welcome in
> [Issues](https://github.com/DosukaSOL/PushFPS/issues).

## What you need

| | Minimum | Recommended |
|---|---|---|
| **System** | Windows 11 64-bit with DirectX 12 | Latest Windows updates |
| **Graphics card** | Any DirectX 12 card: PushFlow (2× to 4×) and AMD FSR (2×) run on NVIDIA, AMD and Intel. NVIDIA RTX 30 for NVIDIA's methods (experimental there) | NVIDIA RTX 40 or 50 series |
| **Capture card** | Any card that shows up as a camera in Windows and sends 1080p at 60 FPS without compression, or [stream instead](#no-capture-card-stream-instead) | A card with 4K60 capture and passthrough |
| **Connection** | USB 3 port directly on the PC (no hub) | USB 3.2 Gen 2 or a PCIe card |
| **Console** | Any console with HDMI out | 60 Hz output, or 120 Hz if your capture card captures 120 Hz (passthrough doesn't count) |

AMD Radeon and Intel cards can use PushFlow (2× to 4×) and FSR frame generation (2× only, so 60 FPS from a
30 FPS game); Radeon cards also get AMD FRC.

**AI picture enhancement** is separate from frame generation. On NVIDIA RTX cards (the app lists RTX 20 to
50) and on Radeon RX 7000 and 9000 it runs on community-built runtime files, which the installer treats as
*experimental extras*: it asks, and the default is no. Without them the switch is greyed out and says *This
package lacks the runtime components for this feature*. Run `Install-PushFPS.cmd` again and answer **y**
when it asks about the experimental extras; your settings are kept, and updates from inside PushFPS keep
the extras.

### Tested graphics cards

| Graphics card | Maker | Status |
|---|---|---|
| GeForce RTX 5090 | NVIDIA | Tested |
| GeForce RTX 5080 | NVIDIA | Tested |
| GeForce RTX 5070 | NVIDIA | Tested |
| GeForce RTX 4090 | NVIDIA | Tested |
| GeForce RTX 4080 | NVIDIA | Tested |
| GeForce RTX 4070 | NVIDIA | Tested |
| Radeon RX 9070 XT | AMD | Tested |

*Tested* means PushFPS has been run on that card with real games. It is a compatibility list, not a speed
ranking, and other DirectX 12 cards should work too. To get your card on it: play for a while, copy the
**performance report** (*Settings → Performance report*) and open a
[hardware report](https://github.com/DosukaSOL/PushFPS/issues/new?template=hardware-report.yml).
[How to measure, and what the numbers mean](docs/BENCHMARKING.md).

<details>
<summary>Which frame generation methods your PC offers</summary>

| Method | Graphics cards | Most frames per real frame |
|---|---|---|
| [PushFlow](#pushflow-our-own-frame-generation), PushFPS's own | DirectX 12 cards: NVIDIA, AMD and Intel | 4× |
| NVIDIA Video Frame Generation (VFG) | RTX 40, RTX 50 (RTX 30 experimental) | 8× |
| NVIDIA DLSS Frame Generation | RTX 40 (2×), RTX 50 (up to 6×) | 6× |
| Intel XeSS Frame Generation (experimental) | DirectX 12 cards; Veyra tested it on NVIDIA | 4× |
| AMD FSR Frame Generation | DirectX 12 cards (FSR 4 ML preferred on a Radeon RX 9000, falls back to FSR 3.1) | 2× |
| AMD FRC video frame interpolation | AMD graphics with a current AMD driver (uses the driver's AMF runtime) | 2× |
| [ANR Frame Generation](https://github.com/apex-compute/ANR) by Apex Compute (experimental) | GTX 16 and newer, RX 5000 and newer, Intel Arc (Apex's list); install it in *Settings → Components and licenses* | 6× |

</details>

## Get started

1. **Connect everything.** Console HDMI out goes into the capture card's input. The card's USB or PCIe
   connection goes into the PC. If the card has an HDMI passthrough, you can leave a TV on it.
2. **Set the console to 60 Hz.** Even for 30 FPS games, consoles send a 60 Hz signal. 1080p is enough for
   most cards; use 4K if your card captures 4K60.
3. **PlayStation only: turn HDCP off.** Settings, System, HDMI, turn off *Enable HDCP*. With HDCP on, the
   capture card only shows black. Xbox and Switch games are not HDCP protected.
4. **Open PushFPS and fill in the quick setup** on the home screen, or let PushFPS do it: connect the capture
   card (step 5), get the game on screen and click **Optimise for my PC**.

   | Setting | What to pick |
   |---|---|
   | **Console** | PlayStation 5, PS5 Pro, Xbox Series X\|S, Nintendo Switch 2 or Other. Each console remembers its own choices |
   | **Game profile** | Optional: save the current settings under a game's name, switch to them later in one click |
   | **Game frame rate** | 30 for most games (GTA VI), 40 for 40 FPS modes, 60 for performance modes |
   | **Target frame rate** | Off, 60 FPS, 120 FPS or Match monitor (on a 90 Hz screen: 90 FPS in place of 120). PushFPS picks the frame generation multiplier. 60 on a 60 Hz monitor, 120 on 120 Hz and faster ([target for every refresh rate](https://dosukasol.github.io/PushFPS/#guide-rates)) |
   | **AI picture enhancement** | Off: frame generation only. On: every frame also goes through AI enhancement (NVIDIA RTX and Radeon RX 7000 / 9000; needs the installer's experimental extras, see [What you need](#what-you-need)). Uses a lot more GPU, so start with it off |

5. **Click Connect** on the capture card. Choose your card, its resolution, 60 FPS and the card's audio input.
6. Press **F11** (or Alt+Enter), or double-click the picture, for fullscreen.

> [!TIP]
> Close other apps that use the capture card (OBS, the card's own software) before connecting.

> [!TIP]
> **Setup guide:** the recommended settings to start from, how to tune them, console, capture card and
> Windows settings, and when to use HDR: [on the website](https://dosukasol.github.io/PushFPS/#guide) or as
> [PushFPS-Setup-Guide.txt](docs/PushFPS-Setup-Guide.txt) (also attached to every release).

## No capture card? Stream instead

The home screen also offers three network sources, built on Veyra's streaming support. Frame generation,
AI enhancement, HUD protection, recording and every other PushFPS feature work on them the same way.

| Source | What it streams | What you need |
|---|---|---|
| **PS5 streaming** | Your own PS5 over your home network (Remote Play protocol, via [chiaki-ng](https://github.com/streetpea/chiaki-ng)) | Remote Play turned on on the PS5, a one-time registration with your PSN account ID and the PIN the PS5 shows. Your controller works through PushFPS |
| **PC streaming** | Another computer running [Sunshine](https://github.com/LizardByte/Sunshine) (Moonlight protocol) | Sunshine on the other PC and a one-time pairing PIN |
| **Xbox streaming** (experimental, unofficial) | Your own Xbox console over your home network | Remote features turned on on the Xbox and a Microsoft account sign-in |

A wired network (or 5 GHz Wi-Fi) gives the most stable picture. Streams are compressed, so a capture card
still gives the sharpest picture and the lowest latency.
> Only one program can use a capture card at a time. To show PushFPS in OBS, use the
> [OBS Studio plugin](#obs-studio-plugin) instead.

## PushFlow: our own frame generation

<img src="assets/pushflow-logo.png" alt="PushFlow" height="64">

PushFlow is PushFPS's own frame generation. It runs **RIFE** (Real-Time Intermediate Flow Estimation, an open
neural network for in-between frames, MIT-licensed, by hzwer) through **ONNX Runtime on DirectML**, so it works
on every DirectX 12 graphics card: NVIDIA, AMD and Intel. Choose it under *Frame generation* in Professional
mode or in streaming mode.

- **2×, 3× or 4×**: each frame is made for its own moment between the two real frames.
- **Tiers**: *Fast* (lite model, motion estimated at up to 540p), *Balanced* (lite model, up to 720p) and
  *Quality* (full model, up to 1080p); whenever the motion is estimated smaller, the picture is rebuilt at
  full resolution, so 4K stays sharp and fast enough for live play.
- **Choose the motion size** (since 3.2.7): *Professional → PushFlow motion size* sets 540p, 720p or 1080p
  for any tier; Auto keeps the sizes above. Smaller is lighter and sometimes follows fast objects
  further, larger is more exact for small motion. The frame quality test's *Worst place* tells you which
  suits your game. Not yet tested on real graphics cards.
- **Sharp Fast tier**: the network's final motion and blend mask are enlarged, not its picture: every pixel is
  sampled from the full-resolution real frames. On a panning fine-texture test this scores 25 dB against
  17 dB for enlarging the half-size picture.
- **Everything stays on the graphics card**: a compute pass packs the two frames into the network's input
  (once per pair; later frames only change the time), the network runs on PushFPS's own GPU queue, and a
  second pass turns the result back into a picture, with no copies through the CPU.
- **Never waits on the CPU**: each generated frame is queued on the GPU and the next step follows it there,
  so the engine thread keeps running while the network works (since 3.2.0).
- **Experiment: encode each real frame once** (*Professional → PushFlow 2*, off by default): the model's
  per-frame part runs once per real frame instead of twice per generated frame, with the same picture
  (checked against the whole model on your card at start). Meant to save the most at 3× and 4×; how much it
  saves on real graphics cards has not been measured yet. `PushFPS.exe --safe-mode` starts with every
  experiment off.
- **Experiment: Guard** (*Professional → PushFlow 2*, off by default): when an object moves too far between
  two real frames for PushFlow to follow, it is missing from the generated frame. Guard finds those places
  and shows the nearer real frame there, so the object moves at the game's own frame rate instead of
  blinking out. It never invents a picture, and it counts every frame it had to replace whole. Checked on
  test pictures only; whether it looks better in a real game is for you to judge.
- **Experiment: Predict** (off by default, live sources only): ordinary frame generation waits for the next
  real frame, so the real picture is shown about one frame late. Predict shows the newest real frame as
  soon as it is processed and forecasts the frames after it from the two that have already arrived. Lower
  delay on paper (up to one frame); a forecast can be wrong until the next real frame corrects it. Nothing
  is invented: every pixel is a pixel of the newest real frame, moved or left where it is.
- **Experiment: Dual** (off by default; with Predict, at 2X): recordings, instant replay and streams get
  the real frames and the true in-between frames instead of the forecasts you see, one frame behind the
  screen.
- **Experiments: Response and Memory** (off by default; measurements only, nothing on the picture):
  Response measures whether the picture's motion follows the right stick, how strongly and how late (PS5
  Remote Play); Memory measures how much two neighbouring real frames differ once lined up. They say, on
  your own setup, whether those two ideas are worth building.
- None of the five experiments above has been measured on a real graphics card yet. They are in
  *Professional → Frame generation* with PushFlow as the method; `docs/PUSHFLOW2.md` says what each was
  tested with and what is still unknown.
- **Tells you when your card can't keep up**: if nearly every frame pair has to be skipped, PushFlow steps
  down one tier and says why, where it used to sit at the game's frame rate without a word.
- **Half precision, tested safely**: *Professional → Frame generation → PushFlow half precision → Test* tries
  the faster fp16 model in a separate process and turns it on only when the picture matches and it really
  is faster on your card. A driver fault in that test can't touch PushFPS.
- **Measure your card**: `PushFlowBench.exe`, next to `PushFPS.exe`, writes PushFlow's timings on your card
  as JSON. Details: [docs/PUSHFLOW.md](docs/PUSHFLOW.md).
- Works with HUD protection, Live HUD Lock, the adaptive multiplier, recording and going live.
- *Optimise for my PC* tests PushFlow like the other methods, and the frame quality test can score it on your
  own game.

**Before 3.2.2 PushFlow did not generate frames inside PushFPS**: one line in the engine left it out of the
methods that are given frames to work on, so the picture stayed at the console's frame rate. PichotM found
and fixed it; with the fix he measured 60 → about 120 FPS on an RTX 4090 over PS5 Remote Play. If you tried
PushFlow before and saw no difference, try it again.

How fast it is depends on your graphics card, the tier and the resolution. Our own tests run in CI on
Windows' software renderer; apart from that one report it has not been measured on real graphics cards
(*Settings → About → Test my graphics card* and PushFlowBench are how we will get those numbers). TensorRT isn't bundled (its runtime is
over a gigabyte and NVIDIA-only); DirectML covers every vendor.

## Optimise for my PC

Click **Optimise for my PC** in the quick setup while your game is on screen, and pick a goal: **Balanced**,
**Lowest latency**, **Smoothest** or **Best picture**. In one to three minutes PushFPS:

1. measures the picture without effects, to find the game's frame rate (30, 40 or 60) and the base latency;
2. tries every frame generation method your PC can run (PushFlow, DLSS, NVIDIA VFG, ANR, FSR 4/3.1, XeSS, AMD FRC) at
   the highest frame rate your screen shows: your monitor's own rate on 144, 165 or 240 Hz screens when the game
   divides into it evenly, otherwise 120 FPS, and 60 FPS if none holds that;
3. adds AI enhancement to the best one, then the upscalers when your screen is larger than the capture;
4. measures the winner again to confirm it, and applies it. **Undo** puts your old settings back.

The result lists every method it skipped and why (not supported by this graphics card, not installed), and
shows PC processing latency, its 95th percentile and frame-time jitter for each tested combination.

Every number is measured on the real picture: PushFPS times each frame it hands to the display and each
frame's latency, after the settings are applied and warmed up, over a 6-second window per combination. A
combination only counts when it really generated frames, reached 95 % of its target, kept its 1% low at
80 % of the target, dropped under 1 % of the capture card's frames, and added no more than 60 ms of latency.

It works the same with a capture card and with PS5 or Xbox Remote Play: it measures whatever picture is on
screen. If PushFPS ever stops while it is testing a method, your settings from before the run come back at the
next start, and that method is not tested again in that version, so one method that fails on your PC can't
break the optimisation (since 3.2.0).

## Game presets

Click **Game presets** in the quick setup and pick a game:

| Preset | Protected areas |
|---|---|
| **GTA VI** | Minimap bottom left; health and focus bars top left; weapon, wanted level and cash top right. Which corner holds what comes from the footage Rockstar showed before release; the edges are estimates, so check them with **Adjust HUD areas** once you play. The adaptive multiplier is on, because the base PS5 dips below 30 FPS in busy scenes |
| Open world, minimap bottom left / top right | The minimap |
| Action, HUD in the bottom corners | Health, weapon and ammo |
| Racing, speedometer bottom right | Speed, gear and minimap |
| Shooter, HUD in all four corners | Every corner |
| Cinematic | None |

Protected areas use *Exclude all effects*: they keep the console's own pixels and are never interpolated, so
text and the minimap don't smear when the camera moves. **Adjust HUD areas** opens the HUD editor (see below);
Professional mode, *Quality*, *Exclude all effects* has the same areas. **Save current settings as a preset** stores your own, including the areas you drew.
**Copy code** gives a `PUSHFPS1:` code to share on Reddit or Discord; **Add** or **Open file** imports one.

### Nintendo Switch and Switch 2

Many Switch games stay at 30 FPS even on Switch 2 when they have no Switch 2 update, and several Switch 2 ports
run at 30 FPS docked. The presets list has a filter for each console (it starts on the console you picked in the
quick setup) and sets the console, the game's frame rate and the target in one click:

- **Switch:** Hyrule Warriors: Age of Calamity, Pokémon Legends: Arceus, Zelda: Breath of the Wild (the original;
  the Nintendo Switch 2 Edition runs at 60), Xenoblade Chronicles X: DE, Monster Hunter Rise, The Witcher 3,
  DOOM Eternal, Batman: Arkham Knight, Kingdom Come: Deliverance, Shin Megami Tensei V: Vengeance,
  Mario + Rabbids Sparks of Hope, Animal Crossing: New Horizons, Fire Emblem: Three Houses, Fire Emblem Engage,
  Persona 5 Royal, Pokémon: Let's Go, Dragon Quest XI S, Astral Chain.
- **Switch 2:** Cyberpunk 2077, Hogwarts Legacy, Final Fantasy VII Remake and Rebirth, Star Wars Outlaws,
  Assassin's Creed Shadows, Elden Ring, Indiana Jones and the Great Circle, Persona 3 Reload, Lies of P.

Frame rates come from public tests (Digital Foundry, the howsitrun.com database and Switch 2
backward-compatibility reports). Games whose frame rate dips (Age of Calamity, Arkham Knight, Cyberpunk 2077,
Star Wars Outlaws, Elden Ring) turn on the adaptive multiplier. HUD areas are set only where the layout is well
known (Breath of the Wild, The Witcher 3, DOOM Eternal, Cyberpunk 2077, Elden Ring, Lies of P); for the others
use **Adjust HUD areas** to find them automatically. Set the Switch to its dock, 60 Hz; games have no HDCP.

## Adaptive multiplier

Console games rarely hold one frame rate: a 30 FPS game dips to 25, a 40 FPS mode wanders between 35 and 45,
Elden Ring on Switch 2 sits anywhere from 30 to 40. With a fixed multiplier the output follows every dip. Turn
on **Adaptive multiplier** in the quick setup (or under frame generation in Professional mode) and the
multiplier becomes a ceiling: PushFPS measures the game's frame rate from the capture card picture (automatic
duplicate removal) and, for every pair of real frames, uses the multiplier whose output lands closest to the
target (your output cap, or the monitor's refresh rate).

At a 120 FPS target: 30 FPS → 4X, 40 FPS → 3X, 36 FPS → 3X (108), 33 FPS → 4X (132), 25 FPS → 5X with ANR or
VFG, 4X with DLSS (which has no 5X). A change waits half a second so a game sitting on a boundary doesn't make
it flicker. The quick setup shows what it is doing ("Now 3×: game 38 FPS → about 114 FPS").

This is the same idea as NVIDIA's DLSS 4.5 Dynamic Multi Frame Generation, which switches between whole
multipliers to reach the display's refresh rate; PushFPS bases it on the console's frame rate and it works
with DLSS, ANR and NVIDIA VFG frame generation. FSR, AMD FRC and XeSS generate a fixed 2X or pace themselves,
so the switch is not offered for them. A lower multiplier also means a little less delay.

## Frame Pacing Repair

Many console games locked to 30 FPS don't deliver their frames evenly. Instead of every frame staying on screen
for two 60 Hz refreshes, some stay for one and others for three (17 ms, then 50 ms). The game simulated them 33 ms
apart, so the motion judders even though the frame rate is right, and frame generation keeps that judder. The
console can't fix it: the picture it sends is already uneven.

**Frame Pacing Repair** (quick setup, or frame generation in Professional mode) puts every real frame back on an
even grid before frames are generated. It fits a line through the arrival times of the last frames, so a 17/50 ms
game becomes 33/33. It only acts on what it can explain as uneven delivery of a locked frame rate. A stall
restarts it, and a game whose frame rate genuinely varies keeps its own timing. A frame is shown at most one
capture refresh later; for the common 17/50 ms pattern about half a refresh. The quick setup shows how much of the
picture it is re-timing. It needs automatic duplicate removal, which it turns on. It is a
choice, off by default: for a locked 30 or 40 FPS game, duplicate removal alone already spaces the frames evenly,
and a game whose frame time really varies should keep its own timing.

## Live HUD Lock

Frame generation interpolates every pixel between two real frames, so still text, crosshairs and health bars get
dragged along with the moving background and smear or wobble. HUD protection fixes this with boxes. **Live HUD
Lock** finds those pixels itself, on every frame, on the graphics card. A pixel that has stayed the same for
about a second and has an edge (text, a crosshair, a bar) is taken from the real frame in the generated frames,
and released the moment it moves. A still pixel looks the same in the real frame, so locking one that isn't HUD
(a still wall) changes nothing visible. Works with DLSS, ANR, NVIDIA VFG, FSR and AMD FRC frame generation; XeSS
generates inside its own presenter, so it can't be combined.

## Keep the HUD sharp

Off by default, and experimental: *Quick setup → Keep the HUD sharp*. (4.0 had it on for everyone. It sharpened
HUDs, and it added artifacts around the edges of moving things, so 4.0.1 went back: with the switch off,
generated frames are exactly what they were before 4.0.) Frame generation works out one motion for every place
of the picture, so a HUD that stands still in front of a moving world gets the world's motion too. With **Keep
the HUD sharp** on, a pixel that is the same in both real frames, and stands in a surrounding that mostly moves,
keeps its real value in the generated frames between them. It works with every method PushFPS runs itself (NVIDIA's,
FSR, PushFlow), not with XeSS. With a still camera or in a menu it touches nothing. It judges every pair of
frames by itself, where Live HUD Lock remembers: the two can be on together.

It does not remove ghosting in the moving world right beside a character, and a source with visible noise gives
it less to find.

## Automatic HUD detection

Click **HUD protection** in the quick setup while your game is playing, then **Detect automatically** and
turn the camera or walk around for about five seconds. PushFPS looks for the parts of the picture that hold
still while everything else moves (minimap, health, ammo, prompts) and draws up to four green boxes
around them on a frame of your own game. Since 3.2.0 it looks at every pixel of a 1080p copy of the picture,
so it also finds text drawn straight over the game, thin bars and a minimap whose map scrolls; the first
version only found HUD elements on solid panels, which few games have.

If a box is off, fix it by hand: drag a box to move it, drag its corners to resize it, drag on an empty spot
to draw a new one, and use the **×** on the selected box to delete it. **Apply** protects the boxes: those
areas keep the console's own pixels and are never interpolated. With no boxes, the whole picture is
interpolated.

If the picture hardly moved during detection (a menu, a paused game), PushFPS can't tell the HUD apart and
asks you to try again while moving. The detection reads a few presented frames on the GPU and works with
every frame generation method; it does not change any setting until you click **Apply**.

## Smooth frame pacing

**Smooth frame pacing** in the quick setup (on by default) makes live play look like a recording:
- every frame is shown at a screen refresh (V-Sync);
- variable refresh (G-SYNC/FreeSync) is requested where your screen has it;
- the output is capped at your refresh rate.

Every generated frame is shown once. With variable refresh it adds almost no delay; without it a frame can wait up to one refresh. Turn it off for the lowest-latency mode, where a frame can now and then be skipped. For the best result, turn on G-SYNC/FreeSync in your graphics driver and play in fullscreen (F11).

## Lowest latency (one tap)

**Lowest latency** in the quick setup turns on every latency setting that costs no picture quality:
- the graphics card queues only one frame (NVIDIA Low Latency on NVIDIA cards, the low queue on others);
- fullscreen when the capture card or stream connects, which skips the desktop compositor's extra refresh;
- on a G-SYNC/FreeSync screen, smooth frame pacing with variable refresh (no tearing, no skipped frames).

Frame generation, AI enhancement, HUD lock and pacing repair stay as they are. On a screen without variable
refresh the display sync is left alone, so the picture never tears. Turning it off brings back your previous
settings. The row shows the measured processing latency, and suggests 120 Hz console output where it would
save about 8 ms per frame.

## Ultrawide screens

A PS5, Xbox or Switch sends a 16:9 picture whatever screen it ends up on, so a 21:9 or 32:9 screen has room
left over at the sides. On such a screen the quick setup shows **Wide-screen fill**:

| Choice | What you get |
|---|---|
| **Bars** | The whole picture, with black bars at the sides |
| **Ambient** | The whole picture, not cut and not distorted; the bars become a soft, dimmed extension of the picture itself |
| **Crop** | The screen filled, with about a quarter of the picture's height cut on 21:9 (half on 32:9). A HUD at the top or bottom can be lost |
| **Stretch** | The screen filled, with the picture pulled about a third wider on 21:9 |

None of them shows more of the game: the console does not render it. Recordings, streams and comparison clips
are made from the frames, not from the window, so they are never affected; recordings have their own 21:9
sizes (3440 × 1440 and 2560 × 1080) under *Output resolution*. Ambient fill is not available with XeSS frame
generation, which presents through its own path.

## Crop and resize

Cut any side of the console picture and choose how large and where it is shown: a game that draws its own
black bars, a border your capture card adds, or a part of the picture you simply don't want to see. In the
quick setup, **Crop and resize → Adjust…** opens an editor on one frame of the game:

- **Drag an edge** to cut that side, **a corner** to cut two, **the middle** to slide the part that is kept.
  Double-click to see the whole picture again.
- **Numbers** for the four sides, in percent of the picture (at most 45 % per side).
- **Shapes**: 16:9, 21:9, 32:9, 4:3 and 1:1, cut evenly around the middle. A 21:9 game with bars inside a
  16:9 signal becomes a real 21:9 picture that fills a 21:9 screen.
- **Size on screen** from 25 % to 400 %, and **Position**.

The picture on screen follows every change at once, and *Wide-screen fill* (bars, crop, stretch) then works
on the part that is kept. The switch in the quick setup shows the whole picture again without losing your
values, and console and game profiles remember the crop. Frame generation, AI enhancement and HUD protection
still work on the whole picture, so nothing about them changes.

Recordings, instant replay and streams keep the whole picture unless you switch on **Crop recordings and
streams too** in the editor. The crop is for a console picture (capture card, or PS5, Sunshine and Xbox
streaming); films and captured windows keep theirs. It does not apply with XeSS frame generation, which
presents through its own path.

## Before/after comparison clips

**Comparison clip** in the quick setup opens a small dialog: pick the layout and the length (10, 15, 30, 60,
90 or 120 seconds, or **Until I stop** since 3.2.4: the clip then runs until you click *Comparison clip* again
or press the recording shortcut, in every layout), then **Start recording** (the same choices are under
*Settings, Playback*). The clip goes into your recordings folder (`PushFPS-compare-….mp4`).

- **Split**: one picture cut down the middle. The left half is the raw console picture, each real frame held
  until the next one arrives, so it moves at the game's own frame rate; the right half is every frame PushFPS
  shows, with frame generation and AI picture enhancement.
- **Side by side**: the whole picture twice, each at half size: the raw console picture on the left, PushFPS
  on the right, on a black background.
- **Vertical (TikTok, Shorts, Reels)**: a 1080×1920 video, the console on top and PushFPS below, each filling
  half the screen with the middle of the picture. The badges sit next to the divider, where those apps leave the
  screen free. H.264, ready to upload from your phone (HDR recordings stay HEVC 10-bit).
- **Reveal** (since 3.2.5): the whole picture, full screen. The clip opens on the raw console picture at the
  game's own frame rate; a glowing line then sweeps from left to right and uncovers PushFPS, holds, sweeps back
  to the console picture, and repeats for as long as the clip runs. **Sweep every** sets how often: every 1, 2,
  3, 5 or 8 seconds. The dialog's preview plays it. Each badge is cut off at the line with its picture, so the
  label always says what you are looking at.

**Clip quality** (split and reveal, the two layouts whose picture fills the frame): *Auto* records at the
picture's own size, up to 1440p, as before. *1080p*, *1440p* and *4K* record at up to that height with a higher
bitrate; a smaller picture is never enlarged. A comparison clip records every frame PushFPS shows, so 4K at a
high frame rate needs a strong graphics card: if the card could not keep up, PushFPS says so when the clip ends
and you can pick one step lower. Side by side and vertical have sizes of their own and are unchanged.

The console side has no AI enhancement, upscaling or colour changes, so the clip shows exactly what PushFPS adds.
Each side has a badge at the top: *Console · 30 FPS* and the PushFPS logo with its frame rate, in SDR and HDR. Frame generation has to be on, using any method except XeSS (XeSS makes its frames as they
are shown, after the recorder has read the picture). Click the tile again to stop early. The card that appears
afterwards can share the clip to Discord.

## Enhancement look

AI picture enhancement reworks light, colour and fine detail. How far it should go is a matter of taste, so the
quick setup has four controls for it, shown while AI picture enhancement is on:

- **Enhancement look**: Subtle, Balanced or Strong. Strong is the full effect and what PushFPS has always
  used. Balanced and Subtle stay closer to the original picture. **Reset** returns to Strong.
- **Lighting**: how far the AI reworks light and colour. At 0% the original's light and colours stay.
- **Detail**: how much fine texture, contact shadow and reflection the AI adds.
- **Compare with the original**: splits the picture while you tune, original on the left, enhanced on the
  right. Frame generation pauses while it is on, so it turns itself off after two minutes, and when you turn
  the enhancement off or leave the page.

The look is saved with every console and game profile. Lighting and Detail are the two values Professional mode
shows for each enhancement layer (*Local tone* and *Local structure*); the quick setup sets them on every
layer. They are hidden with the AMD enhancement runtime, which has no such values.

### Fine-tune

The enhancement is applied as a change on top of the console's picture, and that change can be shaped without
touching the picture itself. **Fine-tune** (under the look, folded until you open it) has ten sliders:

| Slider | What it does |
|---|---|
| **Edit strength** | How much of the change is applied: 0% none, 100% as the model made it, 200% double |
| **Brightening**, **Darkening** | How much of the change that makes pixels brighter, or darker, is kept |
| **Max brightening**, **Max darkening** | A soft limit on how far any pixel may get brighter or darker. 0% is no limit |
| **Shadows**, **Midtones**, **Highlights** | How much of the change is kept in the dark, middle and bright parts of the picture |
| **Colour change** | How far the enhancement changes how strong colours are. 0% keeps the console's colours |
| **Hue shift** | How far it may turn hues. 0% keeps the console's hues |

With everything at its default nothing extra is done. The values apply to every enhancement layer and are saved
with each console and game profile; they work with every enhancement runtime. Edit strength, Brightening,
Darkening and Colour change are the values Professional mode shows per layer; the other six are new.
The limits act on the change itself: a colour that the change pushes out of range can still end slightly
brighter than the limit says.

## Profiles

**Console profiles.** Switching the console in the quick setup keeps what you set for the previous console
and brings back what you set for the new one: game frame rate, target frame rate, AI enhancement and
NVIDIA Low Latency. The setup tip on the home screen follows the console too.

**Game profiles.** Click **+** next to *Game profile*, type a name (for example *GTA VI*) and confirm. The
profile stores the console, game frame rate, target, AI enhancement and low latency. If the Professional
settings currently match a saved preset, the profile remembers that preset as well and applies it first, so
a game profile can carry every detail. Pick the game from the list to switch; the bin removes it.

## Picture and sound

Open **Picture & sound** on the home screen to adjust the picture and sound before you play or record:

| Control | What it does |
|---|---|
| **Presets** | *Natural*, *Vivid*, *Cinematic* or *Bright* as a starting point |
| **Brightness, contrast, gamma, saturation, sharpness** | Fine-tune the look. *Reset* goes back to the original picture, *Undo* takes back the last change |
| **More colour controls** | Opens the full colour editor in Professional mode: curves, colour wheels, LUTs, saved looks |
| **Sound profile** | *Bass boost*, *Voice clarity*, *Footsteps*, *Night mode* (quiet sounds louder, loud sounds softer), *Cinema*, or your own 5-band equaliser |

Everything you set here is part of the picture and sound you see, hear and record. Sound profiles are
also in Professional mode under *Audio*. Dolby and DTS passthrough to a receiver are not changed.

**After recording.** When a recording or an instant replay clip is saved, a card offers **Edit** (opens it in
the editor), **Export** (trim, pick tracks, re-encode) and **Show in folder**.

## Recording, instant replay and microphone

- **Record** (F8) saves what you see and hear, with the picture, sound profile and frame generation applied,
  for as long as you like: a recording runs until you stop it or the disk is full.
- **Choose before you record** (since 4.0): the Record button first asks for the shape (horizontal, or
  vertical 9:16 with *Fill* or *Fit*), the size (the picture's own, 4K, 1440p or 1080p) and the quality
  (*Best*, *High*, *Smaller files*), and says what will be recorded: size, frames a second, codec and bitrate.
  F8 never asks and uses what you chose last. *Ask before every recording* switches the question off.
- **Instant replay** keeps the last minute (15 seconds to 10 minutes) in memory; press **F9** to save it.
  Turn it on next to the Record button or under *Settings, Playback*. It can run while you record. It uses
  an extra hardware encoder and some memory while it is on, so it is off by default.
- **Microphone**: add your mic to recordings, mixed with the game sound or as its own audio track.
- **Share to Discord**: the card that appears after saving has **Share to Discord**. Without setup it copies
  the clip and opens Discord, so you paste it with Ctrl+V. With a Discord webhook in *Settings, Playback*
  it posts clips up to 10 MB straight to that channel.
- **Advanced encoding settings** (*Settings, Playback*, off by default): rate control (VBR, CBR or constant
  quality with a 1–51 level), encoder preset 1–7, keyframe interval, audio bitrate and MP4 or MKV, like OBS's
  output settings. Off, recordings use the standard settings. If the graphics card can't use a combination,
  the recording falls back to the standard settings and says so.
- **Upload presets** (*Settings, Playback*): YouTube (2160p to 720p, 30 or 60 FPS, and HDR), YouTube Shorts,
  TikTok, Instagram Reels (1080 × 1920), X and two for editing (the picture's own size at constant quality).
  A preset fills in resolution, frame rate, bitrate, codec, encoder preset, keyframes and audio; change any of
  them afterwards and the setting shows *modified*. Bitrates follow YouTube's upload recommendations; TikTok,
  Reels and X publish no exact bitrate, so those use the commonly recommended 8–15 Mbps within their limits.
- **Output resolution**: recordings have their own size (the picture's, 4K, 1440p, 1080p, 720p, ultrawide
  3440 × 1440 or 2560 × 1080, vertical 1080 × 1920, 4:5 or square), separate from the stream. **Fit** keeps the whole picture with bars, **Fill**
  crops the edges, **Stretch** distorts.
- **Split files** every 30, 60 or 120 minutes during long recordings, without stopping.
- **Encoders**: NVENC on NVIDIA; on AMD and Intel the card's own hardware encoder through Windows Media
  Foundation. Both do H.264 and HEVC; HDR recordings need NVIDIA (HEVC 10-bit).
- **Standard settings**: about 37 Mbps for 1080p at 60 FPS, 66 Mbps at 1440p and 150 Mbps at 4K (H.264,
  less with HEVC), with room for fast motion. Pick a fixed bitrate under *Settings, Playback* for smaller files.
- **Works in video editors**: a finished MP4 recording is rewritten as a standard MP4 (no re-encoding), so it
  opens in Vegas Pro, Premiere Pro, DaVinci Resolve and others. While recording it is kept in a crash-safe
  format, so a crash or power cut still keeps what was recorded. The rewrite needs the recording's size in
  free disk space once more; without it the recording is kept as written (it plays everywhere, but some
  editors refuse that form) and PushFPS says so.

## Streaming mode

**Streaming mode** (the broadcast button in the top bar) is a page laid out like OBS Studio, with PushFPS's own
features built in:
- **Preview** in the middle, with the source, the game's frame rate and what PushFPS shows.
- **PushFPS features** on the left: frame generation, adaptive multiplier, pacing repair, HUD lock, AI
  enhancement and lowest latency, as in the quick setup.
- **Controls** on the right: go live, record, save the instant replay, comparison clip, screenshot, your
  streaming platforms and each platform's state and bitrate while live.
- **Audio mixer** across the bottom, like OBS's: *Game sound* and *Microphone* faders with level meters and
  mute, for what recordings and streams take, and *Monitor* (what you hear yourself). Choose whether the
  microphone goes into recordings, streams or both.
  - *Desktop audio*: everything Windows plays except PushFPS (Discord, music, a browser). PushFPS leaves
    itself out, so the game is never in the mix twice.
  - *Extra input*: a second microphone, a line-in or an audio interface.
  Both are off until you switch them on, and go into recordings and streams you start after that.
- **Scenes**, like OBS's, switched with one click or **Ctrl+1** to **Ctrl+9**; right-click the list to add,
  duplicate, edit or delete a scene:
  - *game scenes* show the picture, optionally with a corner picture (your logo) or a text tag;
  - *card scenes* (*Starting soon*, *Be right back*, *Thanks for watching*, or your own with a background
    picture) replace the game in recordings and streams and mute the game sound while they are shown.
  Scenes change only what recordings and streams show; you keep seeing the game. The scene and the mixer
  hold in every mode, not only on this page: starting a recording elsewhere with a card shown or the game
  muted says so. Comparison clips always show the game, so a card never silences them.
- **Sources** in a scene (up to four): a **webcam**, another **window** (a browser with alerts or chat, a
  music player) or a looping **video file**. Put each in a corner or the middle, pick its size, and make it
  square, rounded or round (cameras can be mirrored). Sources run while streaming mode is on screen or a
  recording or stream is going, so your camera is not on while you just play. A minimised window has no
  picture; video files play without their sound. A camera opens at 720p by default; choose 1080p, 1440p
  or 4K under its settings, and the list there shows what the camera really offers (in webcam mode many
  cameras send less than they record). The picture under a source's settings and in the layout editor
  is live.
- **Layout editor** (*Layout*): the output picture with the game and every source on it. Drag to move, drag a
  corner to resize: sizes keep their proportions, hold **Ctrl** to stretch freely. Edges and the middle snap,
  the arrow keys nudge (Shift for bigger steps), each item can be cropped on every side, the game picture can
  be shrunk into a box on a background colour, and **Ctrl+Z** / **Ctrl+Y** undo and redo. Switch between the
  stream's and the recording's shape to check both; changes apply at once while you are live or recording.
- **Transitions** between scenes: *Cut*, *Fade* or *Slow fade*.
- **Microphone filters** (the sliders button on the microphone): rumble filter, 5-band equaliser with voice
  presets, noise gate and compressor, then a -1 dB limiter that is always on.
- **Status bar**: live time, bitrate and dropped frames, recording time, processing latency and the encoder.

## Video playback mode

**Video playback mode** (the play button in the top bar) puts PushFPS's tools on any video on your PC:
- pick a **window** (a browser playing YouTube, a video app), a **whole screen** or a **video file**;
- switch on **Smoother** (frame generation: 24 or 30 FPS video looks like 60 or 120 FPS), **Sharper**
  (super resolution to your screen), **AI picture enhancement** (removes compression blocks and noise) and
  **SDR to HDR** (on an HDR monitor with Windows HDR on);
- press F11 to watch fullscreen.

If a browser window captures black, turn off hardware acceleration in the browser's settings; copy-protected
streams may stay black.

## Go live: Twitch, YouTube, Kick and TikTok

Click **Go live** in the quick setup to stream the picture you see, frame generation, picture settings and sound
included, straight from PushFPS. No OBS needed.

- **Several platforms at once.** Add up to eight outputs (Twitch, YouTube, Kick, TikTok, Facebook Live, Trovo or
  any RTMP/RTMPS server) and switch on the ones you want. One encode feeds them all, so streaming to three
  platforms costs no more graphics card time than one; each platform gets its own upload.
- **Platform presets.** Pick Twitch, YouTube, Kick, Facebook, TikTok LIVE or Trovo at the resolution and frame
  rate you want, and the stream's size, bitrate, keyframe interval and audio are set from that platform's own
  guidelines (for example Twitch 1080p60 at 6 Mbps, YouTube 1080p60 at 17 Mbps, TikTok LIVE 1080 × 1920).
  Everything stays adjustable: resolution (the picture's, 4K to 720p, or vertical), fit, bitrate from 1 to
  51 Mbps, 30 or 60 FPS, keyframe every 1–4 seconds, audio 96–320 kbps and the encoder preset.
- **OBS's settings.** Server lists and limits come from OBS Studio's service list. The bitrate never goes above
  the strictest platform you stream to (Twitch 6 Mbps, Kick 8 Mbps); streams are H.264 with constant bitrate,
  as the platforms ask.
- **Stream keys stay private.** A key is encrypted for your Windows user (DPAPI), never shown again, never
  written to logs, and only sent to the server it belongs to.
- **Keeps going.** Each output reconnects by itself when the connection drops; the others keep streaming. The
  dialog shows each output's state, bitrate, dropped frames and reconnects.
- Game sound and your microphone (mixed in). Recording and instant replay can run while you are live; each uses
  its own encoder session.

TikTok gives a server address and key only to accounts with LIVE access (TikTok LIVE Producer). Streaming needs
an SDR picture: turn HDR output off first.

## Upscaling: NVIDIA Image Scaling and more

Professional mode, *Quality*, *Super resolution*, *Upscaling algorithm* offers DLSS SR, RTX Video Super Resolution (three model
families), AMD FSR 3.1 and **NVIDIA Image Scaling (NIS)**. NIS is a fast spatial upscaler with adaptive
sharpening that runs on NVIDIA, AMD and Intel graphics. Set the strength with *NIS sharpening* (50 % is
NVIDIA's default; 0 % only upscales). NIS enlarges up to 2x in one pass, so a 1080p capture fills a 4K
screen directly; larger factors are finished with regular scaling. It works in SDR and HDR.

**Keep the HUD sharp.** *Exclude all effects* regions (up to four rectangles or ellipses, from Veyra 2.1.6)
leave parts of the picture, such as the minimap or the HUD, untouched by every effect and by frame
generation.

## OBS Studio plugin

The plugin adds a **PushFPS** source to OBS Studio 28 or newer on Windows, including OBS 33 (it installs in
the plugin layout that OBS 34 will require, so the Plugin Manager doesn't flag it as Legacy). It shows the PushFPS picture,
generated frames included, in any scene, and can start PushFPS for you.

1. Download **PushFPS-OBS-Plugin-…-win64.zip** from [Releases](https://github.com/DosukaSOL/PushFPS/releases)
   and extract it.
2. Close OBS Studio and double-click **Install-PushFPS-OBS.cmd** (no administrator rights needed).
3. Start OBS Studio. Under *Sources*, click **+** and pick **PushFPS**.

| In the source's properties | What it does |
|---|---|
| **Open PushFPS** | Starts PushFPS with OBS game capture support, or brings it to the front (also under *Tools*) |
| **Capture method** | *Game capture* receives every generated frame (recommended). *Window capture* is the fallback |
| **Start PushFPS when this source is shown** | On by default |
| **PushFPS.exe location** | Only needed if PushFPS isn't in its default folder |

For sound, add an *Application Audio Capture* source and pick `PushFPS.exe`. If you start PushFPS yourself
instead of from OBS, turn on *Settings, Compatible, OBS Game Capture compatibility* in PushFPS once and
restart it. Uninstall: `Install-PushFPS-OBS.cmd -Uninstall`.

<sub>How it works: PushFPS keeps doing the processing on your GPU; the plugin drives OBS's own game capture
(or window capture) pointed at the PushFPS window. Every build is loaded into the latest OBS Studio on
GitHub Actions, which opens a scene with a PushFPS source, lets it start PushFPS and checks that it finds
the window.</sub>

## Capture cards

| Capture card | Status |
|---|---|
| Elgato 4K X | Tested with PushFPS (RTX 5090, console games and videos) |
| T911 4K60 | Tested by Veyra's developer (4K60 NV12, RTX 5070) |
| Elgato HD60 X | Tested with PushFPS |
| Elgato 4K S | Tested with PushFPS |
| Elgato HD60 S | Reported working by a user (PS5 at 1080p60, Radeon RX 7600) |
| AVerMedia Live Gamer Ultra 2.1 (GC553G2) | Reported working by a user (PS5 Pro, RTX 4080 SUPER) |
| Elgato HD60 Pro | Reported working by a user (no further details) |
| ezcap Game Link Raw | Reported working by a user (Xbox Series X over USB 3.0, Radeon RX 9060 XT) |
| Elgato 4K Pro / 4K60 Pro, HD60 S+ | Not tested yet |
| AVerMedia Live Gamer 4K 2.1 (GC575) | Not tested yet |
| Magewell USB Capture HDMI 4K Plus | Not tested yet (native low-latency mode from Veyra) |
| Blackmagic DeckLink / UltraStudio | Not tested yet (Blackmagic's own interface from Veyra) |

Any card that Windows shows as a camera and that sends uncompressed 1080p60 or 4K60 should work. The quick
setup shows whether your card is on this list.

Capture at 60 FPS, even for 30 FPS games (PushFPS removes the repeated pictures), in uncompressed NV12 or
P010 rather than MJPEG. Delay measured by Veyra's developer: about 10-20 ms at 1440p60 NV12, 30-40 ms at
4K60 NV12 (10 Gbps or PCIe cards) and 80-100 ms at 4K30 HDR. For the lowest delay, capture 1440p60 and let
RTX Video Super Resolution upscale it. Which card to buy (5 Gbps, 10 Gbps, PCIe) is in the
[setup guide](https://dosukasol.github.io/PushFPS/#guide-capture). Tested yours?
[Send a capture card report](https://github.com/DosukaSOL/PushFPS/issues/new?template=capture-card-report.yml).

## Languages and appearance

**Settings → Language** has 14 languages: English, Simplified and Traditional Chinese, Japanese, Dutch,
German, Spanish, French, Italian, Portuguese (Brazil), Turkish, Polish, Russian and Arabic. *Automatic*
follows your Windows language.

In the ten newest (Dutch to Arabic) every button, menu and setting name and all PushFPS feature texts are
translated by hand; longer help texts in the advanced pages are machine-translated with
[Argos Translate](https://github.com/argosopentech/argos-translate) and fall back to English where the
machine output was not usable. If a word reads wrong in your language, please open an issue, or fix it in
`scripts/i18n/hand/<language>.txt`. Arabic text is shown correctly, but the layout is not fully mirrored
right-to-left yet.

**Settings → Appearance** switches between *Default* (dark grey), *Black* (almost-black surfaces, good on
OLED screens) and *Light*. Text and controls keep a readable contrast in every mode (checked by an
automated test); the picture itself is never changed.

**Sunset City.** PushFPS opens with a three-second intro: a neon road out of the dark, the logo landing on the
beat, the sky turning to sunset over a palm-lined city, and the logo gliding into place on the home page, where
it stays filled with the sunset city. The music is the startup track that comes with Veyra's runtime package.
Click or press any key to skip; *Settings → Appearance* has separate switches for the Sunset City logo, the
intro and the music. All of the art is drawn by PushFPS itself (`scripts/brand/make_sunset.py`).

## ANR frame generation by Apex Compute

[Apex Neural Rendering (ANR)](https://github.com/apex-compute/ANR) is frame generation made by
[Apex Compute](https://apexcompute.com). Apex lists support for GeForce GTX 16 and newer, Radeon RX 5000 and
newer and Intel Arc, so it is an option on graphics cards that DLSS Frame Generation does not cover. PushFPS
runs it on your capture card picture, the same way Veyra 2.1.6 does.

ANR is in beta and Apex does not allow it to be redistributed, so it is not inside PushFPS. To use it:

1. Download the `anr-fg-addon` package (or `anr-fg-full`) from [ANR's releases](https://github.com/apex-compute/ANR/releases).
2. In PushFPS open *Settings → Components and licenses* and click *Install from download…*, then choose the
   `.zip` you downloaded. PushFPS copies ANR's files into its own folder on your PC; nothing is sent anywhere.
3. Restart PushFPS and pick *ANR FG* as the frame generation method in Professional mode.

ANR in games gets motion data from the game itself. From a capture card PushFPS has only the picture, so the
motion is estimated, as with every method in PushFPS: results can differ from what ANR shows in games.

## Linux and macOS (preview)

PushFPS for Linux and macOS is a separate, smaller app (`portable/` in this repository) with PushFPS's own frame
generation: block motion estimation on the processor and motion-compensated interpolation on the graphics card
(Vulkan or OpenGL on Linux, Metal on macOS), plus Frame Pacing Repair, duplicate-frame removal and Live HUD Lock.
It needs no NVIDIA, AMD or Intel components, so it runs on any graphics card that supports OpenGL 3.3, Vulkan or
Metal.

| | Linux | macOS |
|---|---|---|
| Download | `PushFPS-<version>-x86_64.AppImage` (updates itself with AppImageUpdate) or `PushFPS-<version>-linux-x86_64.tar.gz` | `PushFPS-<version>-macos-universal.zip` (Apple Silicon and Intel) |
| Needs | x86-64, glibc 2.31+ (Ubuntu 20.04+, Debian 11+, Fedora 33+), X11 or Wayland, OpenGL (`libopengl0`, present on desktop systems) | macOS 12 or newer |
| Capture cards | Any UVC card (`/dev/video*`): almost every USB card | Any card macOS lists as a camera |
| Portable | Put an empty `portable.txt` next to the program | Put an empty `portable.txt` next to PushFPS.app |

Try it without a capture card: **Home → Test picture**. `PushFPS --selftest 6` checks the whole chain.

Not in the preview yet: the NVIDIA/AMD/Intel frame generation and AI enhancement (Windows only), recording,
going live and the streaming sources. It is tested automatically (core tests with sanitizers, self-tests, a GPU
check against the processor reference, a clean Ubuntu container); it is not code signed or notarised, so macOS
asks you to confirm the first start (right-click → Open).

## Tune it for your setup

| If you see | Try |
|---|---|
| Black picture from a PlayStation | Turn HDCP off on the console |
| Stutter every second or so | Set the capture card to exactly 60 FPS, close other apps that use the card, use a USB port directly on the PC |
| Smeared HUD or text during fast motion | Pick a lower target (60 instead of 120), or a higher frame generation quality in Professional mode |
| Picture looks washed out | Match the colour settings in the capture dialog to the console: HDR (PQ) or SDR, Limited or Full range |
| The game feels slow to react | Turn on NVIDIA Low Latency. Frame generation can't reduce the game's own delay |
| Frame rate drops with AI enhancement on | Turn AI enhancement off, or lower its resolution in Professional mode |

## Latency

Turn on **Show latency on screen** in the quick setup to see the measured PC latency (capture card picture
arriving to handing it to the display, mean and 95th percentile), the frame rate and the 1% low over the
picture. The capture card, the display and the game add their own delay, which software can't measure.

To place a frame between two real frames, PushFPS has to wait for the second one. With a 30 FPS game that
adds up to one frame (about 33 ms) on top of the capture card's own delay, which depends on the card.
Generated frames make motion look like 120 FPS, but your inputs still take effect at the game's own pace.

For fast competitive games, use **Off** and play on the console's own display path.

Ways to lower it:

- **120 Hz output.** If your capture card captures 120 Hz (for example the Elgato 4K X, or the 4K S at 1080p or
  1440p), set the console to 120 Hz: each frame reaches the PC about 8 ms sooner, and 40 FPS games in a 120 Hz
  mode work too (pick 40 as the game frame rate). Passthrough doesn't count: a card that only passes 120 Hz
  through to your TV (such as the HD60 X, which captures 60 Hz) needs a 60 Hz console output. This is the same
  on every console, PS5 Pro included.
- **NVIDIA low latency** in the quick setup on NVIDIA graphics.
- **One graphics card for everything.** With two graphics cards, connect the monitor to the one PushFPS
  processes on; PushFPS warns when every frame has to be copied between cards.

To see the whole delay, not just PushFPS's part, use **Measure total delay** next to the latency switch: film
the capture card's passthrough screen and PushFPS side by side in slow motion and count the frames between them.

The readout can be dragged anywhere over the picture (a double click sends it back to the corner). The
performance report (*Settings → Performance report*) also says how long your screen took after the hand-over
to show each frame, when Windows reports it.

### Lip sync

Automatic sync delays the sound by the time the picture spends inside PushFPS. It cannot see how much later
than its sound your capture card hands over the picture, or what your screen, soundbar or wireless headphones
add. If voices and mouths don't match, use **Lip sync** in the quick setup: *Earlier 10* / *Later 10* move
the sound in steps of 10 ms on top of automatic sync.

## FAQ

**Does it work with games that already run at 60 FPS?**
Yes. Set *Game frame rate* to 60 and pick 120 FPS or Match monitor: a 60 FPS game gets one generated frame
in between (two on a 240 Hz monitor).

**Does it work on an ultrawide monitor?**
Yes. The console's picture stays 16:9, and you choose what fills the rest: see
[Ultrawide screens](#ultrawide-screens).

**I have a 90 Hz screen. Can I get 90 FPS?**
Yes. On a 90 Hz screen the quick setup offers 90 FPS (three frames for every frame of a 30 FPS game).

**PushFPS closed by itself. What now?**
Start it again: it tells you what it knows and offers a summary to copy into a
[GitHub issue](https://github.com/DosukaSOL/PushFPS/issues). The summary holds no user name, folder path or
stream address. The crash file in the log folder does contain your Windows user name, so send that one
privately.

**Does it change the console or the game?**
No. PushFPS only processes the video coming out of the capture card. Nothing is installed on the console.

**Is it free?**
Yes. PushFPS is free and open source under the GPL-3.0, like Veyra.

**Is there a macOS or Linux version?**
Yes, as a preview: see [Linux and macOS (preview)](#linux-and-macos-preview). It has PushFPS's own frame
generation instead of the NVIDIA, AMD and Intel methods, which only exist for Windows.

**Where are my settings and profiles stored?**
Inside the PushFPS folder itself (`%LOCALAPPDATA%\Programs\PushFPS` unless you chose another one), with the
logs. Updating PushFPS keeps them.

**AI picture enhancement says "This package lacks the runtime components for this feature".**
Its files are an optional extra the installer leaves out unless you say yes. Run `Install-PushFPS.cmd` again
and answer **y** when it asks about the experimental extras: see [What you need](#what-you-need).

**PushFPS says "No generated frames". What now?**
Frame generation is switched on, but PushFPS is only showing the game's own frames, so the picture is no
smoother than the console's. The message says why: the method is not running (pick another one), the graphics
card does not keep up (lower the multiplier, the tier or the output resolution), or nothing comes out although
the method runs. In the last case press **Copy performance report** next to the message and send it to us.

**How do I show what PushFPS does on my PC?**
*Settings → Performance report → Copy*, while a game is playing. It holds the graphics card and driver, your
settings, the game's own frames and the generated frames shown each second, and PushFPS's software delay, and
nothing personal. [What the numbers mean](docs/BENCHMARKING.md).

**Where do I get help?**
On the [PushFPS Discord](https://discord.gg/N88znNpSgC), where other players and the developer answer. Bugs and capture card reports
go to [GitHub Issues](https://github.com/DosukaSOL/PushFPS/issues).

## Roadmap

What comes next, in the order things depend on each other, is in **[docs/ROADMAP.md](docs/ROADMAP.md)**.
Most of it waits for measurements on real graphics cards: [how to take one](docs/BENCHMARKING.md).

- [x] PushFPS name, logo and update channel
- [x] Quick setup: target frame rate, frame generation with or without AI enhancement
- [x] Windows release with automated, reproducible builds
- [x] Per-console and per-game profiles
- [x] OBS Studio plugin
- [x] Cleaner interface: capture card first, less text, aligned controls
- [x] Picture and sound editor, sound profiles, edit after recording
- [x] NVIDIA Image Scaling (NIS) upscaling and sharpening on any DirectX 12 GPU
- [x] Instant replay and microphone recording (from Veyra 2.1.6)
- [x] Optimise for my PC: measured, automatic settings
- [x] Game presets with HUD protection and share codes
- [x] Latency readout on the picture
- [x] Share clips to Discord
- [x] Capture card list with tested status and a report form
- [x] 14 interface languages, light and black appearance modes
- [x] Automatic HUD detection with a box editor
- [x] Before/after comparison clips
- [x] Help marks (**?**) on settings that need explaining
- [ ] Native-speaker review of the machine translations, right-to-left layout for Arabic
- [ ] More tested capture cards and game presets, from player reports
- [x] PS5 Remote Play, Moonlight and Xbox streaming in the release builds
- [x] Veyra 2.1.9: AMD FRC frame generation, FSR 4 ML with FSR 3.1 fallback, local audio monitoring separate from recording, NVIDIA G-SYNC profile check and repair, processing on a second graphics card
- [x] Plug and play (connect on start, fullscreen on connect), 120 Hz and 40 FPS guidance, total-delay guide, always on top
- [x] Go live to Twitch, YouTube, Kick, TikTok and more, several platforms at once
- [x] Optimiser goals, every frame generation method tested, 144-240 Hz targets
- [x] Linux and macOS preview with PushFPS's own frame generation
- [x] PushFlow, PushFPS's own frame generation (RIFE on DirectML)
- [x] Measured frame quality in the optimiser, automatic PS5 latency test
- [x] One-click signed updates, frame input API for other apps
- [x] Streaming mode: scenes, sources, layout editor, audio mixer, output presets
- [x] Video playback mode for any video on the PC
- [x] Ultrawide screens (ambient fill), 90 Hz screens, fine-tuning of AI picture enhancement
- [x] Crash notice and a graphics card test
- [x] Comparison clips up to 2 minutes or until stopped; cameras at up to 4K with a live picture
- [x] Reveal comparison clips (a line sweeps between the console and PushFPS), clip quality up to 4K
- [x] A warning when frame generation shows no frames, a performance report to copy, tested graphics cards
- [x] 4.0: the HUD kept sharp with every method, a steadier 60 → 30 rhythm, lip sync by hand, a line in the
      live comparison, a capture card's own sound input chosen for a new card, recording choices before
      a recording starts
- [ ] PushFlow timings on real NVIDIA, AMD and Intel cards
- [ ] Linux and macOS: tested capture cards, recording and going live
- [ ] Code signing (SignPath Foundation application)

## Privacy

PushFPS has no accounts, analytics or telemetry. It only connects to the internet for the update check (GitHub),
when you share a clip to your own Discord webhook, when you connect a streaming source, and when you go live
(only to the platforms you chose). Details in [PRIVACY.md](PRIVACY.md).

## Build from source

Every push builds PushFPS on GitHub Actions ([build-windows.yml](.github/workflows/build-windows.yml)), and
that workflow is the build recipe: Visual Studio 2022, CMake with Ninja, Qt 6.8.3, FFmpeg 9.0.1 from vcpkg
at the pinned baseline, the NVIDIA DLSS SDK 310.7.0, nv-codec-headers, the CUDA driver header, Intel XeSS
3.0.2 and AMD FidelityFX 1.1.4 and 2.3.0. All of them are downloaded from their official public sources at
pinned versions; none is committed. The workflow then runs the unit and interface tests, checks that
`PushFPS.exe` links exactly the Qt and FFmpeg libraries in Veyra's runtime package, installs it with the real
installer and opens every main page, failing on any interface error.

The NVIDIA Optical Flow SDK needs a developer login, so its headers are never in this repository. Release
builds get its two interface headers from a private repository secret (`NVOF_SDK_HEADERS`) and then include
NVIDIA optical flow as Veyra does; a build without the secret (a fork, for example) uses AMD FidelityFX
optical flow for motion estimation instead (it runs on any DirectX 12 GPU). NVIDIA Video Frame Generation
has its own optical flow and is not affected. If you build locally with the Optical Flow SDK in
`third_party_local/nvidia/Optical_Flow_SDK_5.0.7`, NVIDIA optical flow is available as in Veyra.

The OBS plugin ([obs-plugin/](obs-plugin)) is a small C module built against OBS's public headers:

```
cmake -S obs-plugin -B build-obs -A x64 -DOBS_SOURCE_DIR=<obs-studio checkout>
cmake --build build-obs --config Release
```

More background: [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md), [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
Inside the code, PushFPS keeps Veyra's names (namespaces, targets, the `qml/Veyra` module) so fixes from
Veyra keep merging cleanly. The PushFPS name and links live in
[`include/veyra/Brand.h`](include/veyra/Brand.h).

## Credits and license

PushFPS is made by **DosukaSOL** ([@DosukaSOL on X](https://x.com/DosukaSOL)).

PushFPS is built on **[Veyra](https://github.com/Likely7/Veyra-NRVideo)** by
**[Likely7](https://github.com/Likely7)** (GPL-3.0). It started from Veyra on 2026-11-12 and is now its own
project; Veyra's engine, capture support and streaming sources are still at its core. Likely7 is on
[X](https://x.com/likely2002) and [YouTube](https://www.youtube.com/@Likely7Ai).

ANR frame generation is **[Apex Neural Rendering](https://github.com/apex-compute/ANR)** by
**[Apex Compute](https://apexcompute.com)**. PushFPS contains Veyra's own ANR host code (GPL-3.0) and no ANR files;
ANR is downloaded from Apex by the user. To update ANR, download Apex's newest `anr-fg` package and use *Install from download*
again; PushFPS replaces the previous files.

- PushFPS and Veyra code: [GPL-3.0](LICENSE). Files written for PushFPS, including the OBS plugin, are
  GPL-3.0-or-later. What PushFPS changed, and where each release's source is: [NOTICE.md](NOTICE.md).
- PushFPS.exe incorporates the NVIDIA DLSS SDK and NGX SDK under the NVIDIA RTX SDKs License, and uses the
  AMD FidelityFX SDK and the Intel XeSS SDK; their licenses ship in `licenses/sdk` of every release. For the
  code written for PushFPS, its authors grant an additional permission (GPL-3.0 section 7) to combine it with
  these SDKs; see [NOTICE.md](NOTICE.md), which also lists what the installer leaves out for licensing reasons.
- PushFPS comes with absolutely no warranty, as the GPL-3.0 states. The app shows the license and a link to
  this source under *Settings, About*.
- Streaming: PS5 Remote Play uses [chiaki-ng](https://github.com/streetpea/chiaki-ng) (AGPL-3.0 with an OpenSSL
  exception, [licenses/remoteplay](licenses/remoteplay)); PC streaming uses
  [moonlight-common-c](https://github.com/moonlight-stream/moonlight-common-c) (GPL-3.0); Xbox streaming uses
  [libdatachannel](https://github.com/paullouisageneau/libdatachannel) (MPL-2.0). Each release attaches a
  *streaming-source* archive with the exact source of these libraries as built.
- Third-party components and their licenses: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

PushFPS is not affiliated with or endorsed by Apex Compute, NVIDIA, AMD, Intel, Sony, Microsoft, Nintendo, the OBS Project
or any game publisher, including Rockstar Games; the Sunset City look is PushFPS's own artwork. The startup music
belongs to Veyra's runtime package (a track its author has permission to distribute with Veyra); PushFPS plays it
from there and does not include it. NVIDIA, RTX and DLSS are trademarks of NVIDIA Corporation. PlayStation is a trademark
of Sony Interactive Entertainment. Xbox is a trademark of Microsoft. Nintendo Switch is a trademark of
Nintendo. OBS Studio is a trademark of the OBS Project.
