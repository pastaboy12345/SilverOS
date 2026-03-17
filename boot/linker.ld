/* SilverOS Linker Script — x86_64 */

ENTRY(_start)

SECTIONS
{
    . = 1M;  /* Kernel loaded at 1 MB */

    _kernel_start = .;

    .multiboot ALIGN(8) :
    {
        *(.multiboot)
    }

    .text ALIGN(4K) :
    {
        *(.text)
        *(.text.*)
    }

    .rodata ALIGN(4K) :
    {
        *(.rodata)
        *(.rodata.*)
    }

    .init_array ALIGN(8) :
    {
        _init_array_start = .;
        *(.init_array)
        *(.init_array.*)
        _init_array_end = .;
    }

    .data ALIGN(4K) :
    {
        *(.data)
        *(.data.*)
    }

    .bss ALIGN(4K) :
    {
        *(COMMON)
        *(.bss)
        *(.bss.*)
    }

    _kernel_end = .;

    /DISCARD/ :
    {
        *(.comment)
        *(.note.*)
        *(.eh_frame*)
    }
}
