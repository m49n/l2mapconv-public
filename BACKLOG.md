# Backlog

## Territory render UI

- [ ] Show separate missing, unsupported/corrupt and simplified issue counts
  alongside the total warning count. These categories already exist in the
  JSON report; the current UI shows their combined count.

## Geodata build UI

- [ ] Add a **Build selected maps** action to the Maps panel. It should build
  only the regions selected with checkboxes, rather than all available maps.
- [ ] Add a **Generate client DAT files** checkbox to the build options. When
  enabled, build `XX_YY_conv.dat` alongside the server `.l2j` file; when
  disabled, build only `.l2j`. The current CLI always writes both formats, so
  this requires making client DAT export optional in the build pipeline.
