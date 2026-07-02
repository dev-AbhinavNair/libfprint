

<div align="center">

# LibFPrint

*LibFPrint is part of the **[FPrint][Website]** project.*

<br/>

[![Button Website]][Website]
[![Button Documentation]][Documentation]

[![Button Supported]][Supported]
[![Button Unsupported]][Unsupported]

[![Button Contribute]][Contribute]
[![Button Contributors]][Contributors]

</div>

## History

**LibFPrint** was originally developed as part of an
academic project at the **[University Of Manchester]**.

It aimed to hide the differences between consumer
fingerprint scanners and provide a single uniform
API to application developers.

## Goal

The ultimate goal of the **FPrint** project is to make
fingerprint scanners widely and easily usable under
common Linux environments.

## License

`Section 6` of the license states that for compiled works that use
this library, such works must include **LibFPrint** copyright notices
alongside the copyright notices for the other parts of the work.

**LibFPrint** includes code from **NIST's** **[NBIS]** software distribution.

We include **Bozorth3** from the **[US Export Controlled]**
distribution, which we have determined to be fine
being shipped in an open source project.

## Get in *touch*

 - [IRC] - `#fprint` @ `irc.oftc.net`
 - [Matrix] - `#fprint:matrix.org` bridged to the IRC channel
 - [MailingList] - low traffic, not much used these days

<br/>

<div align="right">

[![Badge License]][License]

</div>


## ElanSPI Enhancement Fork

This fork adds several improvements for the **ElanTech ELAN7001 SPI** fingerprint
sensor found in **ASUS VivoBook X515EA** and similar laptops.

### Why This Exists

The upstream elanspi driver produces poor recognition rates (~50% verify) because:
- **No image enhancement** — raw sensor output has low contrast and noise
- **Only 7 enroll stages** — not enough template coverage for reliable matching
- **No diversity checking** — all enrollment swipes can cover the same finger area
- **bz3_threshold of 24** — too conservative for this sensor's image quality

### Changes

1. **`fpi_image_enhance()`** (new function in `fpi-image.c`)
   - Block contrast normalization (64x64 tiles) to compensate for uneven
     illumination and pressure
   - Unsharp masking (5x5 gaussian kernel, sigma ~1.0, amount 0.5) to
     emphasize ridge edges
   - Replaces the old bilinear upscale (`fpi_image_resize(img, 2, 2)`)

2. **`FP_DEVICE_RETRY_DIFFERENT_AREA`** (new retry code in `fp-device.h`)
   - Lets drivers request a different finger area when an enrollment scan
     is too similar to a previously stored scan

3. **Diversity threshold** (new in `fpi-image-device`)
   - Per-device configurable via `fpi_image_device_set_diversity_threshold()`
   - Compares each new enrollment scan against all stored scans using
     Bozorth3 matching
   - Rejects scans exceeding the threshold with `FP_DEVICE_RETRY_DIFFERENT_AREA`
   - Stores accepted prints in `enroll_prints` array for comparison

4. **ElanSPI driver tuning** (`drivers/elanspi.c`)
   - Replace `fpi_image_resize` → `fpi_image_enhance` for proper image
     processing
   - `bz3_threshold`: 24 → 10 (more forgiving Bozorth3 matching)
   - `nr_enroll_stages`: 7 → 11 (more enrollment data)
   - Enable diversity threshold at 18

### How This Was Built

This fork was developed interactively using **opencode/big-pickle**
— an AI coding assistant — by a non-embedded-engineer user with no C
or GObject expertise. The AI analyzed the libfprint source, proposed
changes, fixed bugs (including a double-free in the diversity check
code path), and iterated through ~15 build-test cycles over 2 days.

### Building

```bash
cd libfprint
meson setup builddir
ninja -C builddir
```

For temporary use without system installation:

```bash
sudo pkill fprintd
LD_LIBRARY_PATH=$PWD/builddir/libfprint /usr/lib/fprintd
# In another terminal:
fprintd-enroll
```

On reboot, the system reverts to the stock library automatically.

### Related Repos

- **fprintd fork**: https://github.com/dev-AbhinavNair/fprintd
  (needs matching `FP_DEVICE_RETRY_DIFFERENT_AREA` handling)


<!----------------------------------------------------------------------------->

[Documentation]: https://fprint.freedesktop.org/libfprint-dev/
[Contributors]: https://gitlab.freedesktop.org/libfprint/libfprint/-/graphs/master
[Unsupported]: https://gitlab.freedesktop.org/libfprint/wiki/-/wikis/Unsupported-Devices
[Supported]: https://fprint.freedesktop.org/supported-devices.html
[Website]: https://fprint.freedesktop.org/
[MailingList]: https://lists.freedesktop.org/mailman/listinfo/fprint
[IRC]: ircs://irc.oftc.net:6697/#fprint
[Matrix]: https://matrix.to/#/#fprint:matrix.org

[Contribute]: ./HACKING.md
[License]: ./COPYING

[University Of Manchester]: https://www.manchester.ac.uk/
[US Export Controlled]: https://fprint.freedesktop.org/us-export-control.html
[NBIS]: http://fingerprint.nist.gov/NBIS/index.html


<!---------------------------------[ Badges ]---------------------------------->

[Badge License]: https://img.shields.io/badge/License-LGPL2.1-015d93.svg?style=for-the-badge&labelColor=blue


<!---------------------------------[ Buttons ]--------------------------------->

[Button Documentation]: https://img.shields.io/badge/Documentation-04ACE6?style=for-the-badge&logoColor=white&logo=BookStack
[Button Contributors]: https://img.shields.io/badge/Contributors-FF4F8B?style=for-the-badge&logoColor=white&logo=ActiGraph
[Button Unsupported]: https://img.shields.io/badge/Unsupported_Devices-EF2D5E?style=for-the-badge&logoColor=white&logo=AdBlock
[Button Contribute]: https://img.shields.io/badge/Contribute-66459B?style=for-the-badge&logoColor=white&logo=Git
[Button Supported]: https://img.shields.io/badge/Supported_Devices-428813?style=for-the-badge&logoColor=white&logo=AdGuard
[Button Website]: https://img.shields.io/badge/Homepage-3B80AE?style=for-the-badge&logoColor=white&logo=freedesktopDotOrg
