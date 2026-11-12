#pragma once

// No charger-strip pin is assigned. isCharging is not debounced here because
// there is no contact signal to debounce yet.
bool charger_sense_available();
