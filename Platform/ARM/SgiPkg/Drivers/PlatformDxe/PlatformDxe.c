/** @file
*
*  Copyright (c) 2018 - 2023, ARM Limited. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <IndustryStandard/Acpi.h>

#include <Library/AcpiLib.h>
#include <Library/ArmMmuLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/IoLib.h>
#include <Library/TimerLib.h>
#include <Library/PL011UartLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <IoVirtSoCExp.h>
#include <SgiPlatform.h>

// SMMUv3 Global Bypass Attribute (GBPA) register offset.
#define SMMU_GBPA                         0x0044

// SMMU_GBPA register fields.
#define SMMU_GBPA_UPDATE                  BIT31
#define SMMU_GBPA_ABORT                   BIT20
#define SMMU_REG_SIZE                     (0x4000000U)

VOID
InitVirtioDevices (
  VOID
  );

/**
  Initialize UART controllers connected to IO Virtualization block.

  Use PL011UartLib Library to initialize UART controllers that are present in
  the SoC expansion block. This SoC expansion block is connected to the IO
  virtualization block on Arm infrastructure reference design (RD) platforms.

  @retval  None
**/
STATIC
VOID
InitIoVirtSocExpBlkUartControllers (
  VOID
  )
{
  EFI_STATUS                 Status;
  EFI_PARITY_TYPE            Parity;
  EFI_STOP_BITS_TYPE         StopBits;
  UINT64                     BaudRate;
  UINT32                     ReceiveFifoDepth;
  UINT8                      DataBits;
  UINT8                      UartIdx;
  UINT32                     ChipIdx;
  UINT64                     UartAddr;

  if (FixedPcdGet32 (PcdIoVirtSocExpBlkUartEnable) == 0) {
    return;
  }

  ReceiveFifoDepth = 0;
  Parity = 1;
  DataBits = 8;
  StopBits = 1;
  BaudRate = 115200;

  for (ChipIdx = 0; ChipIdx < FixedPcdGet32 (PcdChipCount); ChipIdx++) {
    for (UartIdx = 0; UartIdx < 2; UartIdx++) {
      UartAddr = SGI_REMOTE_CHIP_MEM_OFFSET(ChipIdx) + UART_START(UartIdx);

      Status = PL011UartInitializePort (
                 (UINTN)UartAddr,
                 FixedPcdGet32 (PcdSerialDbgUartClkInHz),
                 &BaudRate,
                 &ReceiveFifoDepth,
                 &Parity,
                 &DataBits,
                 &StopBits
                 );

      if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_ERROR,
          "Failed to init PL011_UART%u on IO Virt Block port, status: %r\n",
          UartIdx,
          Status
          ));
      }
    }
  }
}

/**
  Search for a MPID in a list

  Performs a linear search for a specified MPID on the given linear
  list of MPIDs.

  @param[in]  MpidList  Pointer to list.
  @param[in]  Count     Number of the elements in the list.
  @param[in]  Mpid      Target MPID.

  @retval TRUE   MPID is present.
  @retval FALSE  MPID is not present.
**/
STATIC
BOOLEAN
CheckIfMpidIsPresent (
  IN UINT64  *MpidList,
  IN UINT64  Count,
  IN UINT64  Mpid
  )
{
  UINT64 Idx;

  for (Idx = 0; Idx < Count; Idx++) {
    if (MpidList[Idx] == Mpid) {
      return TRUE;
    }
  }

  return FALSE;
}

/**
  Disables isolated CPUs in the MADT table

  Parse the IsolatedCpuInfo from the Hob list and updates the MADT table to
  disable cpu's which are not available on the platfrom.

  @param[in] AcpiHeader  Points to the Madt table.
  @param[in] HobData     Points to the unusable cpuinfo in hoblist.
**/
STATIC
VOID
UpdateMadtTable (
  IN EFI_ACPI_DESCRIPTION_HEADER  *AcpiHeader,
  IN SGI_PLATFORM_DESCRIPTOR      *HobData
  )
{
  UINT8 *StructureListHead;
  UINT8 *StructureListTail;
  EFI_ACPI_6_4_GIC_STRUCTURE *GicStructure;
  BOOLEAN MpidPresent;

  if (HobData->IsolatedCpuList.Count == 0) {
    return;
  }

  StructureListHead =
    ((UINT8 *)AcpiHeader) +
    sizeof(EFI_ACPI_6_4_MULTIPLE_APIC_DESCRIPTION_TABLE_HEADER);
  StructureListTail = (UINT8 *)AcpiHeader + AcpiHeader->Length;

  // Locate ACPI GICC structure in the MADT table.
  while (StructureListHead < StructureListTail) {
    if (StructureListHead[0] == EFI_ACPI_6_4_GIC) {
      GicStructure = (EFI_ACPI_6_4_GIC_STRUCTURE *)StructureListHead;
      // Disable the CPU if its MPID is present in the list.
      MpidPresent = CheckIfMpidIsPresent(
        HobData->IsolatedCpuList.Mpid,
        HobData->IsolatedCpuList.Count,
        GicStructure->MPIDR
        );
      if (MpidPresent == TRUE) {
        DEBUG ((
          DEBUG_INFO,
          "Disabling Core: %lu, MPID: 0x%llx in MADT\n",
          GicStructure->AcpiProcessorUid,
          GicStructure->MPIDR
          ));
        GicStructure->Flags = 0;
      }
    }

    // Second element in the structure component header is length
    StructureListHead += StructureListHead[1];
  }
}

/**
  Callback to validate and/or update ACPI table.

  On finding a MADT table, disable the isolated CPUs in the MADT table. The
  list of isolated CPUs are obtained from the HOB data.

  @param[in] AcpiHeader  Target ACPI table.

  @retval  TURE   Table validated/updated successfully.
  @retval  FALSE  Error in Table validation/updation.
**/
STATIC
BOOLEAN
CheckAndUpdateAcpiTable (
  IN EFI_ACPI_DESCRIPTION_HEADER  *AcpiHeader
  )
{
  VOID *SystemIdHob;
  SGI_PLATFORM_DESCRIPTOR *HobData;

  // This check updates the MADT table to disable isolated CPUs present on the
  // platform.
  if (AcpiHeader->Signature == EFI_ACPI_1_0_APIC_SIGNATURE) {
    SystemIdHob = GetFirstGuidHob (&gArmSgiPlatformIdDescriptorGuid);
    if (SystemIdHob != NULL) {
      HobData = (SGI_PLATFORM_DESCRIPTOR *)GET_GUID_HOB_DATA (SystemIdHob);
      UpdateMadtTable (AcpiHeader, HobData);
    }
  }

  return TRUE;
}

/**
  Poll the SMMU register and test the value based on the mask.

  @param [in]  SmmuReg    Base address of the SMMU register.
  @param [in]  Mask       Mask of register bits to monitor.
  @param [in]  Value      Expected value.

  @retval EFI_SUCCESS     Success.
  @retval EFI_TIMEOUT     Timeout.
**/
STATIC
EFI_STATUS
EFIAPI
SmmuV3Poll (
  IN  UINT64 SmmuReg,
  IN  UINT32 Mask,
  IN  UINT32 Value
  )
{
  UINT32 RegVal;
  UINTN  Count;

  // Set 1ms timeout value.
  Count = 10;
  do {
    RegVal = MmioRead32 (SmmuReg);
    if ((RegVal & Mask) == Value) {
      return EFI_SUCCESS;
    }
    MicroSecondDelay (100);
  } while ((--Count) > 0);

  DEBUG ((DEBUG_ERROR, "Timeout polling SMMUv3 register @%p\n", SmmuReg));
  DEBUG ((
    DEBUG_ERROR,
    "Read value 0x%x, expected 0x%x\n",
    RegVal,
    ((Value == 0) ? (RegVal & ~Mask) : (RegVal | Mask))
    ));
  return EFI_TIMEOUT;
}

/**
  Initialise the SMMUv3 to set Non-secure streams to bypass the SMMU.

  @param [in]  SmmuReg    Base address of the SMMUv3.

  @retval EFI_SUCCESS     Success.
  @retval EFI_TIMEOUT     Timeout.
**/
STATIC
EFI_STATUS
EFIAPI
SmmuV3Init (
  IN  UINT64 SmmuBase
  )
{
  EFI_STATUS  Status;
  UINT32      RegVal;

  // Attribute update has completed when SMMU_(S)_GBPA.Update bit is 0.
  Status = SmmuV3Poll (SmmuBase + SMMU_GBPA, SMMU_GBPA_UPDATE, 0);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // SMMU_(S)_CR0 resets to zero with all streams bypassing the SMMU,
  // so just abort all incoming transactions.
  RegVal = MmioRead32 (SmmuBase + SMMU_GBPA);

  // TF-A configures the SMMUv3 to abort all incoming transactions.
  // Clear the SMMU_GBPA.ABORT to allow Non-secure streams to bypass
  // the SMMU.
  RegVal &= ~SMMU_GBPA_ABORT;
  RegVal |= SMMU_GBPA_UPDATE;

  MmioWrite32 (SmmuBase + SMMU_GBPA, RegVal);

  Status = SmmuV3Poll (SmmuBase + SMMU_GBPA, SMMU_GBPA_UPDATE, 0);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
ArmSgiPkgEntryPoint (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  SGI_PCIE_IO_BLOCK_LIST *IoBlockList;
  SGI_PCIE_IO_BLOCK *IoBlock;
  VOID *PcieMmapTableHob;
  UINTN LoopIoBlock;
  EFI_STATUS              Status;

  Status = LocateAndInstallAcpiFromFvConditional (
             &gArmSgiAcpiTablesGuid,
             &CheckAndUpdateAcpiTable
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to install ACPI tables\n", __FUNCTION__));
    return Status;
  }

  InitVirtioDevices ();
  InitIoVirtSocExpBlkUartControllers ();

  PcieMmapTableHob = GetFirstGuidHob (&gArmSgiPcieMmapTablesGuid);

  if (PcieMmapTableHob != NULL) {
    IoBlockList = (SGI_PCIE_IO_BLOCK_LIST *)GET_GUID_HOB_DATA (PcieMmapTableHob);
    IoBlock = IoBlockList->IoBlocks;
    for (LoopIoBlock = 0; LoopIoBlock < IoBlockList->BlockCount;
        LoopIoBlock++) {
      Status = ArmSetMemoryAttributes (
                IoBlock->SmmuBase,
                SMMU_REG_SIZE,
                EFI_MEMORY_UC,
                0
                );
      if (EFI_ERROR(Status)) {
        return Status;
      }

      Status = SmmuV3Init (IoBlock->SmmuBase);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR,
              "%a: Failed to initialise SMMUv3 in bypass mode.\n", __FUNCTION__));
      }
      IoBlock = (SGI_PCIE_IO_BLOCK *) ((UINT8 *)IoBlock +
          sizeof (SGI_PCIE_IO_BLOCK) +
          (sizeof (SGI_PCIE_DEVICE) * IoBlock->Count));
    }
  }
  return Status;
}
