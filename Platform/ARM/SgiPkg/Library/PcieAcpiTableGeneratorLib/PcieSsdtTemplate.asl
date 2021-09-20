/** @file
*  Secondary System Description Table (SSDT)
*
*  Copyright (c) 2021, ARM Limited. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include "SgiAcpiHeader.h"

DefinitionBlock("SsdtPci.aml", "SSDT", 2, "ARMLTD", "ARMSGI", EFI_ACPI_ARM_OEM_REVISION) {
  Scope (_SB) {
    Device (PCI0) {
      Name (_HID, EISAID("PNP0A08"))  /* PCI Express Root Bridge */
      Name (_CID, EISAID("PNP0A03"))  /* Compatible PCI Root Bridge */
      Name (_SEG, 0)                  /* PCI Segment Group Number */
      Name (_BBN, 0)                  /* PCI Base Bus Number */
      Name (_ADR, Zero)
      Name (_UID, 0)                  /* Unique ID */
      Name (_CCA, 1)                  /* Cache Coherency Attribute */

      Name (_CRS, ResourceTemplate () {
        WordBusNumber (
          ResourceProducer,
          MinFixed,
          MaxFixed,
          PosDecode,
          0,
          0,
          0xF,
          0,
          0x10
        )

        QWordMemory (
          ResourceProducer,
          PosDecode,
          MinFixed,
          MaxFixed,
          Cacheable,
          ReadWrite,
          0x00000000,
          0x60000000,
          0x6FFFFFFF,
          0x00000000,
          0x10000000
        )

        QWordMemory (
          ResourceProducer,
          PosDecode,
          MinFixed,
          MaxFixed,
          Cacheable,
          ReadWrite,
          0x00000000,
          0x4000000000,
          0x5FFFFFFFFF,
          0x00000000,
          0x2000000000
        )

        DWordIo (
          ResourceProducer,
          MinFixed,
          MaxFixed,
          PosDecode,
          EntireRange,
          0x00000000,
          0x00000000,
          0x007FFFFF,
          0x77800000,
          0x00800000,
          ,
          ,
          ,
          TypeTranslation
        )
      })

      Device (RES0)
      {
        Name (_HID, "PNP0C02" /* PNP Motherboard Resources */)
        Name (_CRS, ResourceTemplate ()
        {
           QWordMemory (
             ResourceProducer,
             PosDecode,
             MinFixed,
             MaxFixed,
             NonCacheable,
             ReadWrite,
             0x0000000000000000,
             0x1010000000,
             0x1017FFFFFF,
             0x0000000000000000,
             0x8000000,
             ,
             ,
             ,
             AddressRangeMemory,
             TypeStatic)
        })
      }
    }
  }
}
