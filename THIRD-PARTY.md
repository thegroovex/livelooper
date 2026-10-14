# Third-party notices / Fremdkomponenten

livelooper is licensed under the GNU Affero General Public License v3 (see `LICENSE`).
It contains or links the following third-party components. Their license texts are
reproduced or referenced below.

*Deutsch:* livelooper steht unter der GNU Affero General Public License v3 (siehe
`LICENSE`). Es enthält bzw. verwendet die folgenden Fremdkomponenten; deren
Lizenztexte sind unten wiedergegeben oder verlinkt.

| Component | Version | License | Source |
|---|---|---|---|
| JUCE framework | 8.0.15 | AGPLv3 (used under its AGPLv3 option) | https://github.com/juce-framework/JUCE |
| Signalsmith Stretch | bundled in `third_party/signalsmith-stretch` | MIT | https://github.com/Signalsmith-Audio/signalsmith-stretch |
| Signalsmith Linear | bundled in `third_party/signalsmith-linear` | MIT | https://github.com/Signalsmith-Audio/linear |
| Ableton Link | 4.1 (git submodule `third_party/link`) | GPLv2 or later | https://github.com/Ableton/link |
| asio-standalone (used by Link) | bundled with Link (`third_party/link/modules/asio-standalone`) | Boost Software License 1.0 | https://github.com/chriskohlhoff/asio |
| Drum samples of the built-in kits | `resources/drums` | CC0 1.0 / public domain | see `resources/drums/SOURCES.md` |
| Inno Setup (build tool for the Windows installer only, not part of livelooper) | 6.x | Inno Setup License | https://jrsoftware.org/isinfo.php |

## JUCE

Copyright (c) Raw Material Software Limited. The JUCE modules are dual-licensed under
the AGPLv3 and the commercial JUCE licence; livelooper uses them under the
**GNU Affero General Public License v3** (full text: `LICENSE`).

## Signalsmith Stretch / Signalsmith Linear

MIT License – Copyright (c) 2022 Geraint Luff / Signalsmith Audio Ltd.,
Copyright (c) 2025 Signalsmith Audio. Full text: `third_party/signalsmith-stretch/LICENSE.txt`,
`third_party/signalsmith-linear/LICENSE.txt`.

> Permission is hereby granted, free of charge, to any person obtaining a copy of this
> software and associated documentation files (the "Software"), to deal in the Software
> without restriction, including without limitation the rights to use, copy, modify,
> merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
> permit persons to whom the Software is furnished to do so, subject to the following
> conditions: The above copyright notice and this permission notice shall be included in
> all copies or substantial portions of the Software. THE SOFTWARE IS PROVIDED "AS IS",
> WITHOUT WARRANTY OF ANY KIND.

## Ableton Link

Copyright 2016, Ableton AG, Berlin. Ableton Link is dual-licensed: livelooper uses it under the
**GNU General Public License version 2 or later** (full text: `third_party/link/GNU-GPL-v2.0.md`,
`third_party/link/LICENSE.md`). GPLv2-or-later code may be combined into a work distributed
under the AGPLv3 (via GPLv3), which is why the whole of livelooper is AGPLv3. livelooper follows
the Ableton Link guidelines and test plan (joining a session never changes its tempo, start /
stop sync is optional). *Deutsch:* Ableton Link wird unter der GPLv2 oder neuer genutzt.

## asio-standalone

Copyright (c) 2003-2026 Christopher M. Kohlhoff. Distributed under the Boost Software License,
Version 1.0 (`third_party/link/modules/asio-standalone/LICENSE_1_0.txt`).

## Drum samples

Every drum sample of the built-in kits (Studio, 808, 909 Style, Lo-Fi and, since 1.6, House,
Techno, Boom Bap, Trap, Breaks, Rock, Disco, Afro Latin) comes from a source released under
**CC0 1.0** or into the **public domain**, or was synthesised for livelooper by
`tools/samples/synth_kits.py` (oscillators and noise, no recordings; dedicated to the public
domain under CC0 1.0); the file `resources/drums/SOURCES.md`
lists every sample with its original file, source and license statement. The samples were
converted, trimmed and normalised by `tools/samples/build_drum_samples.py`. The kit names
describe a sound style and do not imply endorsement by any manufacturer. Sound packs that users
import carry their own licences. *Deutsch:* Alle
Drum-Samples stammen aus Quellen unter CC0 1.0 oder sind gemeinfrei bzw. wurden für livelooper
selbst synthetisiert (CC0 1.0; Liste in `resources/drums/SOURCES.md`); die Kit-Namen beschreiben
einen Klangstil. Sound-Packs von Nutzern bringen ihre eigenen Lizenzen mit.

<!-- v1.6 features -->
## Demo loops (since 1.6)

The three loops of the demo set (`resources/demo/*.flac`, `presets/demo-set.livelooper`) are
synthesised from scratch by `tools/demo/make_demo_loops.py` (no samples, no third-party audio)
and released under **CC0 1.0** (public domain dedication). *Deutsch:* Die Demo-Loops sind selbst
erzeugt und unter CC0 1.0 freigegeben.

## Manrope font (since 1.6)

The 9:16 clip export draws its titles with **Manrope** (`resources/fonts/Manrope-600.ttf`,
`Manrope-800.ttf`), Copyright 2018 The Manrope Project Authors (https://github.com/sharanda/manrope),
licensed under the **SIL Open Font License 1.1** (https://openfontlicense.org). The font is embedded
unmodified; it may be bundled with software under any license. *Deutsch:* Die Schrift Manrope steht
unter der SIL Open Font License 1.1.

## Video encoding (since 1.6)

The clip export uses the video encoders of the operating system (AVFoundation on macOS, Media
Foundation on Windows). On Linux it can call an **ffmpeg** program that the user installed; ffmpeg
is not part of livelooper and is not distributed with it.
<!-- end v1.6 features -->

## Trademarks

Ableton, Push, Link, Launchpad, Launchkey, APC and all other product names mentioned are trademarks of their
respective owners; livelooper is not affiliated with or endorsed by them.
