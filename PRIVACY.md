# PushFPS privacy policy

PushFPS has no accounts, no analytics, no telemetry and no advertising. It does not collect, store or send
personal data to the PushFPS project or anyone else. Settings, profiles, recordings and logs stay on your PC
(in PushFPS's data folder and the folders you choose).

PushFPS connects to other systems only in these cases:

| When | Where it connects | What is sent | How to avoid it |
|---|---|---|---|
| Automatic update check, a few seconds after start, and *Settings → Check for updates* | GitHub (`api.github.com`, this repository's latest release) | A normal HTTPS request; GitHub sees your IP address as with any website | Block PushFPS in your firewall, or do not click *Check*; no other data is sent |
| *Share to Discord*, only when you click it | The Discord webhook address you entered (Discord URLs only) | The recording you chose to share | Leave the webhook address empty |
| *Discord status*, only if you switch it on | The Discord app on your PC (a local connection, no network); Discord then shows it to your friends | While a console's picture plays: the chosen game profile's name (unless you switch that off), the console, the frame rates and when the picture started. Nothing about you is read from Discord | Leave *Discord status* off |
| PS5 streaming, only when you connect | Your own PS5 on your network | Remote Play pairing and stream data; the PSN account ID you entered is used for pairing and stored encrypted on this PC | Do not use PS5 streaming |
| PC streaming (Moonlight), only when you connect | Your own Sunshine host on your network | Pairing and stream data | Do not use PC streaming |
| Xbox streaming, only when you sign in and connect | Microsoft's sign-in and Xbox services, then your Xbox | Your Microsoft sign-in (in Microsoft's own page) and stream data; tokens are stored encrypted on this PC | Do not use Xbox streaming |
| *Go live*, only while you stream | The servers of the platforms you added (Twitch, YouTube, Kick, TikTok, Facebook, Trovo or your own RTMP server) | The video and sound you stream, and that output's stream key. Keys are stored encrypted for your Windows user (DPAPI), never shown again and never written to logs | Do not go live |
| Translation (*Live translation*, and *Read the text on the picture → Translate*) with the **local model**: only when you press *Download model* | GitHub (the program that runs the model) and Hugging Face (the model file) | A normal HTTPS download, once, about 1.1 GB; the files are checked against fixed SHA-256 values. After that the model runs on your PC and the text never leaves it | Do not download the model |
| Translation with **MyMemory**, only if you choose that engine | `api.mymemory.translated.net` | The caption text, or the text you asked to translate, and the two languages | Keep the local model as the engine |
| Translation with a **custom API**, only if you enter one | The address you entered | The caption text or the text you asked to translate, and your API key. The key is kept in memory only and is not saved | Do not enter one |
| Opening a network stream (an address you enter) | The address you entered | A normal media request | Do not open one |
| Links and buttons that open web pages (GitHub, Apex Compute, Veyra, PSN account ID lookup, Discord) | Your web browser opens the page | Nothing from PushFPS; the page's own privacy policy applies | Do not click them |
| Installation (`Install-PushFPS.cmd`) | GitHub, to download Veyra's runtime package (Qt, FFmpeg and GPU components) | A normal HTTPS download | Install offline from a downloaded package |

**Reading the text on the picture.** When you ask for it, PushFPS takes one frame of the picture it shows and
Windows' own text recognition reads it on your PC. The frame is deleted as soon as it is read, the text is not
written to the log, and nothing is sent anywhere. Translating that text is a separate button; what it sends, if
anything, is in the table above.

**Local control and the virtual camera.** Local control is off unless you switch it on. It listens on this PC
only (127.0.0.1), every request needs a token that is stored encrypted for your Windows user, and it makes no
connection to anything outside your PC. The virtual camera is a video device on your PC; PushFPS sends its
picture nowhere.

**Feedback.** *Settings → Report a problem* copies diagnostic information (version, GPU, source format and
playback statistics) to your clipboard and opens GitHub Issues. Nothing is sent until you paste it into an issue
yourself; read it first and remove anything you don't want to share.

**Third-party components.** The streaming parts (chiaki-ng, moonlight-common-c, libdatachannel) and
Veyra's engine follow the same rules: they only connect when you start a stream. GPU components from
NVIDIA, AMD and Intel run locally.

**Linux and macOS preview.** It makes no network connections at all: no update check, no sharing, no streaming.
Settings stay in your user folder, or next to the program when `portable.txt` is there.

**Uninstalling.** `Install-PushFPS.cmd -Uninstall` removes the program. Your settings and recordings stay
until you delete PushFPS's data folder and your recordings folder.

Questions: open an issue at https://github.com/DosukaSOL/PushFPS/issues.
