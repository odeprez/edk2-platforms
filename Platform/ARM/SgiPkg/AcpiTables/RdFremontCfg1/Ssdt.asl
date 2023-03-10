/** @file
*  Secondary System Description Table (SSDT)
*
*  Copyright (c) 2023, ARM Limited. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include "SgiAcpiHeader.h"

DefinitionBlock("SsdtPci.aml", "SSDT", 2, "ARMLTD", "ARMSGI", EFI_ACPI_ARM_OEM_REVISION) {
  Scope (_SB) {
    // PCI Root Complexes
    EFI_ACPI_PCI_RC_INIT(0, 0, 0, 63, 64, 0x60000000, 0x67FFFFFF, 0x8000000, 0x4040000000, 0x503FFFFFFF, 0x1000000000, 0x4000000000, 0x4003FFFFFF, 0x4000000)
  }
}
