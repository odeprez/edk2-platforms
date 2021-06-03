/** @file
  Cpu Standalone MM error handling driver.

  Standalone MM driver to handle 1-bit CE and DE generated on the Cpu. The
  driver creates a ARM specific Error Descriptor information. On error event
  publishes the CPER error record of Processor Error type.

  On an error event platform forwards the context information for both the
  security states, care must be taken to not pass the context information to
  OSPM if the error occurred when the Cpu was in secure mode.

  Copyright (c) 2021 - 2022, ARM Limited. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Specification Reference:
    - ARM Perseus Core TRM, revision r0p0
    - UEFI Reference Specification 2.9, Section N.2.4.4 ARM Processor Error
      Section
**/

#include <CpuMm.h>

/**
  Cpu error event handler.

  Cpu event handler with CommBuffer carrying the data structure of type
  CPU_ERR_INFO. Handles the 1-bit overflow CE and DE errors that occur
  on that processor.

  @param[in] DispatchHandle      The unique handle assigned to this handler
                                 by MmiHandlerRegister().
  @param[in] Context             Points to an optional handler context which
                                 was specified when the handler was
                                 registered.
  @param[in, out] CpuBuffer      Buffer carrying CPU information.
  @param[in, out] CpuBufferSize  The size of the CpuCommBuffer.

  @retval EFI_SUCCESS  Event handler successful.
  @retval Other        Failed to handle the event.
**/
STATIC
EFI_STATUS
EFIAPI
CpuErrorEventHandler (
  IN     EFI_HANDLE DispatchHandle,
  IN     CONST VOID *Context,      OPTIONAL
  IN OUT VOID       *CpuBuffer,    OPTIONAL
  IN OUT UINTN      *CpuBufferSize OPTIONAL
  )
{
  EFI_ACPI_6_3_GENERIC_ERROR_DATA_ENTRY_STRUCTURE *ErrBlockSectionDesc;
  EFI_ACPI_6_3_GENERIC_ERROR_STATUS_STRUCTURE     *ErrBlockStatusHeaderData;
  CPU_ERR_SECTION_DATA                            SectionData;
  CPU_ERR_INFO                                    *CpuErr;
  EFI_GUID                                        SectionType;
  VOID                                            *ErrBlockSectionData;
  UINTN                                           *ErrStatusBlock;
  UINT8                                           CorrectedError = 1;
  UINT8                                           ErrorType;
  UINT16                                          ErrorCount;

  // Validate the CpuBuffer parameter is not NULL.
  if (CpuBuffer == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  // Validate the CpuBufferSize parameter.
  if (*CpuBufferSize < sizeof (CPU_ERR_INFO)) {
    return EFI_BAD_BUFFER_SIZE;
  }

  // Retrieve the Cpu error records information.
  CpuErr = (CPU_ERR_INFO *)CpuBuffer;

  // Locate Error Status Data memory space within the firmware reserved memory
  // and populate Cper record for Processor error.
  ErrStatusBlock =
    (UINTN *)(FixedPcdGet64 (PcdCpuErrorDataBase) + ErrorStatusDataOffset);

  //
  // Block Status Header Information.
  // Locate Block Status Header base address and populate it with Error Status
  // Block Header information.
  //
  ErrBlockStatusHeaderData = (EFI_ACPI_6_3_GENERIC_ERROR_STATUS_STRUCTURE *)
                             ErrStatusBlock;
  ErrBlockStatusHeaderData->BlockStatus.UncorrectableErrorValid =
    ((CorrectedError == 0) ? 0 : 1);
  ErrBlockStatusHeaderData->BlockStatus.CorrectableErrorValid =
    ((CpuErr->ErrStatus & CPU_ERR_STATUS_CE_MASK) ? 1 : 0);
  ErrBlockStatusHeaderData->BlockStatus.MultipleUncorrectableErrors = 0x0;
  ErrBlockStatusHeaderData->BlockStatus.MultipleCorrectableErrors   = 0x0,
  ErrBlockStatusHeaderData->BlockStatus.ErrorDataEntryCount         = 0x1;
  ErrBlockStatusHeaderData->RawDataOffset =
    (sizeof (EFI_ACPI_6_3_GENERIC_ERROR_STATUS_STRUCTURE) +
     sizeof (EFI_ACPI_6_3_GENERIC_ERROR_DATA_ENTRY_STRUCTURE));
  ErrBlockStatusHeaderData->RawDataLength = 0;
  ErrBlockStatusHeaderData->DataLength =
    (sizeof(EFI_ACPI_6_3_GENERIC_ERROR_DATA_ENTRY_STRUCTURE) +
     CPU_SECTION_DATA_SIZE (CpuErr->SecurityState));
  ErrBlockStatusHeaderData->ErrorSeverity =
    ((CpuErr->ErrStatus & CPU_ERR_STATUS_CE_MASK) ?
     EFI_ACPI_6_3_ERROR_SEVERITY_CORRECTED :
     EFI_ACPI_6_3_ERROR_SEVERITY_CORRECTABLE);

  //
  // Section Descriptor Information.
  // Locate Section Descriptor base address and populate Error Status Section
  // Descriptor data.
  //
  ErrBlockSectionDesc = (EFI_ACPI_6_3_GENERIC_ERROR_DATA_ENTRY_STRUCTURE *)
                        (ErrBlockStatusHeaderData + 1);
  ErrBlockSectionDesc->ErrorSeverity =
                         ((CpuErr->ErrStatus & CPU_ERR_STATUS_CE_MASK) ?
                          EFI_ACPI_6_3_ERROR_SEVERITY_CORRECTED :
                          EFI_ACPI_6_3_ERROR_SEVERITY_CORRECTABLE);
  ErrBlockSectionDesc->Revision =
    EFI_ACPI_6_3_GENERIC_ERROR_DATA_ENTRY_REVISION;
  ErrBlockSectionDesc->ValidationBits = 0;
  if ((CpuErr->ErrStatus & CPU_ERR_STATUS_CE_MASK) != 0) {
    if ((CpuErr->ErrStatus & CPU_ERR_STATUS_OF_BIT) != 0) {
      ErrBlockSectionDesc->Flags |= BIT7;
    }
  } else if ((CpuErr->ErrStatus & CPU_ERR_STATUS_DE_BIT) != 0) {
    ErrBlockSectionDesc->Flags |= EFI_ERROR_SECTION_FLAGS_LATENT_ERROR;
  }
  ErrBlockSectionDesc->ErrorDataLength =
    CPU_SECTION_DATA_SIZE (CpuErr->SecurityState);
  SectionType = (EFI_GUID) EFI_ERROR_SECTION_PROCESSOR_SPECIFIC_ARM_GUID;
  CopyGuid ((EFI_GUID *)ErrBlockSectionDesc->SectionType, &SectionType);

  //
  // Section Data (Cper) Information.
  // Locate Section base address and populate Cpu Error Section data.
  // CPU Section Data = (EFI_ARM_PROCESSOR_ERROR_RECORD +
  //                    EFI_ARM_PROCESSOR_ERROR_INFORMATION * N +
  //                    EFI_ARM_PROCESSOR_CONTEXT_INFORMATION * P)
  // where, N = CPU_ERR_INFO_NUM
  //        P = CPU_CONTEXT_INFO_NUM
  //
  ErrBlockSectionData = (VOID *)(ErrBlockSectionDesc + 1);

  // Populate EFI_ARM_PROCESSOR_ERROR_RECORD structure.
  SectionData.CpuInfo.ValidFields =
    (EFI_ARM_PROC_ERROR_MPIDR_VALID | EFI_ARM_PROC_ERROR_RUNNING_STATE_VALID);
  SectionData.CpuInfo.ErrInfoNum = CPU_ERR_INFO_NUM;
  SectionData.CpuInfo.ContextInfoNum = CPU_CONTEXT_INFO_NUM;
  SectionData.CpuInfo.SectionLength =
    CPU_SECTION_DATA_SIZE (CpuErr->SecurityState);
  SectionData.CpuInfo.MPIDR_EL1 =
    CpuErr->ErrCtxEl1Reg[CONTEXT_STRUCT_EL1_MPIDR_FIELD];
  SectionData.CpuInfo.MIDR_EL1 =
    CpuErr->ErrCtxEl1Reg[CONTEXT_STRUCT_EL1_MIDR_FIELD];
  SectionData.CpuInfo.RunState = 0x1;
  SectionData.CpuInfo.PsciState =
    ((SectionData.CpuInfo.RunState) ? 0 : 1);

  // Populate EFI_ARM_PROCESSOR_ERROR_INFORMATION structure.
  ErrorCount = (CpuErr->ErrMisc0 & CPU_ERR_MISC0_CECR_COUNT_MASK) >>
               CPU_ERR_MISC0_CECR_COUNT_SHIFT;
  ErrorType = (CpuErr->ErrStatus & CPU_ERR_STATUS_SERR_MASK);
  if (ErrorType < CPU_ERR_STATUS_SERR_TLB) {
    ErrorType = EFI_ARM_PROC_ERROR_TYPE_CACHE;
  }
  else {
    ErrorType = EFI_ARM_PROC_ERROR_TYPE_TLB;
  }

  for (UINTN i = 0; i < CPU_ERR_INFO_NUM; ++i) {
    SectionData.CpuErrInfo[i].Version =
      EFI_ARM_PROCESSOR_ERROR_INFO_STRUCTURE_REVISION;
    SectionData.CpuErrInfo[i].Length =
      sizeof (EFI_ARM_PROCESSOR_ERROR_INFORMATION);
    SectionData.CpuErrInfo[i].ValidFields =
      (EFI_ARM_PROC_ERROR_INFO_MULTIPLE_ERROR_VALID |
       EFI_ARM_PROC_ERROR_INFO_FLAGS_VALID |
       EFI_ARM_PROC_ERROR_INFO_ERROR_INFO_VALID |
       EFI_ARM_PROC_ERROR_INFO_PHY_FAULT_ADDR_VALID);
    SectionData.CpuErrInfo[i].Type = ErrorType;
    SectionData.CpuErrInfo[i].MultipleError =
      ((CpuErr->ErrStatus & CPU_ERR_STATUS_DE_BIT) ?
       0 : ErrorCount);
    SectionData.CpuErrInfo[i].Flags =
      ((CpuErr->ErrStatus & CPU_ERR_STATUS_CE_MASK) ?
       EFI_ARM_PROC_ERROR_INFO_OVERFLOW_FLAG :
       EFI_ARM_PROC_ERROR_INFO_FIRST_ERROR_CAPTURED_FLAG);
    SectionData.CpuErrInfo[i].VirtualFaultAddress  = 0;
    SectionData.CpuErrInfo[i].PhysicalFaultAddress = CpuErr->ErrAddr;

    if (ErrorType == EFI_ARM_PROC_ERROR_TYPE_CACHE) {
      SectionData.CpuErrInfo[i].ErrorInfo.CacheErrorInfo.    \
      ValidFields =
        (EFI_TLB_ERROR_TRANSACTION_TYPE_VALID |
         EFI_TLB_ERROR_OPERATION_VALID |
         EFI_CACHE_ERROR_LEVEL_VALID |
         EFI_CACHE_ERROR_PROCESSOR_CONTEXT_CORRUPT_VALID |
         EFI_CACHE_ERROR_CORRECTED_VALID);
      SectionData.CpuErrInfo[i].ErrorInfo.CacheErrorInfo.    \
      TransactionType = EFI_CACHE_ERROR_TYPE_GENERIC;
      SectionData.CpuErrInfo[i].ErrorInfo.CacheErrorInfo.    \
      Operation = EFI_CACHE_ERROR_OPERATION_GENERIC_ERROR;
      SectionData.CpuErrInfo[i].ErrorInfo.CacheErrorInfo.    \
      Level =
        ((CpuErr->ErrMisc0 & CPU_ERR_MISC0_LVL_MASK) >>
         CPU_ERR_MISC0_LVL_SHIFT);
      SectionData.CpuErrInfo[i].ErrorInfo.CacheErrorInfo.ContextCorrupt = 0;
      SectionData.CpuErrInfo[i].ErrorInfo.CacheErrorInfo.    \
      ErrorCorrected =
        ((CpuErr->ErrStatus & CPU_ERR_STATUS_CE_MASK) ?
         1 : 0);
      SectionData.CpuErrInfo[i].ErrorInfo.CacheErrorInfo.PrecisePc = 0;
      SectionData.CpuErrInfo[i].ErrorInfo.CacheErrorInfo. RestartablePc = 0;
    }
    else if (ErrorType == EFI_ARM_PROC_ERROR_TYPE_TLB) {
      SectionData.CpuErrInfo[i].ErrorInfo.TlbErrorInfo.      \
      ValidFields =
        (EFI_TLB_ERROR_TRANSACTION_TYPE_VALID |
         EFI_TLB_ERROR_OPERATION_VALID |
         EFI_TLB_ERROR_LEVEL_VALID |
         EFI_TLB_ERROR_PROCESSOR_CONTEXT_CORRUPT_VALID |
         EFI_TLB_ERROR_CORRECTED_VALID);
      SectionData.CpuErrInfo[i].ErrorInfo.TlbErrorInfo.      \
      TransactionType = EFI_TLB_ERROR_TYPE_GENERIC;
      SectionData.CpuErrInfo[i].ErrorInfo.TlbErrorInfo.      \
      Operation = EFI_TLB_ERROR_OPERATION_GENERIC_ERROR;
      SectionData.CpuErrInfo[i].ErrorInfo.TlbErrorInfo.      \
      Level =
        ((CpuErr->ErrMisc0 & CPU_ERR_MISC0_LVL_MASK) >>
         CPU_ERR_MISC0_LVL_MASK),
      SectionData.CpuErrInfo[i].ErrorInfo.TlbErrorInfo.ContextCorrupt = 0;
      SectionData.CpuErrInfo[i].ErrorInfo.TlbErrorInfo.      \
      ErrorCorrected =
        ((CpuErr->ErrStatus & CPU_ERR_STATUS_CE_MASK) ?
         1 : 0);
      SectionData.CpuErrInfo[i].ErrorInfo.TlbErrorInfo.PrecisePc = 0;
      SectionData.CpuErrInfo[i].ErrorInfo.TlbErrorInfo.RestartablePc = 0;
    }
  }

  //
  // Populate EFI_ARM_PROCESSOR_CONTEXT_INFORMATION data.
  //
  // Only if the error occurs in NON_SECURE state then the context information
  // is passed to OSPM.
  if (CpuErr->SecurityState == NON_SECURE) {
    SectionData.CpuContextInfo[0].Version = 0;
    SectionData.CpuContextInfo[0].RegisterContextType =
      EFI_REG_CONTEXT_TYPE_4;
    SectionData.CpuContextInfo[0].RegisterArraySize =
      sizeof (EFI_CONTEXT_REGISTER_ARRAY_INFO);
    CopyMem (
      (VOID *)&SectionData.CpuContextInfo[0].RegisterArray.Type4SysRegs,
      (VOID *)&CpuErr->ErrCtxGpr,
      sizeof (EFI_ARM_AARCH64_CONTEXT_GPR)
      );

    SectionData.CpuContextInfo[1].Version = 0;
    SectionData.CpuContextInfo[1].RegisterContextType =
      EFI_REG_CONTEXT_TYPE_5;
    SectionData.CpuContextInfo[1].RegisterArraySize =
      sizeof (EFI_CONTEXT_REGISTER_ARRAY_INFO);
    CopyMem (
      (VOID *)&SectionData.CpuContextInfo[1].RegisterArray.Type5SysRegs,
      (VOID *)&CpuErr->ErrCtxEl1Reg,
      sizeof (EFI_ARM_AARCH64_EL1_CONTEXT_SYSTEM_REGISTERS)
      );

    SectionData.CpuContextInfo[2].Version = 0;
    SectionData.CpuContextInfo[2] .RegisterContextType =
      EFI_REG_CONTEXT_TYPE_6;
    SectionData.CpuContextInfo[2].RegisterArraySize =
      sizeof (EFI_CONTEXT_REGISTER_ARRAY_INFO);
    CopyMem (
      (VOID *)&SectionData.CpuContextInfo[2].RegisterArray.Type6SysRegs,
      (VOID *)&CpuErr->ErrCtxEl2Reg,
      sizeof (EFI_ARM_AARCH64_EL2_CONTEXT_SYSTEM_REGISTERS)
     );
  }

  // Copy Section Data(Cper) information.
  CopyMem (
    ErrBlockSectionData,
    (VOID *)&SectionData,
    CPU_SECTION_DATA_SIZE (CpuErr->SecurityState)
    );

  // Nothing to be returned.
  *CpuBufferSize = 0;

  return EFI_SUCCESS;
}

/**
  Initialization function of the driver.

  Registers Mmi handler to process error events on Cpu and implements required
  protocols to create and publish the error source descriptors.

  @param[in] ImageHandle  Handle to image.
  @param[in] SystemTable  Pointer to System table.

  @retval EFI_SUCCESS  On successful installation of error event handlers for
                       Cpu.
  @retval Other        Failure in installing error event handlers for Cpu.
**/
EFI_STATUS
EFIAPI
CpuMmDriverInitialize (
  IN EFI_HANDLE          ImageHandle,
  IN EFI_MM_SYSTEM_TABLE *SystemTable
  )
{
  EFI_MM_SYSTEM_TABLE *Mmst;
  EFI_STATUS          Status;
  EFI_HANDLE          DispatchHandle;

  ASSERT (SystemTable != NULL);
  Mmst = SystemTable;

  // Register Mmi handlers for Cpu error events.
  Status = Mmst->MmiHandlerRegister (
                    CpuErrorEventHandler,
                    &gArmCpuEventHandlerGuid,
                    &DispatchHandle
                    );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Registration failed for Cpu error event handler, Status:%r\n",
      __FUNCTION__,
      Status
      ));

     return Status;
  }

  // Implement the Hest error source descriptor protocol.
  Status = CpuInstallErrorSourceDescProtocol (SystemTable);
  if (EFI_ERROR (Status)) {
    Mmst->MmiHandlerUnRegister (DispatchHandle);
  }

  return Status;
}
