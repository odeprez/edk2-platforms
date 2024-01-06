/** @file
  Function declared for IORT Acpi table generator.

  Copyright (c) 2024, ARM Limited. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef PCIE_IORT_TABLE_GENERATOR_H
#define PCIE_IORT_TABLE_GENERATOR_H

#include <Library/BaseLib.h>

#include <SgiPlatform.h>

EFI_STATUS
EFIAPI
GenerateAndInstallIortTable (
    IN SGI_PCIE_IO_BLOCK_LIST * CONST IoBlockList
    );

#endif /* PCIE_IORT_TABLE_GENERATOR_H */

