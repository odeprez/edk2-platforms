/** @file
*  PCI Host Bridge Library instance for ARM SGI platforms
*
*  Copyright (c) 2018, ARM Limited. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <PiDxe.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/HobLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/PciHostBridgeLib.h>
#include <Protocol/PciHostBridgeResourceAllocation.h>
#include <Protocol/PciRootBridgeIo.h>

#include <SgiPlatform.h>

GLOBAL_REMOVE_IF_UNREFERENCED
STATIC CHAR16 CONST * CONST mPciHostBridgeLibAcpiAddressSpaceTypeStr[] = {
  L"Mem", L"I/O", L"Bus"
};

STATIC BOOLEAN DynamicTableGenerationEnabled;

#pragma pack(1)
typedef struct {
  ACPI_HID_DEVICE_PATH     AcpiDevicePath;
  EFI_DEVICE_PATH_PROTOCOL EndDevicePath;
} EFI_PCI_ROOT_BRIDGE_DEVICE_PATH;
#pragma pack ()

STATIC EFI_PCI_ROOT_BRIDGE_DEVICE_PATH mEfiPciRootBridgeDevicePath = {
  {
    {
      ACPI_DEVICE_PATH,
      ACPI_DP,
      {
        (UINT8) (sizeof (ACPI_HID_DEVICE_PATH)),
        (UINT8) ((sizeof (ACPI_HID_DEVICE_PATH)) >> 8)
      }
    },
    EISA_PNP_ID (0x0A08), // PCIe
    0
  }, {
    END_DEVICE_PATH_TYPE,
    END_ENTIRE_DEVICE_PATH_SUBTYPE,
    {
      END_DEVICE_PATH_LENGTH,
      0
    }
  }
};

STATIC PCI_ROOT_BRIDGE mPciRootBridge[] = {
  {
    0,                                              // Segment
    0,                                              // Supports
    0,                                              // Attributes
    TRUE,                                           // DmaAbove4G
    FALSE,                                          // NoExtendedConfigSpace
    FALSE,                                          // ResourceAssigned
    EFI_PCI_HOST_BRIDGE_COMBINE_MEM_PMEM |          // AllocationAttributes
    EFI_PCI_HOST_BRIDGE_MEM64_DECODE,
    {
      // Bus
      FixedPcdGet32 (PcdPciBusMin),
      FixedPcdGet32 (PcdPciBusMax)
    }, {
      // Io
      FixedPcdGet64 (PcdPciIoBase),
      FixedPcdGet64 (PcdPciIoBase) + FixedPcdGet64 (PcdPciIoSize) - 1
    }, {
      // Mem
      FixedPcdGet32 (PcdPciMmio32Base),
      FixedPcdGet32 (PcdPciMmio32Base) + FixedPcdGet32 (PcdPciMmio32Size) - 1
    }, {
      // MemAbove4G
      FixedPcdGet64 (PcdPciMmio64Base),
      FixedPcdGet64 (PcdPciMmio64Base) + FixedPcdGet64 (PcdPciMmio64Size) - 1
    }, {
      // PMem
      MAX_UINT64,
      0
    }, {
      // PMemAbove4G
      MAX_UINT64,
      0
    },
    (EFI_DEVICE_PATH_PROTOCOL *)&mEfiPciRootBridgeDevicePath
  },
};

STATIC PCI_ROOT_BRIDGE mRootBridgeTemplate[] = {
  {
    0,                                              // Segment
    0,                                              // Supports
    0,                                              // Attributes
    TRUE,                                           // DmaAbove4G
    FALSE,                                          // NoExtendedConfigSpace
    FALSE,                                          // ResourceAssigned
    EFI_PCI_HOST_BRIDGE_COMBINE_MEM_PMEM |          // AllocationAttributes
    EFI_PCI_HOST_BRIDGE_MEM64_DECODE,
    {
      // Bus
      0,
      0,
    }, {
      // Io
      0,
      0,
      0,
    }, {
      // Mem
      MAX_UINT64,
      0,
      0,
    }, {
      // MemAbove4G
      MAX_UINT64,
      0,
      0,
    }, {
      // PMem
      MAX_UINT64,
      0
    }, {
      // PMemAbove4G
      MAX_UINT64,
      0,
      0,
    },
    (EFI_DEVICE_PATH_PROTOCOL *)&mEfiPciRootBridgeDevicePath
  }
};

STATIC
UINT8
GetPcieRootPortCount(
    SGI_PCIE_IO_BLOCK_LIST *IoBlockList
)
{
  SGI_PCIE_IO_BLOCK *IoBlock;
  SGI_PCIE_DEVICE *RootPorts;
  UINTN Count = 0;
  UINTN LoopRootPort;
  UINTN LoopIoBlock;

  IoBlock = IoBlockList->IoBlocks;
  for (LoopIoBlock = 0; LoopIoBlock < IoBlockList->BlockCount; LoopIoBlock++) {
    // EDK2 Supports only one segment.
    if (IoBlock->Segment == 0) {
      RootPorts = IoBlock->RootPorts;
      for (LoopRootPort = 0; LoopRootPort < IoBlock->Count; LoopRootPort++) {
        if (RootPorts[LoopRootPort].Ecam.Size != 0) {
          Count++;
        }
      }
    }
    IoBlock = (SGI_PCIE_IO_BLOCK *) ((UINT8 *)IoBlock +
                sizeof (SGI_PCIE_IO_BLOCK) +
                (sizeof (SGI_PCIE_DEVICE) * IoBlock->Count));
  }

  return Count;
}

STATIC
EFI_STATUS
GenerateRootBridge (
    PCI_ROOT_BRIDGE                     *Bridge,
    SGI_PCIE_DEVICE                     *RootPort,
    UINT64                               Translation,
    UINTN                                Segment
    )
{
  EFI_PCI_ROOT_BRIDGE_DEVICE_PATH *DevicePath;
  STATIC UINTN UID = 0;

  CopyMem (Bridge, mRootBridgeTemplate, sizeof *Bridge);
  Bridge->Segment = Segment;

  if (RootPort->Bus.Size != 0) {
    Bridge->Bus.Base = RootPort->Bus.Address;
    Bridge->Bus.Limit = RootPort->Bus.Address + RootPort->Bus.Size - 1;
  }

  if (RootPort->MmioL.Size != 0) {
    Bridge->Mem.Base = RootPort->MmioL.Address;
    Bridge->Mem.Limit =
      RootPort->MmioL.Address + RootPort->MmioL.Size - 1;
    Bridge->Mem.Translation = Translation;
  }

  if (RootPort->MmioH.Size != 0) {
    Bridge->MemAbove4G.Base = RootPort->MmioH.Address;
    Bridge->MemAbove4G.Limit =
      RootPort->MmioH.Address + RootPort->MmioH.Size - 1;
  }

  DevicePath = AllocateCopyPool(
      sizeof mEfiPciRootBridgeDevicePath,
      &mEfiPciRootBridgeDevicePath
      );
  if (DevicePath == NULL) {
    DEBUG ((
          DEBUG_ERROR,
          "[%a:%d] - AllocatePool failed!\n",
          __FUNCTION__,
          __LINE__
          ));
    return EFI_OUT_OF_RESOURCES;
  }

  DevicePath->AcpiDevicePath.UID = UID++;
  Bridge->DevicePath = (EFI_DEVICE_PATH_PROTOCOL *)DevicePath;
  return EFI_SUCCESS;
}

PCI_ROOT_BRIDGE *
EFIAPI
PciGenerateHostBridgeInfo (
  SGI_PCIE_IO_BLOCK_LIST *IoBlockList,
  UINTN *Count
  )
{
  EFI_STATUS Status;
  SGI_PCIE_IO_BLOCK *IoBlock;
  UINT8 Idx;
  UINTN LoopRootPort;
  UINTN LoopIoBlock;
  PCI_ROOT_BRIDGE *Bridges;

  *Count = GetPcieRootPortCount(IoBlockList);
  if (*Count == 0) {
    return NULL;
  }

  Bridges = AllocatePool (*Count * sizeof *Bridges);
  if (Bridges == NULL) {
     DEBUG ((
           DEBUG_ERROR,
           "[%a:%d] - AllocatePool failed!\n",
           __FUNCTION__,
           __LINE__
           ));
     return NULL;
  }

  IoBlock = IoBlockList->IoBlocks;
  for (LoopIoBlock = 0; LoopIoBlock < IoBlockList->BlockCount; LoopIoBlock++) {
    // EDK2 support only one segment. Use Segment 0 for device detection.
    SGI_PCIE_DEVICE *RootPorts = IoBlock->RootPorts;
    if (IoBlock->Segment == 0) {
      for (LoopRootPort = 0; LoopRootPort < IoBlock->Count; LoopRootPort++) {
        if (RootPorts[LoopRootPort].Ecam.Size != 0) {
          Status = GenerateRootBridge(
              &Bridges[Idx],
              &RootPorts[LoopRootPort],
              IoBlock->Translation,
              IoBlock->Segment
              );
          if (!EFI_ERROR(Status)) {
            Idx++;
          }
        }
      }
    }
    IoBlock = (SGI_PCIE_IO_BLOCK *) ((UINT8 *)IoBlock +
               sizeof (SGI_PCIE_IO_BLOCK) +
                (sizeof (SGI_PCIE_DEVICE) * IoBlock->Count));
  }

  return Bridges;
}

/**
  Return all the root bridge instances in an array.

  @param Count  Return the count of root bridge instances.

  @return All the root bridge instances in an array.
          The array should be passed into PciHostBridgeFreeRootBridges()
          when it's not used.
**/
PCI_ROOT_BRIDGE *
EFIAPI
PciHostBridgeGetRootBridges (
  UINTN *Count
  )
{
  PCI_ROOT_BRIDGE *Bridges;
  VOID *PcieMmapTableHob;

  PcieMmapTableHob = GetFirstGuidHob (&gArmSgiPcieMmapTablesGuid);
  if (PcieMmapTableHob != NULL) {
    Bridges = PciGenerateHostBridgeInfo(
        (SGI_PCIE_IO_BLOCK_LIST *)GET_GUID_HOB_DATA (PcieMmapTableHob), Count);
    DynamicTableGenerationEnabled = TRUE;
  } else {
    *Count = ARRAY_SIZE (mPciRootBridge);
    Bridges = mPciRootBridge;
    DynamicTableGenerationEnabled = FALSE;
  }

  return Bridges;
}

/**
  Free the root bridge instances array returned from PciHostBridgeGetRootBridges().

  @param Bridges The root bridge instances array.
  @param Count   The count of the array.
**/
VOID
EFIAPI
PciHostBridgeFreeRootBridges (
  PCI_ROOT_BRIDGE *Bridges,
  UINTN           Count
  )
{
  UINTN Index;

  if (DynamicTableGenerationEnabled) {
    for (Index = 0; Index < Count; Index++) {
      FreePool (Bridges[Index].DevicePath);
    }

    if (Bridges != NULL) {
      FreePool (Bridges);
    }
  }
}

/**
  Inform the platform that the resource conflict happens.

  @param HostBridgeHandle Handle of the Host Bridge.
  @param Configuration    Pointer to PCI I/O and PCI memory resource
                          descriptors. The Configuration contains the resources
                          for all the root bridges. The resource for each root
                          bridge is terminated with END descriptor and an
                          additional END is appended indicating the end of the
                          entire resources. The resource descriptor field
                          values follow the description in
                          EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL
                          .SubmitResources().
**/
VOID
EFIAPI
PciHostBridgeResourceConflict (
  EFI_HANDLE                        HostBridgeHandle,
  VOID                              *Configuration
  )
{
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *Descriptor;
  UINTN                             RootBridgeIndex;
  DEBUG ((DEBUG_ERROR, "PciHostBridge: Resource conflict happens!\n"));

  RootBridgeIndex = 0;
  Descriptor = (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *) Configuration;
  while (Descriptor->Desc == ACPI_ADDRESS_SPACE_DESCRIPTOR) {
    DEBUG ((DEBUG_ERROR, "RootBridge[%d]:\n", RootBridgeIndex++));
    for (; Descriptor->Desc == ACPI_ADDRESS_SPACE_DESCRIPTOR; Descriptor++) {
      ASSERT (Descriptor->ResType <
              (sizeof (mPciHostBridgeLibAcpiAddressSpaceTypeStr) /
               sizeof (mPciHostBridgeLibAcpiAddressSpaceTypeStr[0])
               )
              );
      DEBUG ((DEBUG_ERROR, " %s: Length/Alignment = 0x%lx / 0x%lx\n",
              mPciHostBridgeLibAcpiAddressSpaceTypeStr[Descriptor->ResType],
              Descriptor->AddrLen, Descriptor->AddrRangeMax
              ));
      if (Descriptor->ResType == ACPI_ADDRESS_SPACE_TYPE_MEM) {
        DEBUG ((DEBUG_ERROR, "     Granularity/SpecificFlag = %ld / %02x%s\n",
                Descriptor->AddrSpaceGranularity, Descriptor->SpecificFlag,
                ((Descriptor->SpecificFlag &
                  EFI_ACPI_MEMORY_RESOURCE_SPECIFIC_FLAG_CACHEABLE_PREFETCHABLE
                  ) != 0) ? L" (Prefetchable)" : L""
                ));
      }
    }
    //
    // Skip the END descriptor for root bridge
    //
    ASSERT (Descriptor->Desc == ACPI_END_TAG_DESCRIPTOR);
    Descriptor = (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *)(
                   (EFI_ACPI_END_TAG_DESCRIPTOR *)Descriptor + 1
                   );
  }
}
