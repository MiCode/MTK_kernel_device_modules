.data
.pushsection .hyp.reloc, "a"
.global __hyp_section_.hyp.text
.global __hyp_section_.hyp.data
.word 0
.reloc 0, R_AARCH64_PREL32, __hyp_section_.hyp.data + 0x0
.word 0
.reloc 4, R_AARCH64_PREL32, __hyp_section_.hyp.data + 0x8
.word 0
.reloc 8, R_AARCH64_PREL32, __hyp_section_.hyp.data + 0x10
.word 0
.reloc 12, R_AARCH64_PREL32, __hyp_section_.hyp.data + 0x18
.word 0
.reloc 16, R_AARCH64_PREL32, __hyp_section_.hyp.data + 0x20
.word 0
.reloc 20, R_AARCH64_PREL32, __hyp_section_.hyp.data + 0x28
.word 0
.reloc 24, R_AARCH64_PREL32, __hyp_section_.hyp.data + 0x30
.word 0
.reloc 28, R_AARCH64_PREL32, __hyp_section_.hyp.data + 0x38
.word 0
.reloc 32, R_AARCH64_PREL32, __hyp_section_.hyp.data + 0x40
.word 0
.reloc 36, R_AARCH64_PREL32, __hyp_section_.hyp.data + 0x48
.word 0
.reloc 40, R_AARCH64_PREL32, __hyp_section_.hyp.data + 0x50
.popsection
