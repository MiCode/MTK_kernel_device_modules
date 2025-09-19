
SECTIONS {
 .hyp.text : {
  __hyp_section_.hyp.text = .;
  *(.text .text.*)
 }
 .hyp.bss : {
  __hyp_section_.hyp.bss = .;
  *(.bss .bss.*)
 }
 .hyp.rodata : {
  __hyp_section_.hyp.rodata = .;
  *(.rodata .rodata.*)
 }
 .hyp.event_ids : {
  __hyp_section_.hyp.event_ids = .;
  __hyp_event_ids_start = .;
  *(.hyp.event_ids)
  __hyp_event_ids_end = .;
 }
 .hyp.data : {
  __hyp_section_.hyp.data = .;
  *(.data .data.*)
 }
}
