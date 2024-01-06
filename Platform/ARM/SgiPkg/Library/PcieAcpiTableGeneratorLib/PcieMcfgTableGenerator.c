/** @file
  Mcfg Pcie Table Generator.

  Copyright (c) 2022, Arm Limited. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent
 **/

#include <IndustryStandard/Acpi62.h>
#include <IndustryStandard/MemoryMappedConfigurationSpaceAccessTable.h>
#include <Library/AcpiLib.h>
#include <Library/ArmLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/AcpiTable.h>

#include "SgiAcpiHeader.h"
#include "SgiPlatform.h"

STATIC EFI_ACPI_MEMORY_MAPPED_CONFIGURATION_BASE_ADDRESS_TABLE_HEADER HeaderTemplate = {
  ARM_ACPI_HEADER (
      EFI_ACPI_6_2_PCI_EXPRESS_MEMORY_MAPPED_CONFIGURATION_SPACE_BASE_ADDRESS_DESCRIPTION_TABLE_SIGNATURE,
      EFI_ACPI_MEMORY_MAPPED_CONFIGURATION_BASE_ADDRESS_TABLE_HEADER,
      EFI_ACPI_MEMORY_MAPPED_CONFIGURATION_SPACE_ACCESS_TABLE_REVISION
      ),
  EFI_ACPI_RESERVED_QWORD
};

STATIC
VOID
GetSegmentEcam (
    IN SGI_PCIE_IO_BLOCK_LIST * CONST IoBlockList,
    IN UINT64 Segment,
    OUT UINT64 * EcamBaseAddress,
    OUT UINT64 * EcamSize,
    OUT UINTN  * BlocksProcessed
    )
{
  SGI_PCIE_IO_BLOCK *IoBlock;
  UINTN LoopIoBlock;
  UINTN LoopRootPort;

  *EcamBaseAddress = MAX_UINT64;
  *EcamSize = 0;

  IoBlock = IoBlockList->IoBlocks;
  for (LoopIoBlock = 0; LoopIoBlock < IoBlockList->BlockCount; LoopIoBlock++) {
    SGI_PCIE_DEVICE *RootPorts = IoBlock->RootPorts;
    if (Segment == IoBlock->Segment) {
      for (LoopRootPort = 0; LoopRootPort < IoBlock->Count; LoopRootPort++) {
        *EcamBaseAddress = MIN(*EcamBaseAddress,
            RootPorts[LoopRootPort].Ecam.Address +
            IoBlock->Translation);
        *EcamSize += RootPorts[LoopRootPort].Ecam.Size;
      }
      *BlocksProcessed += 1;
    }
    IoBlock = (SGI_PCIE_IO_BLOCK *) ((UINT8 *)IoBlock +
        sizeof (SGI_PCIE_IO_BLOCK) +
        (sizeof (SGI_PCIE_DEVICE) * IoBlock->Count));
  }
}

EFI_STATUS
EFIAPI
GenerateAndInstallMcfgTable (
    IN SGI_PCIE_IO_BLOCK_LIST * CONST IoBlockList
    )
{
  EFI_ACPI_MEMORY_MAPPED_CONFIGURATION_BASE_ADDRESS_TABLE_HEADER * Header;
  EFI_ACPI_MEMORY_MAPPED_ENHANCED_CONFIGURATION_SPACE_BASE_ADDRESS_ALLOCATION_STRUCTURE * Structure;
  EFI_ACPI_TABLE_PROTOCOL * AcpiProtocol;
  EFI_STATUS Status;
  UINT64 Segment = 0;
  UINT32 TableSize;
  UINTN BlocksProcessed = 0;
  UINTN TableHandle;
  UINT64 EcamStart;
  UINT64 EcamSize;

  if (IoBlockList == NULL) {
    Status = EFI_INVALID_PARAMETER;
    goto fail;
  }

  /* Each Chip has its own ECAM region. */
  TableSize = sizeof (EFI_ACPI_MEMORY_MAPPED_CONFIGURATION_BASE_ADDRESS_TABLE_HEADER) +
    (sizeof (EFI_ACPI_MEMORY_MAPPED_ENHANCED_CONFIGURATION_SPACE_BASE_ADDRESS_ALLOCATION_STRUCTURE) *
     IoBlockList->BlockCount);

  Header = (EFI_ACPI_MEMORY_MAPPED_CONFIGURATION_BASE_ADDRESS_TABLE_HEADER *)
    AllocateZeroPool (TableSize);

  if (Header == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto fail;
  }

  CopyMem (Header, &HeaderTemplate, sizeof (HeaderTemplate));
  Structure = (EFI_ACPI_MEMORY_MAPPED_ENHANCED_CONFIGURATION_SPACE_BASE_ADDRESS_ALLOCATION_STRUCTURE *)(Header + 1);
  while (BlocksProcessed < IoBlockList->BlockCount) {
    EcamStart = 0;
    EcamSize = 0;
    GetSegmentEcam(IoBlockList, Segment, &EcamStart, &EcamSize, &BlocksProcessed);
    if (EcamSize != 0) {
      Structure->BaseAddress = EcamStart;
      Structure->PciSegmentGroupNumber = Segment;
      Structure->StartBusNumber = 0;
      Structure->EndBusNumber = (EcamSize / SIZE_1MB) - 1;
      Structure->Reserved = EFI_ACPI_RESERVED_DWORD;
      Structure++;
    }
    Segment++;
  }

  Header->Header.Length = (UINT8 *)(Structure) - (UINT8 *)Header;

  Status = gBS->LocateProtocol (
      &gEfiAcpiTableProtocolGuid,
      NULL,
      (VOID**)&AcpiProtocol
      );

  if (EFI_ERROR (Status)) {
    DEBUG ((
          DEBUG_ERROR,
          "PCIE MCFG Table generation failed\n"
          "Failed to locate AcpiProtocol, Status = %r\n",
          Status
          ));
    goto fail;
  }


  Status = AcpiProtocol->InstallAcpiTable (
      AcpiProtocol,
      Header,
      Header->Header.Length,
      &TableHandle
      );

  if (EFI_ERROR (Status)) {
    DEBUG ((
          DEBUG_ERROR,
          "PCIE MCFG Table generation failed\n"
          "Failed to install MCFG table, Status = %r\n",
          Status
          ));
  }

fail:
  return Status;
}
