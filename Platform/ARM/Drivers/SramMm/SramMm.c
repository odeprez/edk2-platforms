/** @file
  Base Element Ram error handling (Standalone MM) driver.

  Supports 1-bit CE error handling for base element ram. On error event
  publishes the CPER error record of Memory Error.

  Copyright (c) 2022, ARM Limited. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Specification Reference
    - UEFI Reference Specification 2.9, Section N.2.5 Memory Error Section
**/

#include <SramMm.h>

/**
  Helper function to handle the Base Element Ram errors.

  Reads the Base element ram error record registers. Creates a CPER error
  record of type 'Memory Error' and populates it with information collected
  from Base element ram error record registers.

  @param[in] SramErr  Sram error info data structure.
**/
STATIC
VOID
SramErrorHandler (
  IN SRAM_ERR_INFO  *SramErr
  )
{
  EFI_ACPI_6_4_GENERIC_ERROR_DATA_ENTRY_STRUCTURE *ErrBlockSectionDesc;
  EFI_ACPI_6_4_GENERIC_ERROR_STATUS_STRUCTURE     *ErrBlockStatusHeaderData;
  EFI_PLATFORM_MEMORY_ERROR_DATA                  MemorySectionInfo;
  EFI_GUID                                        SectionType;
  VOID                                            *ErrBlockSectionData;
  UINTN                                           *ErrStatusBlock;
  UINT32                                          ErrStatus;
  UINT32                                          ErrAddr;
  BOOLEAN                                         CorrectedError;
  BOOLEAN                                         UncorrectableError;

  // Read Base element ram error record registers.
  ErrStatus = SramErr->ErrStatus;
  ErrAddr = SramErr->ErrAddr;

  DEBUG ((DEBUG_INFO,"ErrStatus = 0x%x 0x%lx 0x%lx\n", ErrStatus));
  DEBUG ((DEBUG_INFO,"ErrAddr = 0x%x\n", ErrAddr));

  // Read the error type and count information.
  CorrectedError = FALSE;
  UncorrectableError = FALSE;
  if ((ErrStatus & FixedPcdGet32 (PcdSramErrorErrStatusCorrectedError)) != 0) {
    CorrectedError = TRUE;
  } else if ((ErrStatus &
		FixedPcdGet32 (PcdSramErrorErrStatusUncorrectedError)) != 0) {
    UncorrectableError = TRUE;
  }

  // Read the faulting Memory Error Address Information.
  ZeroMem (&MemorySectionInfo, sizeof (MemorySectionInfo));
  MemorySectionInfo.ValidFields |=
      EFI_PLATFORM_MEMORY_PHY_ADDRESS_MASK_VALID |
      EFI_PLATFORM_MEMORY_PHY_ADDRESS_VALID;
    MemorySectionInfo.PhysicalAddressMask = 0xFFFFFFFFFFFF;
    MemorySectionInfo.PhysicalAddress = ErrAddr;

  // Locate Error Status Data memory space within the firmware reserved memory
  // and populate Cper record for Memory error.
  ErrStatusBlock =
    (UINTN *)(FixedPcdGet64 (PcdSramErrorDataBase) + ErrorStatusDataOffset);

  //
  // Locate Block Status Header base address and populate it with Error Status
  // Block Header information.
  //
  ErrBlockStatusHeaderData = (EFI_ACPI_6_4_GENERIC_ERROR_STATUS_STRUCTURE *)
                             ErrStatusBlock;
  ErrBlockStatusHeaderData->BlockStatus.UncorrectableErrorValid =
    (UncorrectableError ? 1 : 0);
  ErrBlockStatusHeaderData->BlockStatus.CorrectableErrorValid =
    (CorrectedError ? 1 : 0);
  ErrBlockStatusHeaderData->BlockStatus.MultipleUncorrectableErrors = 0x0;
  ErrBlockStatusHeaderData->BlockStatus.MultipleCorrectableErrors = 0x0;
  ErrBlockStatusHeaderData->BlockStatus.ErrorDataEntryCount = 0x1;
  ErrBlockStatusHeaderData->RawDataOffset =
    (sizeof (EFI_ACPI_6_4_GENERIC_ERROR_STATUS_STRUCTURE) +
     sizeof (EFI_ACPI_6_4_GENERIC_ERROR_DATA_ENTRY_STRUCTURE));
  ErrBlockStatusHeaderData->RawDataLength = 0;
  ErrBlockStatusHeaderData->DataLength =
    (sizeof (EFI_ACPI_6_4_GENERIC_ERROR_DATA_ENTRY_STRUCTURE) +
     sizeof (EFI_PLATFORM_MEMORY_ERROR_DATA));
  ErrBlockStatusHeaderData->ErrorSeverity =
    (CorrectedError ?
     EFI_ACPI_6_4_ERROR_SEVERITY_CORRECTED :
     EFI_ACPI_6_4_ERROR_SEVERITY_FATAL);

   //
  // Locate Section Descriptor base address and populate Error Status Section
  // Descriptor data.
  //
  ErrBlockSectionDesc = (EFI_ACPI_6_4_GENERIC_ERROR_DATA_ENTRY_STRUCTURE *)
                        (ErrBlockStatusHeaderData + 1);
  ErrBlockSectionDesc->ErrorSeverity =
    (CorrectedError ?
     EFI_ACPI_6_4_ERROR_SEVERITY_CORRECTED :
     EFI_ACPI_6_4_ERROR_SEVERITY_FATAL);
  ErrBlockSectionDesc->Revision =
    EFI_ACPI_6_4_GENERIC_ERROR_DATA_ENTRY_REVISION;
  ErrBlockSectionDesc->ValidationBits = 0;
  ErrBlockSectionDesc->Flags = 0;
  ErrBlockSectionDesc->ErrorDataLength =
    sizeof (EFI_PLATFORM_MEMORY_ERROR_DATA);
  SectionType = (EFI_GUID)EFI_ERROR_SECTION_PLATFORM_MEMORY_GUID;
  CopyGuid ((EFI_GUID *)ErrBlockSectionDesc->SectionType, &SectionType);

  // Locate Section base address and populate Memory Error Section (Cper) data.
  ErrBlockSectionData = (VOID *)(ErrBlockSectionDesc + 1);
  CopyMem (
    ErrBlockSectionData,
    (VOID *)&MemorySectionInfo,
    sizeof (EFI_PLATFORM_MEMORY_ERROR_DATA)
    );
}

/**
  Base Element Ram Ecc Mmi event handler.

  Supports processing of errors generated by Base Element Ram Ecc.

  @param[in] DispatchHandle      The unique handle assigned to this handler by
                                 MmiHandlerRegister().
  @param[in] Context             Points to an optional handler context which
                                 was specified when the handler was registered.
  @param[in,out] SramBuffer      Buffer carrying Base Element Ram error
                                 information.
  @param[in,out] SramBufferSize  The size of the SramBuffer.

  @retval EFI_SUCCESS            SramBuffer has valid data.
  @retval EFI_INVALID_PARAMETER  SramBuffer recieved is NULL.
  @retval EFI_BAD_BUFFER_SIZE    Invalid SramBufferSize received.
  @retval Other                  For any other error.
**/
STATIC
EFI_STATUS
EFIAPI
SramErrorEventMmiHandler (
  IN     EFI_HANDLE DispatchHandle,
  IN     CONST VOID *Context,       OPTIONAL
  IN OUT VOID       *SramBuffer,    OPTIONAL
  IN OUT UINTN      *SramBufferSize OPTIONAL
  )
{
  SRAM_ERR_INFO  *SramErr;

  // Validate the SramBuffer parameter is not NULL.
  if (SramBuffer == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  // Validate the SramBufferSize parameter.
  if (*SramBufferSize < sizeof (SRAM_ERR_INFO)) {
    return EFI_BAD_BUFFER_SIZE;
  }

  // Retrieve the Sram error records information.
  SramErr = (SRAM_ERR_INFO *)SramBuffer;

  SramErrorHandler (SramErr);

  // Nothing to be returned.
  //*SramBufferSize = 0;

  return EFI_SUCCESS;
}

/**
  Initialization function for the driver.

  Registers MMI handlers to process error events on Base Element Ram and
  implements required protocols to create and publish the error source
  descriptors.

  @param[in] ImageHandle  Handle to image.
  @param[in] SystemTable  Pointer to System table.

  @retval EFI_SUCCESS  On successful installation of error event handler for
                       Base Element Ram.
  @retval Other        Failure in installing error event handlers for Base
                       Element Ram.
**/
EFI_STATUS
EFIAPI
SramMmDriverInitialize (
  IN EFI_HANDLE          ImageHandle,
  IN EFI_MM_SYSTEM_TABLE *SystemTable
  )
{
  EFI_MM_SYSTEM_TABLE *Mmst;
  EFI_STATUS          Status;
  EFI_HANDLE          DispatchHandle;

  ASSERT (SystemTable != NULL);
  Mmst = SystemTable;

  // Register MMI handlers for Base Element Ram error events.
  Status = Mmst->MmiHandlerRegister (
                    SramErrorEventMmiHandler,
                    &gArmSramEventHandlerGuid,
                    &DispatchHandle
                    );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Registration failed for Base Element Ram error event handler, \
       Status:%r\n",
      __FUNCTION__,
      Status
      ));
    return Status;
  }

  // Installs the HEST error source descriptor protocol.
  Status = SramInstallErrorSourceDescProtocol (SystemTable);
  if (EFI_ERROR (Status)) {
    Mmst->MmiHandlerUnRegister (DispatchHandle);
  }

  return Status;
}
