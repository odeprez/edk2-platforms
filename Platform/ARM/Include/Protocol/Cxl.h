/** @file
  Interface API of CXL Platform protocol.

  Declares the CXL Platform protocol interfaces, which are used by other
  platform drivers for collecting information regarding discovered remote
  memory nodes.

  Copyright (c) 2022, Arm Limited. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef CXL_H_
#define CXL_H_

/** Remote memory details
  *
  *  Remote memory region address in device address space and length of the
  *  region. These informations are passed using ACPI tables, where addressbase
  *  will be mapped to Host system address space.
**/
typedef struct {
  UINT64  DpaAddress;      /// Remote memory base in device address space
  UINT64  DpaLength;       /// Remote memory length lower bytes
} REMOTE_MEMORY_CONFIG;

/**
  Update Remote memory information

  Update the Remote memory details, Base address and Length, for number of
  Remote memory nodes discovered from the CXL devices.

  @param[out] RemoteMem    Array for updating Remote memory config.
  @param[in,out] MemCount  Number of supported remote memory nodes.

  @retval EFI_SUCCES    Memory is updated successfully
**/
typedef
EFI_STATUS
(EFIAPI *CXL_GET_REMOTE_MEM) (
  OUT REMOTE_MEMORY_CONFIG  *RemoteMem,
  IN OUT UINT32   *MemCount
  );

/**
  Return number of remote memory nodes discovered from CXL Mem devices.

  @retval UINT32    Number of supported remote memory nodes.
**/
typedef UINT32 (EFIAPI *CXL_GET_REMOTE_MEM_COUNT) (VOID);

/**
  * CXL Platform Protocol
  *
  * This protocol enables platform drivers to get number of memory range count
  * and associated memory configurations.
**/
typedef struct {
  CXL_GET_REMOTE_MEM CxlGetRemoteMem;
  CXL_GET_REMOTE_MEM_COUNT CxlGetRemoteMemCount;
} CXL_PLATFORM_PROTOCOL;

#endif /* CXL_H_ */
