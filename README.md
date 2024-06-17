# EasyRPG Multiplayer Native

[![Discord](https://img.shields.io/discord/1190331444380307496?color=blue&labelColor=555555&label=&logo=discord&style=for-the-badge)](https://discord.gg/RbCsGgAXnt "Discord")

Implementation of EasyRPG multiplayer in C++, through which you can quickly obtain the client and server.

The first commit of EPMP is [44347b6](https://github.com/monokotech/EasyRPG-Multiplayer-Native/commit/44347b680680454035c80711e38d9588465d65e5) (Initial commit. Add multiplayer changes), based on [ynoclient:cd7b46d](https://github.com/ynoproject/ynoengine/tree/cd7b46d4d02e63e01324e9854342fd4a66eaa281) 2023-05-31.

Not sure how to use it? You can refer to the [Tutorial].

If you are interested in this project, you can clone the repository to your local system to browse the code. If you don't know how to use Git, please refer to: [Git Manual](https://git-scm.com/book/en/v2).

Remember to switch to the `dev` branch, as many updates will be submitted there first.


## Requirements

### Required

- [liblcf] for RPG Maker data reading.
- [libuv] for multiplayer networking.
- [libsodium] for multiplayer chat.
- SDL2 >= 2.0.5 for screen backend support.
- Pixman for low level pixel manipulation.
- libpng for PNG image support.
- zlib for XYZ image and ZIP archive support.
- fmtlib >= 6 for text formatting/coloring and interal logging.

### Optional

- FreeType2 for external font support (+ HarfBuzz for Unicode text shaping).
- mpg123 for MP3 audio support.
- WildMIDI for MIDI audio support using GUS patches.
- FluidSynth for MIDI audio support using soundfonts.
- Libvorbis / Tremor for Ogg Vorbis audio support.
- opusfile for Opus audio support.
- libsndfile for better WAVE audio support.
- libxmp for tracker music support.
- SpeexDSP or libsamplerate for proper audio resampling.
- lhasa for LHA (.lzh) archive support.
- nlohmann_json for processing JSON files (required when targetting Emscripten)

The older SDL version 1.2 is still supported, but deprecated.
Please do not add new platform code for this library.


## Credits

### \[Fork-02 from twig33-ynoclient\] [ynop-ynoengine](https://github.com/ynoproject/ynoengine) (2024-03)

- Flashfyre (sam): Synchronization, Web, ChatName (NameTag), ...
- maru (pancakes): Merge upstream, Handle merge conflicts, Web, ChatName, ...
- aleck099: New YNO protocol, Connection and data, Async download, ...

### \[Fork-01 from twig33-ynoclient\] [Char0x61-ynoclient](https://github.com/CataractJustice/ynoclient) (2022-06)

- Char0x61 (CataractJustice): Split & Improve, Char0x61's nametags, Settings Scene, ...
- xiaodao (苏半岛): In-game chat translation, Font bug fix, ...
- Led (Biel Borel): In-game chat original implementation, ...

### \[Original\] [twig33-ynoclient](https://github.com/twig33/ynoclient) (2021-11)

- twig33: Original concept and implementation

### Greetings to

- [EasyRPG](https://github.com/EasyRPG)
- [twig33](https://github.com/twig33)
- [Flashfyre](https://github.com/Flashfyre)
- [aleck099](https://github.com/aleck099)
- [Ledgamedev](https://github.com/Ledgamedev)
- [Char0x61](https://github.com/CataractJustice)
- [maru](https://github.com/patapancakes)
- [JeDaYoshi](https://github.com/JeDaYoshi)
- [kekami](https://kekami.dev)
- [苏半岛](https://github.com/lychees)
- azarashi
- Altiami

### Additional thanks

- [Jixun](https://github.com/jixunmoe) for helping in the C++ problems
- [Ratizux](https://github.com/Ratizux) for the podman suggestions
- [Proselyte093](https://github.com/Proselyte093) for giving the project a chance to compile on the macOS ARM
- [Mimigris](https://github.com/Mimigris) is looking for bugs and providing suggestions
- [jetrotal](https://github.com/jetrotal) is providing suggestions
- [Nep-Timeline](https://github.com/Nep-Timeline) proposed IPv6 support
- ChatGPT for the C++ knowledge
- With help from various participants


## License

EasyRPG Player is free software available under the GPLv3 license. See the file
[COPYING] for license conditions. For Author information see [AUTHORS document].

EasyRPG [Logo] and [Logo2] are licensed under the CC-BY-SA 4.0 license.

For multiplayer licensing, see [LICENSE-EPMP.txt]

### 3rd party software

EasyRPG Player makes use of the following 3rd party software:

* [minetest util] Minetest C/C++ code - Copyright (C) 2010 celeron55,
  Perttu Ahola \<celeron55@gmail.com\>, provided under the LGPLv2.1+
* [socks5.h] Modern C++ SOCKS5 Client Handler - by harsath, provided under MIT
* [FMMidi] YM2608 FM synthesizer emulator - Copyright (c) 2003-2006 yuno
  (Yoshio Uno), provided under the (3-clause) BSD license
* [dr_wav] WAV audio loader and writer - Copyright (c) David Reid, provided
  under public domain or MIT-0

### 3rd party resources

* [Baekmuk] font family (Korean) - Copyright (c) 1986-2002 Kim Jeong-Hwan,
  provided under the Baekmuk License
* [Shinonome] font family (Japanese) - Copyright (c) 1999-2000 Yasuyuki
  Furukawa and contributors, provided under public domain. Glyphs were added
  and modified for use in EasyRPG Player, all changes under public domain.
* [ttyp0] font family - Copyright (c) 2012-2015 Uwe Waldmann, provided under
  ttyp0 license
* [WenQuanYi] font family (CJK) - Copyright (c) 2004-2010 WenQuanYi Project
  Contributors provided under the GPLv2 or later with Font Exception
* [Teenyicons] Tiny minimal 1px icons - Copyright (c) 2020 Anja van Staden,
  provided under the MIT license (only used by the Emscripten web shell)

[Tutorial]: docs/TUTORIAL.md
[liblcf]: https://github.com/EasyRPG/liblcf
[minetest util]: https://github.com/minetest/minetest
[socks5.h]: https://github.com/harsath/SOCKS5-Proxy-Handler
[libuv]: https://github.com/libuv/libuv
[libsodium]: https://github.com/jedisct1/libsodium
[BUILDING document]: docs/BUILDING.md
[#easyrpg at irc.libera.chat]: https://kiwiirc.com/nextclient/#ircs://irc.libera.chat/#easyrpg?nick=rpgguest??
[COPYING]: COPYING
[AUTHORS document]: docs/AUTHORS.md
[Logo]: resources/logo.png
[Logo2]: resources/logo2.png
[LICENSE-EPMP.txt]: docs/LICENSE-EPMP.txt
[FMMidi]: http://unhaut.epizy.com/fmmidi
[dr_wav]: https://github.com/mackron/dr_libs
[baekmuk]: https://kldp.net/baekmuk
[Shinonome]: http://openlab.ring.gr.jp/efont/shinonome
[ttyp0]: https://people.mpi-inf.mpg.de/~uwe/misc/uw-ttyp0
[WenQuanYi]: http://wenq.org
[Teenyicons]: https://github.com/teenyicons/teenyicons
