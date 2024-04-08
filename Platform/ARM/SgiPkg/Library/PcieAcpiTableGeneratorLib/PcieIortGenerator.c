/** @file
  Function declared for IORT Acpi table generator.

  Copyright (c) 2022, ARM Limited. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <IndustryStandard/Acpi62.h>
#include <IndustryStandard/IoRemappingTable.h>
#include <Library/AcpiLib.h>
#include <Library/ArmLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/AcpiTable.h>

#include "SgiAcpiHeader.h"
#include "SgiPlatform.h"

/* Number of Device IDs = (Bus * Number of devices * Number of functions) */
#define PCI_NUM_IDS(Bus) (Bus * 32 * 8)

#pragma pack(1)

typedef struct
{
  EFI_ACPI_6_0_IO_REMAPPING_ITS_NODE       ItsNode;
  UINT32                                   ItsIdentifiers;
} ARM_EFI_ACPI_6_0_IO_REMAPPING_ITS_NODE;

typedef struct
{
  EFI_ACPI_6_0_IO_REMAPPING_RC_NODE        RcNode;
  EFI_ACPI_6_0_IO_REMAPPING_ID_TABLE       RcIdMap[];
} ARM_EFI_ACPI_6_0_IO_REMAPPING_RC_NODE;

typedef struct
{
  EFI_ACPI_6_0_IO_REMAPPING_SMMU3_NODE     SmmuNode;
  EFI_ACPI_6_0_IO_REMAPPING_ID_TABLE       SmmuIdMap[];
} ARM_EFI_ACPI_6_0_IO_REMAPPING_PCIE_SMMU3_NODE;

typedef struct 
{
  EFI_ACPI_6_0_IO_REMAPPING_TABLE * Header;
  ARM_EFI_ACPI_6_0_IO_REMAPPING_ITS_NODE * ItsBase;
  ARM_EFI_ACPI_6_0_IO_REMAPPING_PCIE_SMMU3_NODE * SmmuBase;
  ARM_EFI_ACPI_6_0_IO_REMAPPING_RC_NODE * RcNodeBase;
  UINTN ItsNodesSize;
  UINTN SmmuNodesSize;
  UINTN RcNodesSize;
  UINTN MaxTableSize;
} IORT_GENERATOR_CONTEXT;

#pragma pack()

STATIC EFI_ACPI_6_0_IO_REMAPPING_TABLE TemplateHeader = {
  ARM_ACPI_HEADER  // EFI_ACPI_DESCRIPTION_HEADER
    (
     EFI_ACPI_6_2_IO_REMAPPING_TABLE_SIGNATURE,
     EFI_ACPI_6_0_IO_REMAPPING_TABLE,
     EFI_ACPI_IO_REMAPPING_TABLE_REVISION_00
    ),
  0,
  sizeof (EFI_ACPI_6_0_IO_REMAPPING_TABLE),  // NodeOffset
  0,  // Reserved
};

/* ARM_EFI_ACPI_6_0_IO_REMAPPING_ITS_NODE0 */
STATIC ARM_EFI_ACPI_6_0_IO_REMAPPING_ITS_NODE TemplateIts = {
  /* EFI_ACPI_6_0_IO_REMAPPING_ITS_NODE */
  {
    /* EFI_ACPI_6_0_IO_REMAPPING_NODE */
    {
      EFI_ACPI_IORT_TYPE_ITS_GROUP,                    /* Type */
      sizeof (ARM_EFI_ACPI_6_0_IO_REMAPPING_ITS_NODE), /* Length */
      0,                                               /* Revision */
      0,                                               /* Reserved */
      0,                                               /* NumIdMappings */
      0,                                               /* IdReference */
    },
    1,                                                 /* ITS count */
  },
  0                                                    /* GIC ITS Identifiers */
};

/* SMMU */
STATIC ARM_EFI_ACPI_6_0_IO_REMAPPING_PCIE_SMMU3_NODE TemplateSmmu = { /* EFI_ACPI_6_0_IO_REMAPPING_SMMU3_NODE */
  {
    /* EFI_ACPI_6_0_IO_REMAPPING_NODE */
    {
      EFI_ACPI_IORT_TYPE_SMMUv3,                         /* Type */
      sizeof (ARM_EFI_ACPI_6_0_IO_REMAPPING_PCIE_SMMU3_NODE), /* Length */
      2,                                                 /* Revision */
      0,                                                 /* Reserved */
      1,                                                 /* NumIdMapping */
      OFFSET_OF (ARM_EFI_ACPI_6_0_IO_REMAPPING_PCIE_SMMU3_NODE,
          SmmuIdMap),                                      /* IdReference */
    },
    0,                                                   /* Base address */
    EFI_ACPI_IORT_SMMUv3_FLAG_COHAC_OVERRIDE,            /* Flags */
    0,                                                   /* Reserved */
    0,                                                   /* VATOS address */
    EFI_ACPI_IORT_SMMUv3_MODEL_GENERIC,                  /* SMMUv3 Model */
    FixedPcdGet32 (PcdSmmuEventGsiv),                    /* Event */
    FixedPcdGet32 (PcdSmmuPriGsiv),                      /* Pri */
    FixedPcdGet32 (PcdSmmuGErrorGsiv),                   /* Gerror */
    FixedPcdGet32 (PcdSmmuSyncGsiv),                     /* Sync */
    0,                                             /* Proximity domain */
    0,                                             /* DevIDMappingIndex */
  },
  /* EFI_ACPI_6_0_IO_REMAPPING_ID_TABLE */
  {
    {
      0x0,                                         /* InputBase */
      0x0,                                         /* NumIds */
      FixedPcdGet32 (PcdSmmuDevIDBase),            /* OutputBase */
      0x0,                                         /* OutputReference */
      EFI_ACPI_IORT_ID_MAPPING_FLAGS_SINGLE,       /* Flags */
    },
  },
};

STATIC
VOID
AddHeader (
    IN EFI_ACPI_6_0_IO_REMAPPING_TABLE * Header
    )
{
  CopyMem(Header, &TemplateHeader, sizeof(TemplateHeader));
}

STATIC
VOID
AddItsNodes (
    IN SGI_PCIE_IO_BLOCK_LIST * CONST IoBlockList,
    IN OUT ARM_EFI_ACPI_6_0_IO_REMAPPING_ITS_NODE * ItsNodes,
    OUT EFI_ACPI_6_0_IO_REMAPPING_TABLE * Header
    )
{
  ARM_EFI_ACPI_6_0_IO_REMAPPING_ITS_NODE * CurrentNode;
  CONST SGI_PCIE_IO_BLOCK * IoBlock;
  UINTN LoopIoBlock;

  CurrentNode = ItsNodes;
  IoBlock = IoBlockList->IoBlocks;
  for (LoopIoBlock = 0; LoopIoBlock < IoBlockList->BlockCount; LoopIoBlock++) {
    Header->Header.Length += sizeof (TemplateIts);
    Header->NumNodes++;
    CopyMem (
        CurrentNode,
        &TemplateIts,
        sizeof (TemplateIts)
        );
    CurrentNode->ItsIdentifiers = IoBlock->HostbridgeId;
    CurrentNode++;
    
    IoBlock = (SGI_PCIE_IO_BLOCK *) ((UINT8 *)IoBlock +
        sizeof (SGI_PCIE_IO_BLOCK) +
        (sizeof (SGI_PCIE_DEVICE) * IoBlock->Count));
  }
}

STATIC
VOID
AddSmmuNodes (
    IN     SGI_PCIE_IO_BLOCK_LIST * CONST IoBlockList,
    IN OUT IORT_GENERATOR_CONTEXT * Context
    )
{
  ARM_EFI_ACPI_6_0_IO_REMAPPING_PCIE_SMMU3_NODE * SmmuBase;
  ARM_EFI_ACPI_6_0_IO_REMAPPING_ITS_NODE * ItsNodes;
  ARM_EFI_ACPI_6_0_IO_REMAPPING_PCIE_SMMU3_NODE * CurrentNode;
  EFI_ACPI_6_0_IO_REMAPPING_TABLE * Header;
  SGI_PCIE_IO_BLOCK *IoBlock;
  UINTN LoopIoBlock;
  UINTN LoopRootPort;
  UINTN NodeSize;
  UINTN ItsOutputReference;
  UINT32 *NumIdMappings;
  UINTN TemplateSize;
 
  Header = Context->Header; 
  SmmuBase = Context->SmmuBase;
  CurrentNode = SmmuBase;
  ItsNodes = Context->ItsBase;
  IoBlock = IoBlockList->IoBlocks;
  TemplateSize =
    sizeof (TemplateSmmu) + \
        (sizeof (EFI_ACPI_6_0_IO_REMAPPING_ID_TABLE) * \
         TemplateSmmu.SmmuNode.Node.NumIdMappings);

  for (LoopIoBlock = 0; LoopIoBlock < IoBlockList->BlockCount; LoopIoBlock++) {
    SGI_PCIE_DEVICE *RootPorts = IoBlock->RootPorts;
    CopyMem (CurrentNode, &TemplateSmmu, TemplateSize);
    NumIdMappings = &CurrentNode->SmmuNode.Node.NumIdMappings;
    CurrentNode->SmmuNode.Base = IoBlock->SmmuBase | IoBlock->Translation;
    EFI_ACPI_6_0_IO_REMAPPING_ID_TABLE * IdMapping = CurrentNode->SmmuIdMap;
    ItsOutputReference = (UINT8 *)&ItsNodes[LoopIoBlock] - (UINT8 *)Context->Header;
    IdMapping[0].OutputReference = ItsOutputReference;

    for (LoopRootPort = 0; LoopRootPort < IoBlock->Count; LoopRootPort++) {
      IdMapping[*NumIdMappings].OutputBase =
        PCI_NUM_IDS(RootPorts[LoopRootPort].Bus.Address) +
        RootPorts[LoopRootPort].BaseInterruptId; 
      IdMapping[*NumIdMappings].InputBase =
        IdMapping[*NumIdMappings].OutputBase;
      IdMapping[*NumIdMappings].NumIds =
        PCI_NUM_IDS(RootPorts[LoopRootPort].Bus.Size) - 1;
      IdMapping[*NumIdMappings].OutputReference = ItsOutputReference;
      *NumIdMappings += 1;
    }

    NodeSize = (UINT8 *)&IdMapping[*NumIdMappings] - (UINT8 *)CurrentNode;

    Header->Header.Length += NodeSize;
    Header->NumNodes++;
    CurrentNode->SmmuNode.Node.Length = NodeSize;

    CurrentNode = 
      (ARM_EFI_ACPI_6_0_IO_REMAPPING_PCIE_SMMU3_NODE *) ((UINT8 *)CurrentNode + NodeSize);
    
    IoBlock = (SGI_PCIE_IO_BLOCK *) ((UINT8 *)IoBlock +
        sizeof (SGI_PCIE_IO_BLOCK) +
        (sizeof (SGI_PCIE_DEVICE) * IoBlock->Count));
  }
}

STATIC
UINTN
GetSmmuNode (
  IN OUT        IORT_GENERATOR_CONTEXT * Context,
  IN UINTN Id
  )
{
  ARM_EFI_ACPI_6_0_IO_REMAPPING_PCIE_SMMU3_NODE *CurrentNode;
  ARM_EFI_ACPI_6_0_IO_REMAPPING_PCIE_SMMU3_NODE *SmmuEnd;
  UINTN Count = 0;
  UINTN NodeSize;

  CurrentNode = Context->SmmuBase;
  SmmuEnd =
    (ARM_EFI_ACPI_6_0_IO_REMAPPING_PCIE_SMMU3_NODE *)
      ((UINT8 *)CurrentNode + Context->SmmuNodesSize);
  while (CurrentNode < SmmuEnd) {
    if (Count == Id) {
      break;
    }
    Count++;

    NodeSize = sizeof(EFI_ACPI_6_0_IO_REMAPPING_SMMU3_NODE) +
      CurrentNode->SmmuNode.Node.NumIdMappings *
      sizeof(EFI_ACPI_6_0_IO_REMAPPING_ID_TABLE);

    CurrentNode = 
      (ARM_EFI_ACPI_6_0_IO_REMAPPING_PCIE_SMMU3_NODE *) ((UINT8 *)CurrentNode + NodeSize);
  }

  ASSERT(CurrentNode < SmmuEnd);
  return (UINTN) ((UINT8 *)CurrentNode - (UINT8 *)Context->Header);
}

STATIC
VOID
AddSegmentMappings (
  IN     UINTN Segment,
  IN OUT UINTN * BlocksProcessed,
  IN OUT IORT_GENERATOR_CONTEXT * Context,
  IN     SGI_PCIE_IO_BLOCK_LIST * CONST IoBlockList,
  IN     ARM_EFI_ACPI_6_0_IO_REMAPPING_RC_NODE *CurrentNode
  )
{
  EFI_ACPI_6_0_IO_REMAPPING_RC_NODE  * RcNode;
  EFI_ACPI_6_0_IO_REMAPPING_ID_TABLE * IdMapping;
  SGI_PCIE_IO_BLOCK *IoBlock;
  UINT32 * NumIdMappings;
  UINTN LoopIoBlock;
  UINTN LoopRootPort;

  RcNode = &CurrentNode->RcNode;
  IdMapping = CurrentNode->RcIdMap;
  IoBlock = IoBlockList->IoBlocks;
  NumIdMappings = &RcNode->Node.NumIdMappings;

  for (LoopIoBlock = 0; LoopIoBlock < IoBlockList->BlockCount; LoopIoBlock++) {
    UINTN SmmuOffset = GetSmmuNode(Context, IoBlock->HostbridgeId);
    if (Segment == IoBlock->Segment) {
      SGI_PCIE_DEVICE *RootPorts = IoBlock->RootPorts;
      for (LoopRootPort = 0; LoopRootPort < IoBlock->Count; LoopRootPort++) {
        IdMapping[*NumIdMappings].InputBase =
          PCI_NUM_IDS(RootPorts[LoopRootPort].Bus.Address);
        IdMapping[*NumIdMappings].NumIds =
          PCI_NUM_IDS(RootPorts[LoopRootPort].Bus.Size) - 1;
        IdMapping[*NumIdMappings].OutputBase =
          PCI_NUM_IDS(RootPorts[LoopRootPort].Bus.Address) +
          RootPorts[LoopRootPort].BaseInterruptId; 
        IdMapping[*NumIdMappings].OutputReference = SmmuOffset;
        *NumIdMappings += 1;
      }
      *BlocksProcessed += 1;
    }

    IoBlock = (SGI_PCIE_IO_BLOCK *) ((UINT8 *)IoBlock +
        sizeof (SGI_PCIE_IO_BLOCK) +
        (sizeof (SGI_PCIE_DEVICE) * IoBlock->Count));
  }
}

STATIC
VOID
AddRcNodes (
 IN     SGI_PCIE_IO_BLOCK_LIST * CONST IoBlockList,
 IN OUT IORT_GENERATOR_CONTEXT * Context
 )
{
  EFI_ACPI_6_0_IO_REMAPPING_TABLE * Header;
  ARM_EFI_ACPI_6_0_IO_REMAPPING_RC_NODE * Nodes;
  EFI_ACPI_6_0_IO_REMAPPING_RC_NODE     * RcNode;
  UINTN BlocksProcessed = 0;
  UINTN Segment = 0;

  Header = Context->Header; 
  Nodes = Context->RcNodeBase;

  while (BlocksProcessed < IoBlockList->BlockCount) {
    AddSegmentMappings (
        Segment,
        &BlocksProcessed,
        Context,
        IoBlockList,
        Nodes);

    RcNode = &Nodes->RcNode;
    RcNode->Node.Type = EFI_ACPI_IORT_TYPE_ROOT_COMPLEX;
    RcNode->Node.Revision = 1;
    RcNode->Node.Identifier = 0;
    RcNode->CacheCoherent = 0;
    RcNode->AllocationHints = 0;
    RcNode->Reserved = 0;
    RcNode->MemoryAccessFlags = 0;
    RcNode->AtsAttribute = EFI_ACPI_IORT_ROOT_COMPLEX_ATS_SUPPORTED;
    RcNode->PciSegmentNumber = Segment;
    RcNode->MemoryAddressSize = 0x30;
    RcNode->Node.IdReference = sizeof (EFI_ACPI_6_0_IO_REMAPPING_RC_NODE);
    RcNode->Node.Length =
      sizeof (EFI_ACPI_6_0_IO_REMAPPING_RC_NODE) +
      (RcNode->Node.NumIdMappings * sizeof (EFI_ACPI_6_0_IO_REMAPPING_ID_TABLE));
    Header->NumNodes++;
    Header->Header.Length += RcNode->Node.Length;
    Segment++;
    Nodes = (VOID *)Nodes + RcNode->Node.Length;

  }
}

STATIC
UINTN
GetTotalNumberRootPorts (
  IN SGI_PCIE_IO_BLOCK_LIST * CONST IoBlockList
  )
{
  SGI_PCIE_IO_BLOCK *IoBlock;
  UINTN NumberRootPorts = 0;
  UINTN LoopIoBlock;

  IoBlock = IoBlockList->IoBlocks;
  for (LoopIoBlock = 0; LoopIoBlock < IoBlockList->BlockCount; LoopIoBlock++) {
    NumberRootPorts += IoBlock->Count;
    IoBlock = (SGI_PCIE_IO_BLOCK *) ((UINT8 *)IoBlock +
        sizeof (SGI_PCIE_IO_BLOCK) +
        (sizeof (SGI_PCIE_DEVICE) * IoBlock->Count));
  }

  return NumberRootPorts;
}

EFI_STATUS
EFIAPI
GenerateContext (
  IN IORT_GENERATOR_CONTEXT * Context,
  IN UINTN NumberHostBridges,
  IN UINTN NumberRootPorts
  )
{
  /* Calculate ITS node size and allocate memory. */
  Context->ItsNodesSize = sizeof (TemplateIts) * NumberHostBridges;
  /* SMMU nodes */
  Context->SmmuNodesSize = sizeof (TemplateSmmu) * NumberHostBridges;
  /* Rempapping nodes */
  Context->SmmuNodesSize +=
    sizeof (EFI_ACPI_6_0_IO_REMAPPING_ID_TABLE)  *
    (NumberRootPorts + (TemplateSmmu.SmmuNode.Node.NumIdMappings * NumberHostBridges));
  /* RC Nodes */
  Context->RcNodesSize =
    NumberHostBridges * sizeof (EFI_ACPI_6_0_IO_REMAPPING_RC_NODE);
  /*Id mapping nodes */
  Context->RcNodesSize +=
    NumberRootPorts * sizeof (EFI_ACPI_6_0_IO_REMAPPING_ID_TABLE);
  Context->MaxTableSize = Context->ItsNodesSize +
                          Context->SmmuNodesSize +
                          Context->RcNodesSize +
                           sizeof(TemplateHeader);
  Context->Header = (EFI_ACPI_6_0_IO_REMAPPING_TABLE *)AllocateZeroPool (Context->MaxTableSize);
  if (Context->Header == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }
  Context->ItsBase =
    (ARM_EFI_ACPI_6_0_IO_REMAPPING_ITS_NODE*)((UINT8*)Context->Header + sizeof(TemplateHeader));
  Context->SmmuBase =
    (ARM_EFI_ACPI_6_0_IO_REMAPPING_PCIE_SMMU3_NODE *)((UINT8 *)Context->ItsBase + Context->ItsNodesSize);
  Context->RcNodeBase =
    (ARM_EFI_ACPI_6_0_IO_REMAPPING_RC_NODE *)((UINT8 *) Context->SmmuBase + Context->SmmuNodesSize);
  
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
GenerateAndInstallIortTable (
    IN SGI_PCIE_IO_BLOCK_LIST * CONST IoBlockList
    )
{
  EFI_STATUS Status;
  EFI_ACPI_TABLE_PROTOCOL * AcpiProtocol;
  IORT_GENERATOR_CONTEXT Context = {0};
  UINTN NumberHostBridges;
  UINTN NumberRootPorts;
  UINTN TableHandle;

  NumberHostBridges = IoBlockList->BlockCount;
  NumberRootPorts = GetTotalNumberRootPorts(IoBlockList);

  Status = GenerateContext (
      &Context,
      NumberHostBridges,
      NumberRootPorts
      );
  if (EFI_ERROR (Status)) {
    DEBUG ((
          DEBUG_ERROR,
          "PCIE IORT: Context generation failed. Status = %r\n",
          Status
          ));

    return Status;
  }

  AddHeader (Context.Header);

  AddItsNodes (IoBlockList, Context.ItsBase, Context.Header);

  AddSmmuNodes (IoBlockList, &Context);

  AddRcNodes (IoBlockList, &Context);

  ASSERT (Context.Header->Header.Length <= Context.MaxTableSize);

  Status = gBS->LocateProtocol (
      &gEfiAcpiTableProtocolGuid,
      NULL,
      (VOID**)&AcpiProtocol
      );

  if (EFI_ERROR (Status)) {
    DEBUG ((
          DEBUG_ERROR,
          "PCIE IORT Table generation failed\n"
          "Failed to locate AcpiProtocol, Status = %r\n",
          Status
          ));
  }


  Status = AcpiProtocol->InstallAcpiTable (
      AcpiProtocol,
      Context.Header,
      Context.Header->Header.Length,
      &TableHandle
      );

  if (EFI_ERROR (Status)) {
    DEBUG ((
          DEBUG_ERROR,
          "PCIE Iort Table generation failed\n"
          "Failed to install Iort table, Status = %r\n",
          Status
          ));
  }

  return EFI_SUCCESS;
}
