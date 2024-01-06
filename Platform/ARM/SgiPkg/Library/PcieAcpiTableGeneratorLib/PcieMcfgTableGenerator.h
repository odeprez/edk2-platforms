/** @file
  Function declared for Mcfg Acpi table generator.

  Copyright (c) 2022, ARM Limited. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
 **/

#ifndef PCIE_MCFG_TABLE_GENERATOR_H
#define PCIE_MCFG_TABLE_GENERATOR_H

#include <Library/BaseLib.h>

#include <SgiPlatform.h>

EFI_STATUS
EFIAPI
GenerateAndInstallMcfgTable (
    IN SGI_PCIE_IO_BLOCK_LIST * CONST IoBlockList
    );

#endif /* PCIE_MCFG_TABLE_GENERATOR_H */
