/** @file
  SRAT Table Generator.

  SRAT table provides information that allows OSPM to associate devices such as
  processors with system locality / proximity domains and clock domains.

  Copyright (c) 2022, Arm Limited. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Specification Reference:
      - ACPI 6.4, Chapter 5.2.16 System Resource Affinity Table (SRAT)
**/

#include <Library/DxeServicesTableLib.h>
#include <Library/HobLib.h>
#include "AcpiTableGenerator.h"

STATIC EFI_ACPI_6_4_MEMORY_AFFINITY_STRUCTURE  *RemoteMemory;

EFI_ACPI_6_4_SYSTEM_RESOURCE_AFFINITY_TABLE_HEADER  SratHeader = {
  ARM_ACPI_HEADER (
    EFI_ACPI_6_4_SYSTEM_RESOURCE_AFFINITY_TABLE_SIGNATURE,
    EFI_ACPI_6_4_SYSTEM_RESOURCE_AFFINITY_TABLE_HEADER,
    EFI_ACPI_6_4_SYSTEM_RESOURCE_AFFINITY_TABLE_REVISION
    ),
    0x00000001,
    EFI_ACPI_RESERVED_QWORD
};

EFI_ACPI_6_4_GICC_AFFINITY_STRUCTURE  Gicc[8] = {
  EFI_ACPI_6_4_GICC_AFFINITY_STRUCTURE_INIT (
    0x0, 0x00000000, 0x00000001, 0x00000000),
  EFI_ACPI_6_4_GICC_AFFINITY_STRUCTURE_INIT (
    0x0, 0x00000001, 0x00000001, 0x00000000),
  EFI_ACPI_6_4_GICC_AFFINITY_STRUCTURE_INIT (
    0x0, 0x00000002, 0x00000001, 0x00000000),
  EFI_ACPI_6_4_GICC_AFFINITY_STRUCTURE_INIT (
    0x0, 0x00000003, 0x00000001, 0x00000000),
  EFI_ACPI_6_4_GICC_AFFINITY_STRUCTURE_INIT (
    0x0, 0x00000004, 0x00000001, 0x00000000),
  EFI_ACPI_6_4_GICC_AFFINITY_STRUCTURE_INIT (
    0x0, 0x00000005, 0x00000001, 0x00000000),
  EFI_ACPI_6_4_GICC_AFFINITY_STRUCTURE_INIT (
    0x0, 0x00000006, 0x00000001, 0x00000000),
  EFI_ACPI_6_4_GICC_AFFINITY_STRUCTURE_INIT (
    0x0, 0x00000007, 0x00000001, 0x00000000),
#if ((CORE_COUNT * CLUSTER_COUNT) > 8)
  EFI_ACPI_6_4_GICC_AFFINITY_STRUCTURE_INIT (
    0x0, 0x00000008, 0x00000001, 0x00000000),
  EFI_ACPI_6_4_GICC_AFFINITY_STRUCTURE_INIT (
    0x0, 0x00000009, 0x00000001, 0x00000000),
  EFI_ACPI_6_4_GICC_AFFINITY_STRUCTURE_INIT (
    0x0, 0x0000000A, 0x00000001, 0x00000000),
  EFI_ACPI_6_4_GICC_AFFINITY_STRUCTURE_INIT (
    0x0, 0x0000000B, 0x00000001, 0x00000000),
  EFI_ACPI_6_4_GICC_AFFINITY_STRUCTURE_INIT (
    0x0, 0x0000000C, 0x00000001, 0x00000000),
  EFI_ACPI_6_4_GICC_AFFINITY_STRUCTURE_INIT (
    0x0, 0x0000000D, 0x00000001, 0x00000000),
  EFI_ACPI_6_4_GICC_AFFINITY_STRUCTURE_INIT (
    0x0, 0x0000000E, 0x00000001, 0x00000000),
  EFI_ACPI_6_4_GICC_AFFINITY_STRUCTURE_INIT (
      0x0, 0x0000000F, 0x00000001, 0x00000000),
#endif
};

EFI_ACPI_6_4_MEMORY_AFFINITY_STRUCTURE  LocalMemory[3] = {
  // Memory at 32-bit address space
  EFI_ACPI_6_4_MEMORY_AFFINITY_STRUCTURE_INIT (
    0x0, FixedPcdGet64 (PcdSystemMemoryBase),
    FixedPcdGet64 (PcdSystemMemorySize), 0x00000001),
  // Memory at 64-bit address space
  EFI_ACPI_6_4_MEMORY_AFFINITY_STRUCTURE_INIT (
    0x0, FixedPcdGet64 (PcdDramBlock2Base),
    FixedPcdGet64 (PcdDramBlock2Size), 0x00000001),
  // MmBuffer region
  EFI_ACPI_6_4_MEMORY_AFFINITY_STRUCTURE_INIT (
    0x0, FixedPcdGet64 (PcdMmBufferBase),
    FixedPcdGet64 (PcdMmBufferSize), 0x00000001),
};

/**
  Fetch the details of Remote Memory Node, using CXL protocol interfaces.

  By using CXL platform protocol interfaces, fetch number CXL remote memory
  nodes and their corresponding configurations(Base address, length).

  @param[out]  RemoteMemCount  Number of Remote CXL memory nodes.

  @retval  RemoteMem       Remote memory configuraiton on successful fetching
                           of remote memory configuration.
  @retval  Zero            Returns Zero on failure.
**/
STATIC
REMOTE_MEMORY_CONFIG  *
FetchRemoteCxlMem (OUT UINT32  *RemoteMemCount)
{
  EFI_STATUS            Status;
  CXL_PLATFORM_PROTOCOL *CxlProtocol;
  REMOTE_MEMORY_CONFIG  *RemoteMem;
  UINT32                RemoteMemNumber;

  Status = gBS->LocateProtocol (
                  &gCxlPlatformProtocolGuid,
                  NULL,
                  (VOID **)&CxlProtocol
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to locate CXL Protocol, status: %r\n",
      __FUNCTION__,
      Status
      ));
    return 0;
  }

  RemoteMemNumber = CxlProtocol->CxlGetRemoteMemCount ();
  if (RemoteMemNumber) {
    RemoteMem = (REMOTE_MEMORY_CONFIG *) AllocateZeroPool (
                                           sizeof (REMOTE_MEMORY_CONFIG) *
                                           RemoteMemNumber
                                           );
    if (RemoteMem == NULL) {
      DEBUG ((DEBUG_WARN, "No memory for Remote Memory affinity structure:\n"));
      return 0;
    }
  } else {
      DEBUG ((DEBUG_INFO, "No Remote Memory node exists:\n"));
      return 0;
  }

  Status = CxlProtocol->CxlGetRemoteMem (RemoteMem, &RemoteMemNumber);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to get CXL Remote memory details: %r\n",
      __FUNCTION__,
      Status
      ));
    FreePool(RemoteMem);
    return 0;
  }

  *RemoteMemCount = RemoteMemNumber;

  return RemoteMem;
}

/**
  Fetch the details of all Remote Memory Node, using CXL protocol interfaces.
  Prepare SRAT table structure by combining LocalMemoryNode and
  RemoteMemoryNode information. Thereafter installs the SRAT table.

  @param[in]  mAcpiTableProtocol  Handle to AcpiTableProtocol.

  @retval  EFI_SUCCESS  On successful installation of SRAT table.
  @retval  Other        Failure in installing SRAT table.
**/
EFI_STATUS
EFIAPI
SratTableGenerator (
  IN EFI_ACPI_TABLE_PROTOCOL *mAcpiTableProtocol
  )
{
  EFI_STATUS            Status;
  UINTN                 AcpiTableHandle;
  UINTN                 MemoryNodeCount;
  UINTN                 TableSize;
  UINT8                 Idx;
  VOID                  *Srat, *SratDataNext;
  REMOTE_MEMORY_CONFIG  *RemoteMem;
  UINT32                RemoteMemCount;
  UINT64                HostPhysicalBase;
  UINT64                MemDevicePhysicalBase;

  RemoteMem = FetchRemoteCxlMem (&RemoteMemCount);

  if (RemoteMemCount) {
    RemoteMemory = AllocateZeroPool (
                     sizeof (EFI_ACPI_6_4_MEMORY_AFFINITY_STRUCTURE) *
                     RemoteMemCount
                     );

    if (RemoteMemory == NULL) {
      DEBUG ((DEBUG_WARN, "No memory for Remote Memory affinity structure:\n"));
      RemoteMemCount = 0;
    } else {
      HostPhysicalBase = FixedPcdGet64 (PcdRemoteMemoryBase);

      for (Idx = 0; Idx < RemoteMemCount; Idx++) {
        RemoteMemory[Idx].Type = 1;
        RemoteMemory[Idx].Length =
          sizeof (EFI_ACPI_6_4_MEMORY_AFFINITY_STRUCTURE);
        RemoteMemory[Idx].ProximityDomain = 1;
        RemoteMemory[Idx].Reserved1 = EFI_ACPI_RESERVED_WORD;

        MemDevicePhysicalBase = HostPhysicalBase + RemoteMem[Idx].DpaAddress;
        RemoteMemory[Idx].AddressBaseLow =
          MemDevicePhysicalBase & LOWER_BYTES_MASK;
        RemoteMemory[Idx].AddressBaseHigh =
          MemDevicePhysicalBase >> LOWER_BYTES_SHIFT;

        RemoteMemory[Idx].LengthLow =
          RemoteMem[Idx].DpaLength & LOWER_BYTES_MASK;
        RemoteMemory[Idx].LengthHigh =
          RemoteMem[Idx].DpaLength >> LOWER_BYTES_SHIFT;

        RemoteMemory[Idx].Reserved2 = EFI_ACPI_RESERVED_WORD;
        RemoteMemory[Idx].Flags = 0x1;
        RemoteMemory[Idx].Reserved3 = EFI_ACPI_RESERVED_WORD;

        HostPhysicalBase += RemoteMem[Idx].DpaLength;
      }

      Status = gDS->AddMemorySpace (
                      EfiGcdMemoryTypeSystemMemory,
                      FixedPcdGet64 (PcdRemoteMemoryBase),
                      (HostPhysicalBase - FixedPcdGet64 (PcdRemoteMemoryBase)),
                      EFI_MEMORY_WC | EFI_MEMORY_WT | EFI_MEMORY_WB);

      Status = gDS->SetMemorySpaceAttributes (
                      FixedPcdGet64 (PcdRemoteMemoryBase),
                      (HostPhysicalBase - FixedPcdGet64 (PcdRemoteMemoryBase)),
                      EFI_MEMORY_WB);
      if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: Failed to set memory attributes: %r\n",
          __FUNCTION__,
          Status
          ));
        FreePool (RemoteMem);
        return 0;
      }
    }

    FreePool (RemoteMem);
  }

  MemoryNodeCount = FixedPcdGet32 (PcdNumLocalMemBlock);
//TODO: Not including CXL memory area under a NUMA node. It needs
//further debugging.
  //MemoryNodeCount += RemoteMemCount;
  TableSize = sizeof (Gicc) +
              sizeof (SratHeader) +
              (MemoryNodeCount *
              sizeof (EFI_ACPI_6_4_MEMORY_AFFINITY_STRUCTURE));

  Srat = AllocatePool (TableSize);

  if (Srat == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to allocate memory for SRAT table\n",
      __FUNCTION__
      ));

    FreePool(RemoteMemory);
    return EFI_OUT_OF_RESOURCES;
  }

  CopyMem (
    Srat,
    &SratHeader,
    sizeof (SratHeader)
    );

  SratDataNext = ((char *)Srat + sizeof (SratHeader));
  CopyMem (
    SratDataNext,
    Gicc,
    sizeof (Gicc)
    );

  SratDataNext = ((char *)SratDataNext + sizeof (Gicc));
  CopyMem (
    SratDataNext,
    LocalMemory,
    sizeof (LocalMemory)
    );

/**
 * TODO: CXL expansion memory region is also added in CEDT table. Kernel picks
 * up from either SRAT or CMFWS. Having presence in both place causing a dummy
 * NUMA node. Temporarily the entry in SRAT is removed, it will be looked into
 * further.
**/
#if 0
  SratDataNext = ((char *)SratDataNext + sizeof (LocalMemory));
  if (RemoteMemCount) {
    CopyMem (
      SratDataNext,
      RemoteMemory,
      sizeof (EFI_ACPI_6_4_MEMORY_AFFINITY_STRUCTURE) *
      RemoteMemCount
      );
  }
#endif

  ((EFI_ACPI_DESCRIPTION_HEADER *)Srat)->Length = TableSize;

  Status = mAcpiTableProtocol->InstallAcpiTable (
             mAcpiTableProtocol,
             (EFI_ACPI_6_4_SYSTEM_RESOURCE_AFFINITY_TABLE_HEADER *)Srat,
             TableSize,
             &AcpiTableHandle
             );

  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: SRAT table installation failed, status: %r\n",
      __FUNCTION__,
      Status
      ));
  } else {
    DEBUG ((DEBUG_INFO, "Installed SRAT table \n"));
  }

  return Status;
}
