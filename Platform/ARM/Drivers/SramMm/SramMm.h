/** @file
  Base Element Ram Ecc error source Standalone Mm driver.

  Defines bit fields and macros specific to Base Element Ram Error Records.

  Copyright (c) 2022, ARM Limited. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef SRAM_MM_DRIVER_H_
#define SRAM_MM_DRIVER_H_

#include <Base.h>
#include <Guid/Cper.h>
#include <IndustryStandard/Acpi.h>
#include <Library/ArmLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Protocol/MmHestErrorSourceProtocol.h>

// Base Element Ram Error Record offsets.
#define ERRSTATUS 0
#define ERRCODE   8
#define ERRADDR   12

// Base Element Ram Ecc Error Status register bit fields.
#define ECC_ERR_STATUS_CE_BIT BIT0
#define ECC_ERR_STATUS_UE_BIT BIT1
#define ECC_ERR_STATUS_OF_BIT BIT2

// Base Element Ram Ecc Error Code register bit fields.
#define ECC_ERR_CODE_MASK        (BIT2|BIT1|BIT0)
#define ECC_ERR_CODE_NO_ERR      0x0
#define ECC_ERR_CODE_RD_DATA_ERR 0x1
#define ECC_ERR_CODE_WR_DATA_ERR 0x2
#define ECC_ERR_CODE_POISON_ERR  0x3

#define ECC_ERR_CODE_MBIT_TYPE_MASK  (BIT4|BIT3)
#define ECC_ERR_CODE_MBIT_TYPE_SHIFT 3
#define ECC_ERR_CODE_MBIT_NO_ERR     0x0
#define ECC_ERR_CODE_MBIT_CE_ERR     0x1
#define ECC_ERR_CODE_MBIT_UE_ERR     0x2

//
// Offsets within Firmware Reserved Memory (Error Status Block).
// The firmware reserved memory is used by the driver to convey the error data
// to OSPM at runtime. The firmware reserved memory carries below information:
// - Read Ack Register: Holds the physical address to the block of memory that
//                      contains Read Ack Data.
// - Error Status Register: Holds the physical address to the block of memory
//                          that contains Error Status Data (Cper).
// - Error Status Data: Actual error information buffer or Cper.
// - Read Ack Data.
//
#define ReadAckRegisterOffset     0
#define ErrorStatusRegisterOffset 8
#define ErrorStatusDataOffset     16
#define ReadAckDataOffset         (ErrorStatusDataOffset + \
                                   sizeof (EFI_PLATFORM_MEMORY_ERROR_DATA))

/**
  Allow reporting of supported Base element ram error sources.

  Installs the HEST Error Source Descriptor protocol handler that publishes the
  supported Base element ram error sources as error source descriptor.

  @param[in] MmSystemTable  Pointer to System table.

  @retval EFI_SUCCESS            Protocol installation successful.
  @retval EFI_INVALID_PARAMETER  Invalid system table parameter.
  @retval Other                  Other error while installing the protocol.
**/
EFI_STATUS
SramInstallErrorSourceDescProtocol (
  IN EFI_MM_SYSTEM_TABLE *MmSystemTable
  );

#endif //SRAM_MM_DRIVER_H_
