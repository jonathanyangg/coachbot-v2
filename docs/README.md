# Docs

Locked plan: [v2_os_plan.md](v2_os_plan.md). Wire protocol: [../proto/ops.md](../proto/ops.md). What the code actually does: [../README.md](../README.md).

The plan's diagram shows the Feather ESP32 owning motors. That is **unconfirmed**. Motors and the LED use the Pi BCM map from `bottom_board_v2.sch` (TB1 and SV1). The ESP32 UART driver is a stub and defines no frames.

Open: whether anything moves to the ESP32, Vive/nucifera bytes (v1 never published them), radio for `sendMsg`. LED color order on SV1 is the v1 R/G/B assignment; the sheet does not name the colors.
