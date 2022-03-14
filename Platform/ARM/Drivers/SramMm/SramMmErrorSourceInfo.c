/** @file
  Base Element Ram Error Source descriptor information.

  Implements the Hest Error Source Descriptor protocol. Creates and publishes
  error source descriptors of type GHESv2 for supported error sources.

  Copyright (c) 2022, ARM Limited. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Specification Reference:
    - ACPI Reference Specification 6.4, Table 18.13 GHESv2 Structure.
**/

#include <Library/AcpiLib.h>
#include <SramMm.h>

/**
  HEST error source descriptor protocol implementation for Base element Ram MM
  driver.

  At boot returns the error source descriptor information for all supported
  base element ram error sources. As defined by the HEST Error Source
  Descriptor protocol interface this handler returns error source count and
  length when it is called with Buffer parameter set to NULL.

  @param[in]     This                Pointer to this protocol.
  @param[out]    Buffer              HEST error source descriptor Information
                                     buffer.
  @param[in,out] ErrorSourcesLength  Total length of Error Source Descriptors.
  @param[in,out] ErrorSourceCount    Total number of supported error sources.

  @retval EFI_SUCCESS            Buffer has valid Error Source Descriptor
                                 information.
  @retval EFI_BUFFER_TOO_SMALL   Buffer is NULL.
  @retval EFI_INVALID_PARAMETER  ErrorSourcesLength or ErrorSourcesCount is
                                 NULL.
**/
STATIC
EFI_STATUS
EFIAPI
SramErrorSourceDescInfoGet (
  IN     EDKII_MM_HEST_ERROR_SOURCE_DESC_PROTOCOL *This,
  OUT    VOID                                     **Buffer,
  IN OUT UINTN                                    *ErrorSourcesLength,
  IN OUT UINTN                                    *ErrorSourcesCount
  )
{
  EFI_ACPI_6_4_GENERIC_HARDWARE_ERROR_SOURCE_VERSION_2_STRUCTURE *ErrorDescriptor;
  UINTN    ErrorStatusBlock;
  UINTN    *ErrorStatusRegister;

  // Check ErrorSourcesLength and ErrorSourcesCount params are valid.
  if (ErrorSourcesLength == NULL ||
      ErrorSourcesCount == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Update the error source length and error source count parameters. These
  // parameters represent total count of error sources that this driver
  // publishes and their total length.
  //
  *ErrorSourcesLength =
    FixedPcdGet64 (PcdSramErrorSourceCount) *
    sizeof (EFI_ACPI_6_4_GENERIC_HARDWARE_ERROR_SOURCE_VERSION_2_STRUCTURE);
  *ErrorSourcesCount = FixedPcdGet64 (PcdSramErrorSourceCount);

  //
  // If 'Buffer' is NULL, return. The current invocation of interface is to
  // determine the total size of all the error source descriptor.
  // Caller will allocate adequate size buffer using the length param and call
  // again.
  //
  if (Buffer == NULL) {
    return EFI_BUFFER_TOO_SMALL;
  }

  // Initialize firmware reserved memory(Cper) section for base element ram.
  SetMem (
    (VOID *)FixedPcdGet64 (PcdSramErrorDataBase),
    FixedPcdGet64 (PcdSramErrorDataSize),
    0
    );

  //
  // Locate Error Status Register memory space within the firmware reserved
  // memory and initialize it with physical address of Cper.
  //
  // Determine and initialize the address range of firmware reserved memory
  // (Error Status Block) for given Ram error source. This memory is used to
  // hold the runtime error information for Ram error. This firmware
  // reserved memory holds information about:
  // - Read Ack Register: Holds the physical address to block of memory that
  //                      contains Read Ack Data.
  // - Error Status Register: Holds the physical address of block of memory
  //                          that contains Cper.
  // - Error Status Data : Cper.
  // - Read Ack Data.
  //
  ErrorStatusBlock = FixedPcdGet64 (PcdSramErrorDataBase);
  ErrorStatusRegister = (UINTN *)(ErrorStatusBlock + ErrorStatusRegisterOffset);
  *ErrorStatusRegister = (ErrorStatusBlock + ErrorStatusDataOffset);


  // Buffer to be updated with error source descriptor(s) information.
  ErrorDescriptor =
    (EFI_ACPI_6_4_GENERIC_HARDWARE_ERROR_SOURCE_VERSION_2_STRUCTURE *)*Buffer;

  // Populate boot time Base element ram error source descriptor information.
  ErrorDescriptor->Type = EFI_ACPI_6_4_GENERIC_HARDWARE_ERROR_VERSION_2;
  ErrorDescriptor->SourceId = FixedPcdGet16 (PcdSramErrorSourceId);
  ErrorDescriptor->RelatedSourceId = 0xFFFF;
  ErrorDescriptor->Flags = 0;
  ErrorDescriptor->Enabled = 1;
  ErrorDescriptor->NumberOfRecordsToPreAllocate = 1;
  ErrorDescriptor->MaxSectionsPerRecord = 1;
  ErrorDescriptor->MaxRawDataLength = sizeof (EFI_PLATFORM_MEMORY_ERROR_DATA);
  // Initialize Error Status Register address with physical address of Cper.
  ErrorDescriptor->ErrorStatusAddress = (EFI_ACPI_6_4_GENERIC_ADDRESS_STRUCTURE)
    ARM_GAS64 (ErrorStatusBlock + ErrorStatusRegisterOffset);
  ErrorDescriptor->NotificationStructure =
   (EFI_ACPI_6_4_HARDWARE_ERROR_NOTIFICATION_STRUCTURE)
    EFI_ACPI_6_4_HARDWARE_ERROR_NOTIFICATION_STRUCTURE_INIT (
      EFI_ACPI_6_4_HARDWARE_ERROR_NOTIFICATION_SOFTWARE_DELEGATED_EXCEPTION,
      0,
      FixedPcdGet32 (PcdSramErrorSdeiEventBase)
      );
  ErrorDescriptor->ErrorStatusBlockLength =
    (sizeof (EFI_ACPI_6_4_GENERIC_ERROR_STATUS_STRUCTURE) +
     sizeof (EFI_ACPI_6_4_GENERIC_ERROR_DATA_ENTRY_STRUCTURE) +
     sizeof (EFI_PLATFORM_MEMORY_ERROR_DATA));
  // Initialize Read Ack Register with physical address of Acknowledge
  // buffer.
  ErrorDescriptor->ReadAckRegister = (EFI_ACPI_6_4_GENERIC_ADDRESS_STRUCTURE)
    ARM_GAS64 (ErrorStatusBlock);
  ErrorDescriptor->ReadAckPreserve = 0;
  ErrorDescriptor->ReadAckWrite = 0;

  return EFI_SUCCESS;
}

//
// Base element ram EDKII_MM_HEST_ERROR_SOURCE_DESC_PROTOCOL protocol instance.
//
STATIC EDKII_MM_HEST_ERROR_SOURCE_DESC_PROTOCOL mSramErrorSourceDesc = {
  SramErrorSourceDescInfoGet
};

/**
  Allow reporting of supported Base element ram error sources.

  Installs the Hest Error Source Descriptor protocol handler that publishes the
  supported Base element ram error sources as error source descriptor.

  @param[in] MmSystemTable  Pointer to System table.

  @retval EFI_SUCCESS            Protocol installation successful.
  @retval EFI_INVALID_PARAMETER  Invalid system table parameter.
  @retval Other                  Other error while installing the protocol.
**/
EFI_STATUS
SramInstallErrorSourceDescProtocol (
  IN EFI_MM_SYSTEM_TABLE *MmSystemTable
  )
{
  EFI_HANDLE SramHandle = NULL;
  EFI_STATUS Status;

  // Check if the SystemTable is initialized.
  if (MmSystemTable == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  // Install HEST error source descriptor protocol for Base element ram.
  Status = MmSystemTable->MmInstallProtocolInterface (
                            &SramHandle,
                            &gMmHestErrorSourceDescProtocolGuid,
                            EFI_NATIVE_INTERFACE,
                            &mSramErrorSourceDesc
                            );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed installing HEST error source protocol, status: %r\n",
      __FUNCTION__,
      Status
      ));
  }

  return Status;
}
