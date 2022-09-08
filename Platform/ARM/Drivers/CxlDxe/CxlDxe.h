/** @file
  CXL driver file.

  Defines CXL specific structures and macros.

  Copyright (c) 2022, Arm Limited. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Specification Reference:
    - CXL Specificiation Revision 3.0, Version 0.7
**/

#ifndef CXL_DXE_H_
#define CXL_DXE_H_

#define PCIE_EXTENDED_CAP_OFFSET             0x100
#define PCIE_EXTENDED_CAP_ID_MASK            0xFFFF
#define PCIE_EXTENDED_NEXT_CAP_OFFSET_MASK   0xFFF
#define PCIE_EXTENDED_NEXT_CAP_OFFSET_SHIFT  20

#define PCIE_EXT_CAP_DOE_ID                  0x2E

#define PCIE_EXTENDED_CAP_NEXT(n)  ((n)>>(PCIE_EXTENDED_NEXT_CAP_OFFSET_SHIFT))
#define IS_CXL_DVSEC(n) (((n)&(0xFFFF)) == 0x1E98)

#define DOE_DATA_OBJECT_VID                  0x0000ffff
#define DOE_DATA_OBJECT_TYPE                 0x00ff0000
#define DOE_DATA_OBJECT_LENGTH               0x0003ffff

#define CXL_DOE_TABLE_ENTRY_HANDLE           0xffff0000

#define CXL_DOE_TABLE_ENTRY_HANDLE_LAST      0xffff

#define DVSEC_CXL_VENDOR_ID                  0x1E98

#define DOE_DATA_OBJ_HEADER_1                0x0
#define DOE_DATA_OBJ_HEADER_2                0x4

#define DOE_CAPABILITIES_REG                 0x4
#define DOE_CONTROL_REG                      0x8
#define DOE_STATUS_REG                       0xC
#define DOE_WRITE_DATA_MAILBOX_REG           0x10
#define DOE_READ_DATA_MAILBOX_REG            0x14

#define DOE_STAT_DOE_BUSY                    0x1
#define DOE_STAT_DATA_OBJ_READY              ((0x1) << 31)
#define DOE_CTRL_DOE_GO                      ((0x1) << 31)

#define IS_DOE_SUPPORTED(n)  \
  (((n)&(PCIE_EXTENDED_CAP_ID_MASK))==(PCIE_EXT_CAP_DOE_ID))

typedef enum {
  PCIE_EXT_CAP_HEADER,
  PCIE_DVSEC_HEADER_1,
  PCIE_DVSEC_HEADER_2,
  PCIE_DVSEC_HEADER_MAX
} PCIE_DVSEC_HEADER_ENUM;

/**
  * Data Object Header
  *
  * Data Object Exchange(DOE) Header1 and Header2 put together.
  * Reference taken from PCI-SIG ECN and
  * CXL Specification (Revision 3.0, Version 0.7).
**/
typedef struct {
  UINT16  VendorId;
  UINT8   DataObjType;
  UINT8   Reserved;
  UINT32  Length;
} DOE_HEADER;

#define DOE_DATA_OBJ_TYPE_COMPLIANCE   0x0
#define DOE_DATA_OBJ_TYPE_CDAT         0x2

/**
  * DOE read request data
  *
  * DOE read request data structure. For CXL DOE requests are made
  * to read CDAT tables.
  * Reference taken from CXL Specification (Revision 3.0, Version 0.7).
**/
typedef struct {
  DOE_HEADER  Header;
  UINT8       ReqCode;
  UINT8       TableType;
  UINT16      EntryHandle;
} CXL_CDAT_READ_ENTRY_REQ;

#define CXL_CDAT_DOE_ENTRYHANDLE_LAST_ENTRY  0xFFFF

/* Size in DW(4 Bytes) */
#define CDAT_READ_ENTRY_REQ_SIZE    3

/**
  * DOE read response data
  *
  * DOE read response data structure. For CXL, DOE responses hold
  * information about CDAT tables.
  * Reference taken from CXL Specification (Revision 3.0, Version 0.7).
**/
typedef struct {
  DOE_HEADER  Header;
  UINT8       RspCode;
  UINT8       TableType;
  UINT16      EntryHandle;
  UINT32      CdatTable[32];
} CXL_CDAT_READ_ENTRY_RESP;

/* Size in DW(4 Bytes) */
#define CDAT_READ_ENTRY_RESP_SIZE    3

/**
  * Coherent Device Attribute Table(CDAT) Header
  *
  * CDAT header, which is followed by variable number of CDAT structures.
  * Reference taken from CDAT Specification (Revision 1.02).
**/
typedef struct {
  UINT32  Length;
  UINT8   Revision;
  UINT8   Checksum;
  UINT8   Reserved[6];
  UINT32  Sequence;
} CDAT_TABLE_HEADER;

/* Size in DW(4 Bytes) */
#define CDAT_TABLE_HEADER_SIZE    4

/* Total CDAT table size. It can be increased further. */
#define TOTAL_CDAT_ENTRY          24

/**
  * Device Scoped Memory Affinity Structure (DSMAS)
  *
  * DSMAS returns Device Physical Address(DPA) range and it's attributes.
  * Reference taken from CDAT Specification (Revision 1.02).
**/
typedef struct {
  UINT8   Type;
  UINT8   Reserved_1;
  UINT16  Length;
  UINT8   DsmadHandle;
  UINT8   Flags;
  UINT16  Reserved_2;
  UINT64  DpaBase;
  UINT64  DpaLength;
} CDAT_DSMAS;

/* Size in DW(4 Bytes) */
#define CDAT_STRUCTURE_DSMAS_SIZE   6

typedef enum {
  CDAT_STRUCTURE_DSMAS,
  CDAT_STRUCTURE_DSLBIS,
  CDAT_STRUCTURE_DSMSCIS,
  CDAT_STRUCTURE_DSIS,
  CDAT_STRUCTURE_DSEMTS,
  CDAT_STRUCTURE_SSLBIS,
  CDAT_STRUCTURE_COUNT
} CDAT_STRUCTURE_TYPE;

#endif /* CXL_DXE_H_ */
