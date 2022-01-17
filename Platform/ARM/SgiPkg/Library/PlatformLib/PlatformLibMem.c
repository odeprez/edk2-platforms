/** @file
*
*  Copyright (c) 2018-2024, Arm Limited. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <Library/ArmPlatformLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/PcdLib.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>

#include <IoVirtSoCExp.h>
#include <SgiPlatform.h>

// Total number of descriptors, including the final "end-of-table" descriptor.
#define MAX_VIRTUAL_MEMORY_MAP_DESCRIPTORS                                     \
          ((14 + (FixedPcdGet32 (PcdChipCount) * 2)) +                         \
          (FeaturePcdGet (PcdEinjSupported)) +                                 \
           ((FeaturePcdGet (PcdPcieEnable) == TRUE) ? 1 : 0) +                 \
           (FixedPcdGet32 (PcdIoVirtSocExpBlkUartEnable) *                     \
            FixedPcdGet32 (PcdChipCount) * 2))

// Memory Map descriptor for IO Virtualization SoC Expansion Block UART
#define IO_VIRT_SOC_EXP_BLK_UART_MMAP(UartIdx, ChipIdx)                        \
  VirtualMemoryTable[++Index].PhysicalBase =                                   \
    SGI_REMOTE_CHIP_MEM_OFFSET(ChipIdx) + UART_START(UartIdx);                 \
  VirtualMemoryTable[Index].VirtualBase    =                                   \
    SGI_REMOTE_CHIP_MEM_OFFSET(ChipIdx) + UART_START(UartIdx);                 \
  VirtualMemoryTable[Index].Length         = SIZE_64KB;                        \
  VirtualMemoryTable[Index].Attributes     = ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;


#if (FixedPcdGet32 (PcdChipCount) > 1)
/**
  Return the physical address of the second DRAM on remote chip.

  @param[in]  ChipIdx   The identifier of the chip
**/
STATIC
EFI_PHYSICAL_ADDRESS
GetRemoteDram2BaseAddr (
  IN UINT8 ChipIdx
  )
{
  UINT8                         SgiProductIdx;

  SgiProductIdx = SgiGetProductId();
  if (SgiProductIdx == RdFremontCfg2)
    return DRAM_BLOCK2_BASE_REMOTE_FREMONT(ChipIdx);

  return DRAM_BLOCK2_BASE_REMOTE(ChipIdx);
}
#endif

/**
  Returns the Virtual Memory Map of the platform.

  This Virtual Memory Map is used by MemoryInitPei Module to initialize the MMU
  on your platform.

  @param[out] VirtualMemoryMap Array of ARM_MEMORY_REGION_DESCRIPTOR describing
                               a Physical-to-Virtual Memory mapping. This array
                               must be ended by a zero-filled entry.
**/
VOID
ArmPlatformGetVirtualMemoryMap (
  IN ARM_MEMORY_REGION_DESCRIPTOR** VirtualMemoryMap
  )
{
  UINTN                         Index;
  ARM_MEMORY_REGION_DESCRIPTOR  *VirtualMemoryTable;
  EFI_RESOURCE_ATTRIBUTE_TYPE   ResourceAttributes;
  UINTN                         TotalMemoryMapDescirptors;

  ResourceAttributes =
    EFI_RESOURCE_ATTRIBUTE_PRESENT |
    EFI_RESOURCE_ATTRIBUTE_INITIALIZED |
    EFI_RESOURCE_ATTRIBUTE_UNCACHEABLE |
    EFI_RESOURCE_ATTRIBUTE_WRITE_COMBINEABLE |
    EFI_RESOURCE_ATTRIBUTE_WRITE_THROUGH_CACHEABLE |
    EFI_RESOURCE_ATTRIBUTE_WRITE_BACK_CACHEABLE |
    EFI_RESOURCE_ATTRIBUTE_TESTED;

  BuildResourceDescriptorHob (
    EFI_RESOURCE_SYSTEM_MEMORY,
    ResourceAttributes,
    FixedPcdGet64 (PcdDramBlock2Base),
    FixedPcdGet64 (PcdDramBlock2Size));

#if (FixedPcdGet32 (PcdChipCount) > 1)
  BuildResourceDescriptorHob (
     EFI_RESOURCE_SYSTEM_MEMORY,
     ResourceAttributes,
     SYSTEM_MEMORY_BASE_REMOTE (1),
     PcdGet64 (PcdSystemMemorySize));

   BuildResourceDescriptorHob (
     EFI_RESOURCE_SYSTEM_MEMORY,
     ResourceAttributes,
     GetRemoteDram2BaseAddr (1),
     FixedPcdGet64 (PcdDramBlock2Size));

#if (FixedPcdGet32 (PcdChipCount) > 2)
  BuildResourceDescriptorHob (
     EFI_RESOURCE_SYSTEM_MEMORY,
     ResourceAttributes,
     SYSTEM_MEMORY_BASE_REMOTE (2),
     FixedPcdGet64 (PcdSystemMemorySize));

   BuildResourceDescriptorHob (
     EFI_RESOURCE_SYSTEM_MEMORY,
     ResourceAttributes,
     GetRemoteDram2BaseAddr (2),
     FixedPcdGet64 (PcdDramBlock2Size));

#if (FixedPcdGet32 (PcdChipCount) > 3)
  BuildResourceDescriptorHob (
     EFI_RESOURCE_SYSTEM_MEMORY,
     ResourceAttributes,
     SYSTEM_MEMORY_BASE_REMOTE (3),
     FixedPcdGet64 (PcdSystemMemorySize));

   BuildResourceDescriptorHob (
     EFI_RESOURCE_SYSTEM_MEMORY,
     ResourceAttributes,
     GetRemoteDram2BaseAddr (3),
     FixedPcdGet64 (PcdDramBlock2Size));
#endif
#endif
#endif

  ASSERT (VirtualMemoryMap != NULL);
  Index = 0;

  TotalMemoryMapDescirptors = MAX_VIRTUAL_MEMORY_MAP_DESCRIPTORS;

#if (FixedPcdGetBool (PcdRemoteCxlMemory))
   TotalMemoryMapDescirptors += 1;
#endif

  VirtualMemoryTable = (ARM_MEMORY_REGION_DESCRIPTOR*)AllocatePages
                       (EFI_SIZE_TO_PAGES (sizeof (ARM_MEMORY_REGION_DESCRIPTOR) *
                       TotalMemoryMapDescirptors));
  if (VirtualMemoryTable == NULL) {
    return;
  }

  // Expansion AXI - SMC Chip Select 0 (NOR Flash)
  VirtualMemoryTable[Index].PhysicalBase    = FixedPcdGet64 (PcdSmcCs0Base);
  VirtualMemoryTable[Index].VirtualBase     = FixedPcdGet64 (PcdSmcCs0Base);
  VirtualMemoryTable[Index].Length          = SIZE_64MB;
  VirtualMemoryTable[Index].Attributes      = ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;

  // Expansion AXI - SMC Chip Select 1 (NOR Flash)
  VirtualMemoryTable[++Index].PhysicalBase  = FixedPcdGet64 (PcdSmcCs1Base);
  VirtualMemoryTable[Index].VirtualBase     = FixedPcdGet64 (PcdSmcCs1Base);
  VirtualMemoryTable[Index].Length          = SIZE_64MB;
  VirtualMemoryTable[Index].Attributes      = ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;

  // Expansion AXI - System Peripherals
  VirtualMemoryTable[++Index].PhysicalBase  = FixedPcdGet64 (PcdSysPeriphBase);
  VirtualMemoryTable[Index].VirtualBase     = FixedPcdGet64 (PcdSysPeriphBase);
  VirtualMemoryTable[Index].Length          = SIZE_32MB;
  VirtualMemoryTable[Index].Attributes      = ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;

  // Sub System Peripherals - Generic Watchdog
  VirtualMemoryTable[++Index].PhysicalBase  = FixedPcdGet32 (PcdWdogBase);
  VirtualMemoryTable[Index].VirtualBase     = FixedPcdGet32 (PcdWdogBase);
  VirtualMemoryTable[Index].Length          = FixedPcdGet32 (PcdWdogSize);
  VirtualMemoryTable[Index].Attributes      = ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;

  // Sub System Peripherals - GIC-600
  VirtualMemoryTable[++Index].PhysicalBase  = FixedPcdGet64(PcdGicDistributorBase);
  VirtualMemoryTable[Index].VirtualBase     = FixedPcdGet64(PcdGicDistributorBase);
  VirtualMemoryTable[Index].Length          = FixedPcdGet64(PcdGicSize);
  VirtualMemoryTable[Index].Attributes      = ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;

  // Sub System Peripherals - Counter
  VirtualMemoryTable[++Index].PhysicalBase  = FixedPcdGet32 (PcdTimerCounterReadBase);
  VirtualMemoryTable[Index].VirtualBase     = FixedPcdGet32 (PcdTimerCounterReadBase);
  VirtualMemoryTable[Index].Length          = FixedPcdGet32 (PcdTimerCounterReadSize);
  VirtualMemoryTable[Index].Attributes      = ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;

  // Sub System Peripherals - Timer Control
  VirtualMemoryTable[++Index].PhysicalBase  = FixedPcdGet32 (PcdTimerControlBase);
  VirtualMemoryTable[Index].VirtualBase     = FixedPcdGet32 (PcdTimerControlBase);
  VirtualMemoryTable[Index].Length          = FixedPcdGet32 (PcdTimerControlSize);
  VirtualMemoryTable[Index].Attributes      = ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;

  // Sub System Peripherals - Timer Base0
  VirtualMemoryTable[++Index].PhysicalBase  = FixedPcdGet32 (PcdTimerBase0Base);
  VirtualMemoryTable[Index].VirtualBase     = FixedPcdGet32 (PcdTimerBase0Base);
  VirtualMemoryTable[Index].Length          = FixedPcdGet32 (PcdTimerBase0Size);
  VirtualMemoryTable[Index].Attributes      = ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;

  // Sub System Peripherals - SMMU
  VirtualMemoryTable[++Index].PhysicalBase  = FixedPcdGet64 (PcdSmmuBase);
  VirtualMemoryTable[Index].VirtualBase     = FixedPcdGet64 (PcdSmmuBase);
  VirtualMemoryTable[Index].Length          = FixedPcdGet32 (PcdSmmuSize);
  VirtualMemoryTable[Index].Attributes      = ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;

  // Expansion AXI - Platform Peripherals - HDLCD1
  VirtualMemoryTable[++Index].PhysicalBase  = FixedPcdGet32 (PcdArmHdLcdBase);
  VirtualMemoryTable[Index].VirtualBase     = FixedPcdGet32 (PcdArmHdLcdBase);
  VirtualMemoryTable[Index].Length          = SIZE_64KB;
  VirtualMemoryTable[Index].Attributes      = ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;

  // Expansion AXI - Platform Peripherals - UART1
  VirtualMemoryTable[++Index].PhysicalBase  = FixedPcdGet64 (PcdSerialRegisterBase);
  VirtualMemoryTable[Index].VirtualBase     = FixedPcdGet64 (PcdSerialRegisterBase);
  VirtualMemoryTable[Index].Length          = SIZE_64KB;
  VirtualMemoryTable[Index].Attributes      = ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;

#if (FixedPcdGet32 (PcdIoVirtSocExpBlkUartEnable) == 1)
  // Chip-0 IO Virtualization SoC Expansion Block - UART0
  IO_VIRT_SOC_EXP_BLK_UART_MMAP(0, 0)
  // Chip-0 IO Virtualization SoC Expansion Block - UART1
  IO_VIRT_SOC_EXP_BLK_UART_MMAP(1, 0)
#if (FixedPcdGet32 (PcdChipCount) > 1)
  // Chip-1 IO Virtualization SoC Expansion Block - UART0
  IO_VIRT_SOC_EXP_BLK_UART_MMAP(0, 1)
  // Chip-1 IO Virtualization SoC Expansion Block - UART1
  IO_VIRT_SOC_EXP_BLK_UART_MMAP(1, 1)
#if (FixedPcdGet32 (PcdChipCount) > 2)
  // Chip-2 IO Virtualization SoC Expansion Block - UART0
  IO_VIRT_SOC_EXP_BLK_UART_MMAP(0, 2)
  // Chip-2 IO Virtualization SoC Expansion Block - UART1
  IO_VIRT_SOC_EXP_BLK_UART_MMAP(1, 2)
#if (FixedPcdGet32 (PcdChipCount) > 3)
  // Chip-3 IO Virtualization SoC Expansion Block - UART0
  IO_VIRT_SOC_EXP_BLK_UART_MMAP(0, 3)
  // Chip-3 IO Virtualization SoC Expansion Block - UART1
  IO_VIRT_SOC_EXP_BLK_UART_MMAP(1, 3)
#endif
#endif
#endif
#endif
  // Expansion AXI - Debug UART
  VirtualMemoryTable[++Index].PhysicalBase  = FixedPcdGet64 (PcdSerialDbgRegisterBase);
  VirtualMemoryTable[Index].VirtualBase     = FixedPcdGet64 (PcdSerialDbgRegisterBase);
  VirtualMemoryTable[Index].Length          = SIZE_64KB;
  VirtualMemoryTable[Index].Attributes      = ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;

  // DDR - (2GB - 16MB)
  VirtualMemoryTable[++Index].PhysicalBase  = PcdGet64 (PcdSystemMemoryBase);
  VirtualMemoryTable[Index].VirtualBase     = PcdGet64 (PcdSystemMemoryBase);
  VirtualMemoryTable[Index].Length          = PcdGet64 (PcdSystemMemorySize);
  VirtualMemoryTable[Index].Attributes      = ARM_MEMORY_REGION_ATTRIBUTE_WRITE_BACK;

  // DDR - Second Block
  VirtualMemoryTable[++Index].PhysicalBase  = PcdGet64 (PcdDramBlock2Base);
  VirtualMemoryTable[Index].VirtualBase     = PcdGet64 (PcdDramBlock2Base);
  VirtualMemoryTable[Index].Length          = PcdGet64 (PcdDramBlock2Size);
  VirtualMemoryTable[Index].Attributes      = ARM_MEMORY_REGION_ATTRIBUTE_WRITE_BACK;

#if (FixedPcdGetBool (PcdRemoteCxlMemory))
    // Expanded Remote memory region
    VirtualMemoryTable[++Index].PhysicalBase  = FixedPcdGet64 (PcdRemoteMemoryBase);
    VirtualMemoryTable[Index].VirtualBase     = FixedPcdGet64 (PcdRemoteMemoryBase);
    VirtualMemoryTable[Index].Length          = FixedPcdGet64 (PcdRemoteMemorySize);
    VirtualMemoryTable[Index].Attributes      = ARM_MEMORY_REGION_ATTRIBUTE_WRITE_BACK;
#endif

#if (FixedPcdGet32 (PcdChipCount) > 1)
  // Chip 1 DDR Block 1 - (2GB)
  VirtualMemoryTable[++Index].PhysicalBase  = SYSTEM_MEMORY_BASE_REMOTE (1),
  VirtualMemoryTable[Index].VirtualBase     = SYSTEM_MEMORY_BASE_REMOTE (1),
  VirtualMemoryTable[Index].Length          = PcdGet64 (PcdSystemMemorySize);
  VirtualMemoryTable[Index].Attributes      = ARM_MEMORY_REGION_ATTRIBUTE_WRITE_BACK;

  // Chip 1 DDR Block 2 - (6GB)
  VirtualMemoryTable[++Index].PhysicalBase  = GetRemoteDram2BaseAddr (1),
  VirtualMemoryTable[Index].VirtualBase     = GetRemoteDram2BaseAddr (1),
  VirtualMemoryTable[Index].Length          = PcdGet64 (PcdDramBlock2Size);
  VirtualMemoryTable[Index].Attributes      = ARM_MEMORY_REGION_ATTRIBUTE_WRITE_BACK;

#if (FixedPcdGet32 (PcdChipCount) > 2)
  // Chip 2 DDR Block 1 - (2GB)
  VirtualMemoryTable[++Index].PhysicalBase  = SYSTEM_MEMORY_BASE_REMOTE (2),
  VirtualMemoryTable[Index].VirtualBase     = SYSTEM_MEMORY_BASE_REMOTE (2),
  VirtualMemoryTable[Index].Length          = PcdGet64 (PcdSystemMemorySize);
  VirtualMemoryTable[Index].Attributes      = ARM_MEMORY_REGION_ATTRIBUTE_WRITE_BACK;

  // Chip 2 DDR Block 2 - (6GB)
  VirtualMemoryTable[++Index].PhysicalBase  = GetRemoteDram2BaseAddr (2),
  VirtualMemoryTable[Index].VirtualBase     = GetRemoteDram2BaseAddr (2),
  VirtualMemoryTable[Index].Length          = PcdGet64 (PcdDramBlock2Size);
  VirtualMemoryTable[Index].Attributes      = ARM_MEMORY_REGION_ATTRIBUTE_WRITE_BACK;

#if (FixedPcdGet32 (PcdChipCount) > 3)
  // Chip 3 DDR Block 1 - (2GB)
  VirtualMemoryTable[++Index].PhysicalBase  = SYSTEM_MEMORY_BASE_REMOTE (3),
  VirtualMemoryTable[Index].VirtualBase     = SYSTEM_MEMORY_BASE_REMOTE (3),
  VirtualMemoryTable[Index].Length          = PcdGet64 (PcdSystemMemorySize);
  VirtualMemoryTable[Index].Attributes      = ARM_MEMORY_REGION_ATTRIBUTE_WRITE_BACK;

  // Chip 3 DDR Block 2 - (6GB)
  VirtualMemoryTable[++Index].PhysicalBase  = GetRemoteDram2BaseAddr (3),
  VirtualMemoryTable[Index].VirtualBase     = GetRemoteDram2BaseAddr (3),
  VirtualMemoryTable[Index].Length          = PcdGet64 (PcdDramBlock2Size);
  VirtualMemoryTable[Index].Attributes      = ARM_MEMORY_REGION_ATTRIBUTE_WRITE_BACK;
#endif
#endif
#endif

  if (FeaturePcdGet (PcdPcieEnable)) {
    // PCI Configuration Space
    VirtualMemoryTable[++Index].PhysicalBase  = PcdGet64 (PcdPciExpressBaseAddress);
    VirtualMemoryTable[Index].VirtualBase     = PcdGet64 (PcdPciExpressBaseAddress);
    VirtualMemoryTable[Index].Length          = (FixedPcdGet32 (PcdPciBusMax) -
                                                 FixedPcdGet32 (PcdPciBusMin) + 1) *
                                                 SIZE_1MB;
    VirtualMemoryTable[Index].Attributes      = ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;
  }

 // MM Memory Space
  VirtualMemoryTable[++Index].PhysicalBase  = PcdGet64 (PcdMmBufferBase);
  VirtualMemoryTable[Index].VirtualBase     = PcdGet64 (PcdMmBufferBase);
  VirtualMemoryTable[Index].Length          = PcdGet64 (PcdMmBufferSize);
  VirtualMemoryTable[Index].Attributes      = ARM_MEMORY_REGION_ATTRIBUTE_UNCACHED_UNBUFFERED;

  if (FeaturePcdGet (PcdEinjSupported)) {
    // EINJ instruction address region
    VirtualMemoryTable[++Index].PhysicalBase  = PcdGet64 (PcdEinjInstBufferBase);
    VirtualMemoryTable[Index].VirtualBase     = PcdGet64 (PcdEinjInstBufferBase);
    VirtualMemoryTable[Index].Length          = PcdGet64 (PcdEinjInstBufferSize);
    VirtualMemoryTable[Index].Attributes      = ARM_MEMORY_REGION_ATTRIBUTE_UNCACHED_UNBUFFERED;
  }

  // End of Table
  VirtualMemoryTable[++Index].PhysicalBase  = 0;
  VirtualMemoryTable[Index].VirtualBase     = 0;
  VirtualMemoryTable[Index].Length          = 0;
  VirtualMemoryTable[Index].Attributes      = (ARM_MEMORY_REGION_ATTRIBUTES)0;

  ASSERT ((Index + 1) <= TotalMemoryMapDescirptors);
  *VirtualMemoryMap = VirtualMemoryTable;
}
