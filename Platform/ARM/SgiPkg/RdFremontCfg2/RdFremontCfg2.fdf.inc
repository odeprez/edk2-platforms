#
#  Copyright (c) 2023, Arm Limited. All rights reserved.<BR>
#
#  SPDX-License-Identifier: BSD-2-Clause-Patent
#

# Per-platform additional content of the DXE phase firmware volume

  # ACPI support
  INF RuleOverride=ACPITABLE Platform/ARM/SgiPkg/AcpiTables/RdFremontCfg2AcpiTables.inf
