/*
 * Development-only linker script for QEMU's stm32vldiscovery machine
 * (STM32F100, 8 KB RAM). Do NOT use this for the real STM32F103CB board;
 * the normal build uses STM32F103CB.ld (20 KB RAM).
 */
ENTRY(Reset_Handler)

_estack = 0x20002000;

MEMORY
{
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 128K
  RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 8K
}

SECTIONS
{
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } >FLASH

  .text :
  {
    . = ALIGN(4);
    *(.text)
    *(.text*)
    *(.rodata)
    *(.rodata*)
    . = ALIGN(4);
  } >FLASH

  .init :
  {
    KEEP(*(.init))
  } >FLASH

  .fini :
  {
    KEEP(*(.fini))
  } >FLASH

  .ARM.extab :
  {
    *(.ARM.extab* .gnu.linkonce.armextab.*)
  } >FLASH

  .ARM :
  {
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
  } >FLASH

  _sidata = LOADADDR(.data);

  .data :
  {
    . = ALIGN(4);
    _sdata = .;
    *(.data)
    *(.data*)
    . = ALIGN(4);
    _edata = .;
  } >RAM AT> FLASH

  .bss :
  {
    . = ALIGN(4);
    _sbss = .;
    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
  } >RAM

  ._heap :
  {
    . = ALIGN(8);
    PROVIDE(end = .);
    PROVIDE(_end = .);
  } >RAM
}
