# IRIScan Book 5 WiFi — USB protocol

This document describes a proprietary protocol identified by reverse engineering USB traffic exchanged with an IRIScan Book 5 WiFi scanner. It is not a vendor specification. Unknown fields remain unspecified.

## Transport

The device enumerates as USB Mass Storage (`VID_07B4&PID_0109`, model string `MagicScan`). Vendor-specific SCSI CDBs are sent over the Mass Storage transport. Windows supports `IOCTL_SCSI_PASS_THROUGH_DIRECT`; Linux can use `SG_IO` on the corresponding SCSI generic device, subject to permissions.

**All multi-byte offsets and image lengths in C3 CDBs must be reproduced precisely.**

## Device identification

An IN transfer of four bytes with the 16-byte CDB

```text
C5 07 00 00 00 00 00 00 00 04 FF 02 00 00 00 00
```

returns ASCII `NOVA`.

## Image metadata

An established polling sequence is:

```text
A: C5 07 00 00 00 00 00 00 00 34 02 01 01 00 00 00   IN 52 bytes
B: C5 07 00 00 00 00 00 00 00 10 FF 04 00 00 00 00   IN 16 bytes
C: C5 07 00 00 00 00 00 00 00 34 02 02 01 00 00 00   IN 52 bytes
```

In the C response, little-endian values at offsets 0x00, 0x0C, 0x10 and 0x14 indicate image availability, width (2592), strip height (192), and the current image buffer address (for example 0x00758880), respectively. Other fields are not fully characterized. When ready, the reader requests FF04 and FF06 metadata before the pixel transfer.

## Pixel transfer

Vendor CDB `C3` reads memory chunks at the address supplied by the metadata. Example, 65536 bytes from 0x00758880:

```text
C3 07 00 75 88 80 00 01 00 00 00 00 00 00 00 00
```

The starting address occupies CDB bytes 2–5 in big-endian order; the transfer size occupies bytes 6–9 in big-endian order.

A 2592 x 192 RGB strip contains 1,492,992 bytes: 22 reads of 65,536 bytes plus one read of 51,200 bytes, with addresses increasing by each transfer size.

Each image row contains consecutive **R, G and B planes**, each 2592 bytes wide. Reverse each reconstructed row horizontally to obtain the expected orientation. Concatenate strips vertically to build the page.

## Start and end of scanning

A scan-status inquiry has the CDB:

```text
C5 07 00 00 00 00 00 00 00 04 02 02 00 00 00 00
```

The observed status changes `0 -> 1` when scanning begins, and `1 -> 0` after the second SCAN press **when issued within the correct polling context**. Isolated or differently ordered inquiries can instead produce transient values around each strip; thus command sequencing matters.

The proven monitoring order includes FF04, FF03, FF03, FF05, status 02 01, FF04, status 02 02, FF04, FF06. A page reader must additionally perform metadata A–B–C and C3 reads. After detecting scan end, continue draining pending strips before finalization, since an image strip may arrive after the status changes to zero.

Exact meaning of several fields, the optimal post-stop drain condition, and interoperability with other scanner models remain open.

## Safety

Never send undocumented OUT or configuration commands during discovery. Do not modify filesystem contents on the scanner's microSD card. Back up the card before experimental hardware access.
