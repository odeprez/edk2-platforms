/** @file
  Cpu Error Source descriptor information.

  Implements the Hest Error Source Descriptor protocol. Creates and publishes
  error source descriptors of type GHESv2 for supported error sources.

  Copyright (c) 2021 - 2022, ARM Limited. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Specification Reference:
    - ACPI Reference Specification 6.4, Table 18.13 GHESv2 Structure.
**/

#include <Library/AcpiLib.h>
#include <CpuMm.h>

/**
  HEST error source descriptor protocol implementation for Cpu MM driver.

  At boot returns the error source descriptor information for all supported
  Cpu error sources. As defined by the HEST Error Source Descriptor
  protocol interface this handler returns error source count and length when
  it is called with Buffer parameter set to NULL.

  @param[in]  This                Pointer for this protocol.
  @param[out] Buffer              Hest error source descriptor Information
                                  buffer.
  @param[out] ErrorSourcesLength  Total length of Error Source Descriptors.
  @param[out] ErrorSourceCount    Total number of supported error sources.

  @retval EFI_SUCCESS            Buffer has valid Error Source descriptor
                                 information.
  @retval EFI_BUFFER_TOO_SMALL   Buffer is NULL.
  @retval EFI_INVALID_PARAMETER  Buffer is NULL.
**/
STATIC
EFI_STATUS
EFIAPI
CpuErrorSourceDescInfoGet (
  IN  EDKII_MM_HEST_ERROR_SOURCE_DESC_PROTOCOL *This,
  OUT VOID                                     **Buffer,
  OUT UINTN                                    *ErrorSourcesLength,
  OUT UINTN                                    *ErrorSourcesCount
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
    FixedPcdGet64 (PcdCpuErrorSourceCount) *
    sizeof (EFI_ACPI_6_4_GENERIC_HARDWARE_ERROR_SOURCE_VERSION_2_STRUCTURE);
  *ErrorSourcesCount = FixedPcdGet32 (PcdCpuErrorSourceCount);

  //
  // If 'Buffer' is NULL, return. The current invocation of interface is to
  // determine the total size of all the error source descriptor.
  // Caller will allocate adequate size buffer using the length param and call
  // again.
  //
  if (Buffer == NULL) {
    return EFI_BUFFER_TOO_SMALL;
  }

  // Initialize firmware reserved memory(Cper) section for Cpu.
  SetMem (
    (VOID *)FixedPcdGet64 (PcdCpuErrorDataBase),
    FixedPcdGet64 (PcdCpuErrorDataSize),
    0
    );

  //
  // Locate Error Status Register memory space within the firmware reserved
  // memory and initialize it with physical address of Cper.
  //
  // Determine and initialize the address range of firmware reserved memory
  // (Error Status Block) for given Cpu error. This memory is used to hold
  // the runtime error information for Cpu error. This firmware
  // reserved memory contains information about:
  // - Read Ack Register: Holds the physical address to block of memory that
  //                      contains Read Ack Data.
  // - Error Status Register: Holds the physical address of block of memory
  //                          that contains Cper.
  // - Error Status Data : Cper.
  // - Read Ack Data.
  //
  ErrorStatusBlock = FixedPcdGet64 (PcdCpuErrorDataBase);
  ErrorStatusRegister = (UINTN *)(ErrorStatusBlock + ErrorStatusRegisterOffset);
  *ErrorStatusRegister = (ErrorStatusBlock + ErrorStatusDataOffset);

  // Buffer to be updated with error source descriptor(s) information.
  ErrorDescriptor =
    (EFI_ACPI_6_4_GENERIC_HARDWARE_ERROR_SOURCE_VERSION_2_STRUCTURE *)*Buffer;

  // Populate boot time Cpu Error source descriptor information.
  ErrorDescriptor->Type = EFI_ACPI_6_4_GENERIC_HARDWARE_ERROR_VERSION_2;
  ErrorDescriptor->SourceId = FixedPcdGet16 (PcdCpuErrorSourceId);
  ErrorDescriptor->RelatedSourceId = 0xFFFF;
  ErrorDescriptor->Flags = 0;
  ErrorDescriptor->Enabled = 1;
  ErrorDescriptor->NumberOfRecordsToPreAllocate = 1;
  ErrorDescriptor->MaxSectionsPerRecord = 1;
  ErrorDescriptor->MaxRawDataLength = CPU_SECTION_DATA_SIZE (NON_SECURE);
  // Initialize Error Status Register address with physical address of Cper.
  ErrorDescriptor->ErrorStatusAddress = (EFI_ACPI_6_4_GENERIC_ADDRESS_STRUCTURE)
    ARM_GAS64 (ErrorStatusBlock + ErrorStatusRegisterOffset);
  ErrorDescriptor->NotificationStructure =
    (EFI_ACPI_6_4_HARDWARE_ERROR_NOTIFICATION_STRUCTURE)
    EFI_ACPI_6_4_HARDWARE_ERROR_NOTIFICATION_STRUCTURE_INIT (
      EFI_ACPI_6_4_HARDWARE_ERROR_NOTIFICATION_SOFTWARE_DELEGATED_EXCEPTION,
      0,
      FixedPcdGet32 (PcdCpuErrorSdeiEventBase)
      );
  ErrorDescriptor->ErrorStatusBlockLength =
    sizeof (EFI_ACPI_6_4_GENERIC_ERROR_STATUS_STRUCTURE) +
    sizeof (EFI_ACPI_6_4_GENERIC_ERROR_DATA_ENTRY_STRUCTURE) +
    CPU_SECTION_DATA_SIZE (NON_SECURE);
  // Initialize Read Ack Register with physical address of Acknowledge
  // buffer.
  ErrorDescriptor->ReadAckRegister = (EFI_ACPI_6_4_GENERIC_ADDRESS_STRUCTURE)
    ARM_GAS64 (ErrorStatusBlock);
  ErrorDescriptor->ReadAckPreserve = 0;
  ErrorDescriptor->ReadAckWrite = 0;

  return EFI_SUCCESS;
}

//
// Cpu EDKII_MM_HEST_ERROR_SOURCE_DESC_PROTOCOL protocol instance.
//
STATIC EDKII_MM_HEST_ERROR_SOURCE_DESC_PROTOCOL mCpuErrorSourceDesc = {
  CpuErrorSourceDescInfoGet
};

/**
  Allow reporting of supported Cpu error sources.

  Installs the HEST Error Source Descriptor protocol handler that publishes the
  supported Cpu error sources as error source descriptor.

  @param[in] MmSystemTable  Pointer to System table.

  @retval EFI_SUCCESS            Protocol installation successful.
  @retval EFI_INVALID_PARAMETER  Invalid system table parameter.
**/
EFI_STATUS
CpuInstallErrorSourceDescProtocol (
  IN EFI_MM_SYSTEM_TABLE *MmSystemTable
  )
{
  EFI_HANDLE CpuHandle = NULL;
  EFI_STATUS Status;

  // Check if the MmSystemTable is initialized.
  if (MmSystemTable == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  // Install Hest error source descriptor protocol for Cpu.
  Status = MmSystemTable->MmInstallProtocolInterface (
                            &CpuHandle,
                            &gMmHestErrorSourceDescProtocolGuid,
                            EFI_NATIVE_INTERFACE,
                            &mCpuErrorSourceDesc
                            );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed installing Hest error source protocol, status: %r\n",
      __FUNCTION__,
      Status
      ));
  }

  return Status;
}
