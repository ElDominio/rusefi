// Controller for Harley Davidson Automatic Compression Release
//
// To make the starter's job easier, these bike engines have a solenoid
// that dumps cylinder pressure to atmosphere until the engine is spinning
// fast enough to actually have a chance at starting.
// We open the valve the instant the engine starts moving, then close it
// once the specified number of revolutions have occurred, plus some engine phase.
// This allows the valve to close at just the right moment that you don't get a
// weird half-charge if it closed mid way up on a compression stroke.

#include "pch.h"

#if EFI_HD_ACR


#endif // EFI_HD_ACR
