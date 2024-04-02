## @file
#  Flash Description include file for RD-N2-Cfg1 platform.
#
#  Copyright (c) 2021, Arm Limited. All rights reserved.
#
#  SPDX-License-Identifier: BSD-2-Clause-Patent
##

# Per-platform additional content of the DXE phase firmware volume

  # ACPI support
!if $(EDK2_ENABLE_KERNEL_FIRST) == TRUE
  INF RuleOverride=ACPITABLE Platform/ARM/SgiPkg/AcpiTables/RdN2Cfg1AcpiTablesRas.inf
!else
!if $(EDK2_ERROR_INJ_EN) == TRUE
  INF RuleOverride=ACPITABLE Platform/ARM/SgiPkg/AcpiTables/RdN2Cfg1AcpiTablesEinj.inf
!else
  INF RuleOverride=ACPITABLE Platform/ARM/SgiPkg/AcpiTables/RdN2Cfg1AcpiTables.inf
!endif
!endif
