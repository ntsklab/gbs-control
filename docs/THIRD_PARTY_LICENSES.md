# Third-Party Licenses

This file documents the licenses of third-party code and assets included
in or used by the gbs-control-esp project.

The project itself is licensed under **GNU General Public License v3.0**
(see [LICENSE](../LICENSE)).

---

## 1. arduinoWebSockets — Markus Sattler

- **Files:** `components/gbs_control/src/WebSockets.cpp`, `WebSocketsServer.cpp`,
  `components/gbs_control/include/WebSockets.h`, `WebSocketsServer.h`
- **Origin:** https://github.com/Links2004/arduinoWebSockets
- **License:** GNU Lesser General Public License v2.1 or later (LGPL-2.1-or-later)
- **Included via:** upstream gbs-control project (bundled in `src/`)

```
Copyright (c) 2015 Markus Sattler. All rights reserved.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.
```

---

## 2. Si5351mcu — Pavel Milanes

- **Files:** `components/gbs_control/src/si5351mcu.cpp`,
  `components/gbs_control/include/si5351mcu.h`
- **Origin:** https://github.com/pavelmc/Si5351mcu
- **License:** GNU General Public License v3.0 or later (GPL-3.0-or-later)
- **Included via:** upstream gbs-control project (bundled in `src/`)

```
Copyright (C) 2017 Pavel Milanes <pavelmc@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
```

---

## 3. PersWiFiManager — Ryan Downing

- **Files:** `components/gbs_control/src/PersWiFiManager.cpp`,
  `components/gbs_control/include/PersWiFiManager.h`
- **Origin:** https://github.com/r-downing/PersWiFiManager
- **License:** MIT License
- **Included via:** upstream gbs-control project (modified for inclusion)

```
MIT License

Copyright (c) 2017 Ryan Downing

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

---

## 4. ThingPulse esp8266-oled-ssd1306 — Daniel Eichhorn, Fabrice Weinberg

- **Files:** `components/gbs_control/include/OLEDDisplayFonts_ext.h` (generated)
- **Origin:** https://github.com/ThingPulse/esp8266-oled-ssd1306
- **License:** MIT License
- **Usage:** ArialMT_Plain font data extracted by `setup_deps.sh`.
  The file is NOT checked into the repository — it must be generated
  by running `./setup_deps.sh`.

```
MIT License

Copyright (c) 2016 by Daniel Eichhorn
Copyright (c) 2016 by Fabrice Weinberg

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

**Note:** The `SSD1306Wire.cpp/h` implementation in this project is an
**independent reimplementation** of a compatible API — it does not contain
code copied from the ThingPulse library.

---

## 5. Font Data in fonts.h / OLEDMenuFonts.h

These files are part of the upstream gbs-control project (GPLv3) and contain
font data generated using the ThingPulse online font generator
(http://oleddisplay.squix.ch/).

The underlying font designs have their own licenses:

| Font | License | Source |
|------|---------|--------|
| URW Gothic L Book | GPL with Font Exception | Ghostscript / URW++ |
| Open Sans Regular | Apache License 2.0 | Google Fonts |
| DejaVu Sans Mono | Bitstream Vera License + Public Domain | DejaVu Fonts project |

The "GPL with Font Exception" means that documents or programs that embed the
font data do not themselves become subject to the GPL solely by virtue of
embedding the font. All three font licenses are compatible with GPLv3.

---

## License Compatibility Summary

| Component | License | Compatible with GPLv3? |
|---|---|---|
| gbs-control (upstream) | GPLv3 | — (same) |
| arduinoWebSockets | LGPL-2.1+ | ✓ |
| Si5351mcu | GPL-3.0+ | ✓ |
| PersWiFiManager | MIT | ✓ |
| ThingPulse OLED fonts | MIT | ✓ |
| URW Gothic L font data | GPL + Font Exception | ✓ |
| Open Sans font data | Apache-2.0 | ✓ |
| DejaVu Sans Mono data | Bitstream Vera License | ✓ |
