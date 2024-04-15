#
#  Copyright (c) 2023, ARM Limited. All rights reserved.
#
#  SPDX-License-Identifier: BSD-2-Clause-Patent
#

################################################################################
#
# Defines Section - statements that will be processed to create a Makefile.
#
################################################################################
[Defines]
  PLATFORM_NAME                  = RdFremontCfg1
  PLATFORM_GUID                  = fd140b0f-4467-4314-aa69-cd0bd712e08e
  PLATFORM_VERSION               = 0.1
  DSC_SPECIFICATION              = 0x0001001B
  OUTPUT_DIRECTORY               = Build/$(PLATFORM_NAME)
  SUPPORTED_ARCHITECTURES        = AARCH64
  BUILD_TARGETS                  = NOOPT|DEBUG|RELEASE
  SKUID_IDENTIFIER               = DEFAULT
  FLASH_DEFINITION               = Platform/ARM/SgiPkg/SgiPlatform.fdf
  BOARD_DXE_FV_COMPONENTS        = Platform/ARM/SgiPkg/RdFremontCfg1/RdFremontCfg1.fdt.inc
  BUILD_NUMBER                   = 1

  DEFINE PCIE_ENABLE             = TRUE

# include common definitions from SgiPlatform.dsc
!include Platform/ARM/SgiPkg/SgiPlatform.dsc.inc
!include Platform/ARM/SgiPkg/SgiMemoryMap3.dsc.inc

# include common/basic libraries from MdePkg.
!include MdePkg/MdeLibs.dsc.inc

################################################################################
#
# Pcd Section - list of all EDK II PCD Entries defined by this Platform
#
################################################################################

[PcdsFixedAtBuild.common]
  # Verbose Printing
  gEfiMdePkgTokenSpaceGuid.PcdDebugPrintErrorLevel|0x80000004

  # GIC Base Addresses
  gArmTokenSpaceGuid.PcdGicDistributorBase|0x30000000
  gArmTokenSpaceGuid.PcdGicRedistributorsBase|0x30100000
  gArmSgiTokenSpaceGuid.PcdGicSize|0x200000

  # ARM Cores and Clusters
  gArmPlatformTokenSpaceGuid.PcdCoreCount|1
  gArmPlatformTokenSpaceGuid.PcdClusterCount|8

  # Error Injection
  gArmSgiTokenSpaceGuid.PcdEinjInstBufferBase|0xFB1DF000
  gArmSgiTokenSpaceGuid.PcdEinjInstBufferSize|0x10000
  gArmSgiTokenSpaceGuid.PcdEinjTriggerActionBase|0xFB1E0000
  gArmSgiTokenSpaceGuid.PcdEinjSetErrorTypeAddress|0xFB1E1000
#
################################################################################
#
# Components Section - list of all EDK II Modules needed by this Platform
#
################################################################################

[Components.common]
  Platform/ARM/SgiPkg/AcpiTables/RdFremontCfg1AcpiTables.inf
