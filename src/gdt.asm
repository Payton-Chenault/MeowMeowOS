; Access Byte Reference:
; b7 -> P, Present Bit. Allows an entry to refer to a valid segment (Must be set to 1 for a valid segment)
; b6-5 -> DPL, Descriptor Privilege Level. Contains CPU Privilege Level of the segment (0 Being kernel level, 3 being user applications)
; b4 -> S, Descriptor. If 0, segment is a system segment, if 1, defines a code or data segment
; b3 -> E, Executible. If 0, segment is a data segment, if 1, defines a code segment in which it can be executed.
; b2 -> DC, Direction/Conforming. {(1) (For Data Selectors: Direction Bit) if 0. the segment grows up, if 1 the segment grows down} {(2) (For Code Selectors: Conforming Bit) if 0, code can only be executed in the privillege ring mentioned in DPL, if 1, code can be executed from an equal or lower priveledge level (Again, as described in DPL)}
; b1 -> RW, Readable Writable. {(1) For Data Segments if 0, write access is not allowed. If 1, write access is allowed !!NOTE: DATA SEGMENTS ALWAYS HAVE READ ACCESS!!} {(2) For Code Segments, if 0, Read access is not allowed, if 1, read access is allowed !!NOTE: CODE SEGMENTS NEVER HAVE WRITE ACCESS!!}
; b0 -> A, Accessed. CPU will set it unless set to one in advance. Best to leave this as 1, as CPU will trigger a page fault if GDT is stored in a read only page.



; Flag Byte Reference:
; b3 -> G, Granularity Flag. Indicates the size the limit value is scaled by. If 0, the limit is in 1 byte blocks. If 1, the limit is in 4 kiB blocks
; b2 -> DB, Size Flag. If 0, the descriptor defines a 16 bit protected mode segment, if 1, it sefines a 32 bit protected mode segment
; b1 -> L, Long-Mode Code Flag. If 1, Descriptor defines a 64-bit code segment. When set, DB should always be clear. For any other code types (or data segments), this should be set to 0
; b0 -> Reserved, do not use.

gdt_start:
    dd 0x0
    dd 0x0

    ; Code Segment Descriptor
    dw 0xFFFF               ; Limit
    dw 0x0000               ; Base
    db 0x0000               ; Base 2 

    db 10011010b            ; Access Byte for CODE SEGMENT DESCRIPTION
    db 11001111b            ; Flags
    db 0x00                 ; Base

    ; Data Segment Descriptor
    dw 0xFFFF               ; Limit
    dw 0x0000               ; Base
    db 0x0000               ; Base 2 

    db 10010010b            ; Access Byte for DATA SEGMENT DESCRIPTION
    db 11001111b            ; Flags
    db 0x00                 ; Base


gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start