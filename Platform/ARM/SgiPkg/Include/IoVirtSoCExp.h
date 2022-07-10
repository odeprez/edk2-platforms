/** @file
*
*  Copyright (c) 2023, Arm Limited. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include "SgiPlatform.h"

#define IO_VIRT_BLK_BASE     FixedPcdGet64 (PcdIoVirtSocExpBlk0Base)
#define DEV_OFFSET           0x10000000
#define RESOURCE_SIZE        0x10000

/** Macros to calculate base addresses of UART and DMA devices within IO
    virtualization SoC expansion block address space.

  @param [in] n         Index of UART or DMA device within SoC expansion block.
                        Should be either 0 or 1.

  The base address offsets of UART and DMA devices within a SoC expansion block
  are shown below. The UARTs are at offset (2 * index * offset), while the DMAs
  are at offsets ((2 * index + 1) * offset).
  +----------------------------------------------+
  | Port # |  Peripheral   | Base address offset |
  |--------|---------------|---------------------|
  |  x4_0  | PL011_UART0   |     0x0000_0000     |
  |--------|---------------|---------------------|
  |  x4_1  | PL011_DMA0_NS |     0x1000_0000     |
  |--------|---------------|---------------------|
  |   x8   | PL011_UART1   |     0x2000_0000     |
  |--------|---------------|---------------------|
  |   x16  | PL011_DMA1_NS |     0x3000_0000     |
  +----------------------------------------------+
**/
#define UART_START(n)        IO_VIRT_BLK_BASE + (2 * n * DEV_OFFSET)
#define DMA_START(n)         IO_VIRT_BLK_BASE + (((2 * n) + 1) * DEV_OFFSET)

// Interrupt numbers of PL330 DMA-0 and DMA-1 devices in the SoC expansion
// connected to the IO Virtualization block. Each DMA PL330 controller uses
// eight data channel interrupts and one instruction channel interrupt to
// notify aborts.
#define RD_IOVIRT_SOC_EXP_DMA0_INTERRUPTS_INIT                                 \
  Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive) {                 \
    493, 494, 495, 496, 497, 498, 499, 500, 501                                \
  }
#define RD_IOVIRT_SOC_EXP_DMA1_INTERRUPTS_INIT                                 \
  Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive) {                 \
    503, 504, 505, 506, 507, 508, 509, 510, 511                                \
  }

#define RD_IOVIRT_SOC_EXP_DMA2_INTERRUPTS_INIT                                 \
  Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive) {                 \
    973, 974, 975, 976, 977, 978, 979, 980, 981                                \
  }

#define RD_IOVIRT_SOC_EXP_DMA3_INTERRUPTS_INIT                                 \
  Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive) {                 \
    983, 984, 985, 986, 987, 988, 989, 990, 991                                \
  }

#define RD_IOVIRT_SOC_EXP_DMA4_INTERRUPTS_INIT                                 \
  Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive) {                 \
    4557, 4558, 4559, 4560, 4561, 4562, 4563, 4564, 4565                       \
  }

#define RD_IOVIRT_SOC_EXP_DMA5_INTERRUPTS_INIT                                 \
  Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive) {                 \
    4567, 4568, 4569, 4570, 4571, 4572, 4573, 4574, 4575                       \
  }

#define RD_IOVIRT_SOC_EXP_DMA6_INTERRUPTS_INIT                                 \
  Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive) {                 \
    5037, 5038, 5039, 5040, 5041, 5042, 5043, 5044, 5045                       \
  }

#define RD_IOVIRT_SOC_EXP_DMA7_INTERRUPTS_INIT                                 \
  Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive) {                 \
    5047, 5048, 5049, 5050, 5051, 5052, 5053, 5054, 5055                       \
  }

/** Macro for PL011 UART controller node instantiation in SSDT table.

  See section 5.2.11.2 of ACPI specification v6.4 for the definition of SSDT
  table.

  @param [in] ComIdx          Index of Com device to be initializaed;
                              to be passed as 2-digit index, such as 01 to
                              support multichip platforms as well.
  @param [in] ChipIdx         Index of chip to which this DMA device belongs
  @param [in] StartOff        Starting offset of this device within IO
                              virtualization block memory map
  @param [in] IrqNum          Interrupt ID used for the device
**/
#define RD_IOVIRT_SOC_EXP_COM_INIT(ComIdx, ChipIdx, StartOff, IrqNum)          \
  Device (COM ##ComIdx) {                                                      \
    Name (_HID, "ARMH0011")                                                    \
    Name (_UID, ComIdx)                                                        \
    Name (_STA, 0xF)                                                           \
                                                                               \
    Method (_CRS, 0, Serialized) {                                             \
      Name (RBUF, ResourceTemplate () {                                        \
        QWordMemory (                                                          \
          ResourceProducer,                                                    \
          PosDecode,                                                           \
          MinFixed,                                                            \
          MaxFixed,                                                            \
          NonCacheable,                                                        \
          ReadWrite,                                                           \
          0x0,                                                                 \
          0,                                                                   \
          1,                                                                   \
          0x0,                                                                 \
          2,                                                                   \
          ,                                                                    \
          ,                                                                    \
          MMI1,                                                                \
          AddressRangeMemory,                                                  \
          TypeStatic                                                           \
        )                                                                      \
                                                                               \
        Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive) {           \
          IrqNum                                                               \
        }                                                                      \
      }) /* end Name(RBUF) */                                                  \
      /* Work around ASL's inability to add in a resource definition */        \
      CreateQwordField (RBUF, MMI1._MIN, MIN1)                                 \
      CreateQwordField (RBUF, MMI1._MAX, MAX1)                                 \
      CreateQwordField (RBUF, MMI1._LEN, LEN1)                                 \
      Add (SGI_REMOTE_CHIP_MEM_OFFSET(ChipIdx), StartOff, MIN1)                \
      Add (MIN1, RESOURCE_SIZE - 1, MAX1)                                      \
      Add (RESOURCE_SIZE, 0, LEN1)                                             \
                                                                               \
      Return (RBUF)                                                            \
    } /* end Method(_CRS) */                                                   \
  }

/** Macro for PL330 DMA controller node instantiation in SSDT table.

  See section 5.2.11.2 of ACPI specification v6.4 for the definition of SSDT
  table.

  @param [in] DmaIdx          Index of DMA device to be initializaed
  @param [in] ChipIdx         Index of chip to which this DMA device belongs
  @param [in] StartOff        Starting offset of this device within IO
                              virtualization block memory map
**/
#define RD_IOVIRT_SOC_EXP_DMA_INIT(DmaIdx, ChipIdx, StartOff)                  \
  Device (\_SB.DMA ##DmaIdx) {                                                 \
    Name (_HID, "ARMH0330")                                                    \
    Name (_UID, DmaIdx)                                                        \
    Name (_CCA, 1)                                                             \
    Name (_STA, 0xF)                                                           \
                                                                               \
    Method (_CRS, 0, Serialized) {                                             \
      Name (RBUF, ResourceTemplate () {                                        \
        QWordMemory (                                                          \
          ResourceProducer,                                                    \
          PosDecode,                                                           \
          MinFixed,                                                            \
          MaxFixed,                                                            \
          NonCacheable,                                                        \
          ReadWrite,                                                           \
          0x0,                                                                 \
          0,                                                                   \
          1,                                                                   \
          0x0,                                                                 \
          2,                                                                   \
          ,                                                                    \
          ,                                                                    \
          MMI2,                                                                \
          AddressRangeMemory,                                                  \
          TypeStatic                                                           \
        )                                                                      \
                                                                               \
        RD_IOVIRT_SOC_EXP_DMA ##DmaIdx## _INTERRUPTS_INIT                      \
      }) /* end Name(RBUF) */                                                  \
      /* Work around ASL's inability to add in a resource definition */        \
      CreateQwordField (RBUF, MMI2._MIN, MIN2)                                 \
      CreateQwordField (RBUF, MMI2._MAX, MAX2)                                 \
      CreateQwordField (RBUF, MMI2._LEN, LEN2)                                 \
      Add (SGI_REMOTE_CHIP_MEM_OFFSET(ChipIdx), StartOff, MIN2)                \
      Add (MIN2, RESOURCE_SIZE - 1, MAX2)                                      \
      Add (RESOURCE_SIZE, 0, LEN2)                                             \
                                                                               \
      Return (RBUF)                                                            \
    } /* end Method(_CRS) */                                                   \
  }

// x16/x8/x4_1/x4_0 ports of the IO virtualization block to which the PCIe
// root bus or the SoC expansion block is connected.
typedef enum {
  PCIex4_0,
  PCIex4_1,
  PCIex8,
  PCIex16,
} ARM_RD_PCIE_PORT_ID;

// x16/x8/x4_1/x4_0 ports of the IO virtualization block have a base
// DeviceID that is added to the StreamID of the devices connected
// to ports to create the IDs sent to the SMMUv3 and ITS.
//
// Stream ID coming at SMMUv3 is calculated as below:
//    Stream ID = DMA base Stream/deviceID +
//                DMA Channel Index + deviceID for port
// On each IO-virtualization SoC expansion block the base DeviceID
// for DMA is 0.
#define DEV_ID_BASE(Port)                                                      \
    FixedPcdGet32 (PcdIoVirtBlkPortPciex40DevIdBase) +                         \
    (FixedPcdGet32 (PcdIoVirtBlkPortDevIdOffset) * Port)

// StreamID base for PL330 DMA0 controller
// IO virtualization SoC expansion block has DMA-0 and DMA-1 connected
// to PCIex4_1 and PCIex16 ports respectively.
#define DMA_STREAM_ID_BASE(DmaIdx)                                             \
    ( DmaIdx == 0 ? DEV_ID_BASE(PCIex4_1) :                                    \
      DmaIdx == 1 ? DEV_ID_BASE(PCIex16) :                                     \
      0 )

/** Helper macro for ID mapping table initialization of DMA Named Component
    IORT node.

    See Table 4 of Arm IORT specification, version E.b.

    Output StreamID for a channel can be calculated as -
    ((IDBase for x16/x8/x4_1/x4_0) + BaseSID of DMA controller) + Channel Idx).

    @param [in] DmaIdx            Index of DMA pl330 controller connected to
                                  a IO virtualization SoC expansion block.
    @param [in] ChStreamIdx       Channel index within one DMA controller -
                                  0 to 8 that includes 8 data channels, and
                                  one instruction channel.
**/
#define DMA_NC_ID_TABLE_INIT(DmaIdx, ChStreamIdx)                              \
  {                                                                            \
    ChStreamIdx,                                       /* InputBase */         \
    0,                                                 /* NumIds */            \
    (DMA_STREAM_ID_BASE(DmaIdx % 2) + ChStreamIdx),    /* OutputBase */        \
    0x0,                                               /* OutputReference */   \
    EFI_ACPI_IORT_ID_MAPPING_FLAGS_SINGLE,             /* Flags */             \
  }

/** Helper macro for DMA Named Component node initialization for Arm Iort
    table.

    See Table 13 of Arm IORT specification, version E.b.

    @param [in] DmaIdx            Index of DMA pl330 controller connected to
                                  a IO virtualization SoC expansion block.
**/
#define ARM_RD_ACPI_IO_VIRT_BLK_DMA_NC_INIT(DmaIdx)                            \
  /* RD_IOVIRT_SOC_EXP_IORT_DMA_NC_NODE */                                     \
  {                                                                            \
    /* EFI_ACPI_6_0_IO_REMAPPING_NAMED_COMP_NODE */                            \
    {                                                                          \
      {                                                                        \
        EFI_ACPI_IORT_TYPE_NAMED_COMP,                       /* Type */        \
        sizeof (RD_IOVIRT_SOC_EXP_IORT_DMA_NC_NODE),         /* Length */      \
        4,                                                   /* Revision */    \
        0,                                                   /* Identifier */  \
        9,                                          /* NumIdMappings */        \
        OFFSET_OF (RD_IOVIRT_SOC_EXP_IORT_DMA_NC_NODE, DmaIdMap)               \
                                                    /* IdReference */          \
      },                                                                       \
      0x0,                                          /* Flags */                \
      0x0,                                          /* CacheCoherent */        \
      0x0,                                          /* AllocationHints */      \
      0x0,                                          /* Reserved */             \
      0x0,                                          /* MemoryAccessFlags */    \
      0x30,                                         /* AddressSizeLimit */     \
    },                                                                         \
    {                                                                          \
      /* Object RefName */                                                     \
    },                                                                         \
    /* ID mapping table */                                                     \
    {                                                                          \
      DMA_NC_ID_TABLE_INIT(DmaIdx, 0),              /* Data Channel - 0 */     \
      DMA_NC_ID_TABLE_INIT(DmaIdx, 1),              /* Data Channel - 1 */     \
      DMA_NC_ID_TABLE_INIT(DmaIdx, 2),              /* Data Channel - 2 */     \
      DMA_NC_ID_TABLE_INIT(DmaIdx, 3),              /* Data Channel - 3 */     \
      DMA_NC_ID_TABLE_INIT(DmaIdx, 4),              /* Data Channel - 4 */     \
      DMA_NC_ID_TABLE_INIT(DmaIdx, 5),              /* Data Channel - 5 */     \
      DMA_NC_ID_TABLE_INIT(DmaIdx, 6),              /* Data Channel - 6 */     \
      DMA_NC_ID_TABLE_INIT(DmaIdx, 7),              /* Data Channel - 7 */     \
      DMA_NC_ID_TABLE_INIT(DmaIdx, 8),              /* Instruction Channel */  \
    },                                                                         \
  }
