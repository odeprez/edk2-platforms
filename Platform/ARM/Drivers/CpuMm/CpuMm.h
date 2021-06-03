/** @file
  Cpu Standalone MM error source driver file.

  Defines bit fields and macros specific to Cpu Error Records. Also adds
  data structures specific to Cpu error Section(Cper) data.

  Copyright (c) 2021 - 2022, ARM Limited. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Specification Reference:
    - ARM Perseus Core TRM, revision r0p0
    - UEFI Reference Specification 2.8, Section N.2.4.4 ARM Processor Error
      Section
**/

#ifndef CPU_MM_DRIVER_H_
#define CPU_MM_DRIVER_H_

#include <Base.h>
#include <Guid/Cper.h>
#include <IndustryStandard/Acpi.h>
#include <Library/ArmLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Protocol/MmHestErrorSourceProtocol.h>

//
// Cpu Section Data(Cper) helper macros.
//
// Number of Processor Error Information data structers and Context Information
// data structures that will be part of Cpu Section(Cper) data.
#define CPU_ERR_INFO_NUM     1
// 3 data structures to carry Type4, Type5 and Type6 Context System Register
// information.
#define CPU_CONTEXT_INFO_NUM 3

// Total Cpu Section Data(Cper) Size. The security state of the error will
// decide wheather the context information will be added to Cpu Section data.
#define CPU_SECTION_DATA_SIZE(state)                                          \
  (sizeof (EFI_ARM_PROCESSOR_ERROR_RECORD) +                                  \
   (sizeof (EFI_ARM_PROCESSOR_ERROR_INFORMATION) * CPU_ERR_INFO_NUM) +        \
   ((sizeof (EFI_ARM_PROCESSOR_CONTEXT_INFORMATION) * CPU_CONTEXT_INFO_NUM) * \
    state))

// Cpu Section Data(Cper) Structure.
typedef struct {
  EFI_ARM_PROCESSOR_ERROR_RECORD        CpuInfo;
  EFI_ARM_PROCESSOR_ERROR_INFORMATION   CpuErrInfo[CPU_ERR_INFO_NUM];
  EFI_ARM_PROCESSOR_CONTEXT_INFORMATION CpuContextInfo[CPU_CONTEXT_INFO_NUM];
} CPU_ERR_SECTION_DATA;

// Error Record Status register bit fields.
#define CPU_ERR_STATUS_SERR_MASK 0xFF//(BIT0|BIT1|BIT2|BIT3|BIT4|BIT5|BIT6|BIT7)
#define CPU_ERR_STATUS_PN_BIT    BIT22
#define CPU_ERR_STATUS_DE_BIT    BIT23
#define CPU_ERR_STATUS_CE_MASK   (BIT24|BIT25)
#define CPU_ERR_STATUS_MV_BIT    BIT26
#define CPU_ERR_STATUS_OF_BIT    BIT27
#define CPU_ERR_STATUS_V_BIT     BIT30
#define CPU_ERR_STATUS_AV_BIT    BIT31

#define CPU_ERR_STATUS_SERR_TLB 0x08

// Error Record Misc0 register bit fields.
#define CPU_ERR_MISC0_LVL_MASK         (BIT1|BIT2|BIT3)
#define CPU_ERR_MISC0_LVL_SHIFT        1
#define CPU_ERR_MISC0_CECR_COUNT_MASK  0xFF00000000
//  (BIT32|BIT33|BIT34|BIT35|BIT36|BIT36|BIT37|BIT38|BIT39)
#define CPU_ERR_MISC0_CECR_COUNT_SHIFT 32

// Cpu Section Data(Cper) Context Information helper macros.
#define GPR_ARR_SIZE \
  (sizeof (EFI_ARM_AARCH64_CONTEXT_GPR) / sizeof (UINT64))
#define EL1_REG_ARR_SIZE \
  (sizeof (EFI_ARM_AARCH64_EL1_CONTEXT_SYSTEM_REGISTERS) / sizeof (UINT64))
#define EL2_REG_ARR_SIZE \
  (sizeof (EFI_ARM_AARCH64_EL2_CONTEXT_SYSTEM_REGISTERS) / sizeof (UINT64))
#define EL3_REG_ARR_SIZE \
  (sizeof (EFI_ARM_AARCH64_EL3_CONTEXT_SYSTEM_REGISTERS) / sizeof (UINT64))

#define CONTEXT_STRUCT_EL1_MPIDR_FIELD \
  (OFFSET_OF (EFI_ARM_AARCH64_EL1_CONTEXT_SYSTEM_REGISTERS, MPIDR_EL1) / \
   sizeof (UINT64))
#define CONTEXT_STRUCT_EL1_MIDR_FIELD \
  (OFFSET_OF (EFI_ARM_AARCH64_EL1_CONTEXT_SYSTEM_REGISTERS, MIDR_EL1) /  \
   sizeof (UINT64))

//
// Security States.
//
#define SECURE     0
#define NON_SECURE 1

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
                                   sizeof (CPU_ERR_SECTION_DATA))

//
// Data structure to communicate Cpu Error Information.
//
typedef struct {
  UINT64 ErrStatus;
  UINT64 ErrMisc0;
  UINT64 ErrAddr;
  UINT64 SecurityState;
  UINT64 ErrCtxGpr[GPR_ARR_SIZE];
  UINT64 ErrCtxEl1Reg[EL1_REG_ARR_SIZE];
  UINT64 ErrCtxEl2Reg[EL2_REG_ARR_SIZE];
  UINT64 ErrCtxEl3Reg[EL3_REG_ARR_SIZE];
} CPU_ERR_INFO;

/**
  Allow reporting of supported Cpu error sources.

  Installs the HEST Error Source Descriptor protocol handler that publishes the
  supported Cpu error sources as error source descriptor.

  @param[in] MmSystemTable  Pointer to System table.

  @retval EFI_SUCCESS            Protocol installation successful.
  @retval EFI_INVALID_PARAMETER  Invalid system table parameter.
  @retval Other                  Other error while installing the protocol.
**/
EFI_STATUS
CpuInstallErrorSourceDescProtocol (
  IN EFI_MM_SYSTEM_TABLE *MmSystemTable
  );

#endif // CPU_MM_DRIVER_H_
