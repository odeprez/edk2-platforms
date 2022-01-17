/** @file
  Heterogeneous Memory Attribute Table (HMAT) Table Generator.

  The (HMAT) describes the memory attributes, such as bandwidth and latency
  details, related to Memory Proximity Domains. The software is expected
  to use this information as a hint for optimization, or when the system has
  heterogeneous memory.

  Copyright (c) 2022, Arm Limited. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Specification Reference:
      - ACPI 6.4, Chapter 5.2.27 Heterogeneous Memory Attribute Table (HMAT)
**/

#include "AcpiTableGenerator.h"

#define CHIP_CNT                      2
#define INITATOR_PROXIMITY_DOMAIN_CNT 2
#define TARGET_PROXIMITY_DOMAIN_CNT   2

/* HMAT Table */
typedef struct InitiatorTargetProximityMatrix {
  UINT32  InitatorProximityDomain[INITATOR_PROXIMITY_DOMAIN_CNT];
  UINT32  TargetProximityDomain[TARGET_PROXIMITY_DOMAIN_CNT];
  UINT16  MatrixEntry[INITATOR_PROXIMITY_DOMAIN_CNT *
                      TARGET_PROXIMITY_DOMAIN_CNT];
} INITIATOR_TARGET_PROXIMITY_MATRIX;

typedef struct {
  EFI_ACPI_6_4_HETEROGENEOUS_MEMORY_ATTRIBUTE_TABLE_HEADER Header;
  EFI_ACPI_6_4_HMAT_STRUCTURE_MEMORY_PROXIMITY_DOMAIN_ATTRIBUTES
    Proximity[CHIP_CNT];
  EFI_ACPI_6_4_HMAT_STRUCTURE_SYSTEM_LOCALITY_LATENCY_AND_BANDWIDTH_INFO
    LatencyInfo;
  INITIATOR_TARGET_PROXIMITY_MATRIX  Matrix;
} EFI_ACPI_HETEROGENEOUS_MEMORY_ATTRIBUTE_TABLE;

EFI_ACPI_HETEROGENEOUS_MEMORY_ATTRIBUTE_TABLE Hmat = {
  // Header
  {
    ARM_ACPI_HEADER (
      EFI_ACPI_6_4_HETEROGENEOUS_MEMORY_ATTRIBUTE_TABLE_SIGNATURE,
      EFI_ACPI_HETEROGENEOUS_MEMORY_ATTRIBUTE_TABLE,
      EFI_ACPI_6_4_HETEROGENEOUS_MEMORY_ATTRIBUTE_TABLE_REVISION
      ),
    {
      EFI_ACPI_RESERVED_BYTE,
      EFI_ACPI_RESERVED_BYTE,
      EFI_ACPI_RESERVED_BYTE,
      EFI_ACPI_RESERVED_BYTE
    },
  },

  // Memory Proximity Domain
  {
    EFI_ACPI_6_4_HMAT_STRUCTURE_MEMORY_PROXIMITY_DOMAIN_ATTRIBUTES_INIT (
      1, 0x0, 0x0),
    EFI_ACPI_6_4_HMAT_STRUCTURE_MEMORY_PROXIMITY_DOMAIN_ATTRIBUTES_INIT (
      1, 0x0, 0x1),
   },

  // Latency Info
  EFI_ACPI_6_4_HMAT_STRUCTURE_SYSTEM_LOCALITY_LATENCY_AND_BANDWIDTH_INFO_INIT (
    0, 0, 0, INITATOR_PROXIMITY_DOMAIN_CNT, TARGET_PROXIMITY_DOMAIN_CNT, 100),
  {
    {0, 1},
    {0, 1},
    {
      //
      // The latencies mentioned in this table are hypothetical values and
      // represents typical latency between two chips. These values are
      // applicable only for RD-N1-Edge dual-chip fixed virtual platform and
      // should not be reused for other platforms.
      //
      10, 20,
      20, 10,
    }
  }
};

/**
  Installs the HMAT table.

  @param[in]  mAcpiTableProtocol  Handle to AcpiTableProtocol.

  @retval  EFI_SUCCESS  On successful installation of HMAT table.
  @retval  Other        Failure in installing HMAT table.
**/
EFI_STATUS
EFIAPI
HmatTableGenerator (
  IN EFI_ACPI_TABLE_PROTOCOL *mAcpiTableProtocol
  )
{
  EFI_STATUS Status;
  UINTN AcpiTableHandle;

  Status = mAcpiTableProtocol->InstallAcpiTable (
                                 mAcpiTableProtocol,
                                 &Hmat,
                                 sizeof (Hmat),
                                 &AcpiTableHandle
                                 );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: HMAT table installation failed, status: %r\n",
      __FUNCTION__,
      Status
      ));
  } else {
    DEBUG ((
      DEBUG_INFO,
      "Installed HMAT table \n"
      ));
  }

  return Status;
}

