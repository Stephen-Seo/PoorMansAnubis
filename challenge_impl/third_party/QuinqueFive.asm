.section .data

.global _binary_QuinqueFive_ttf_start
.global _binary_QuinqueFive_ttf_end

_binary_QuinqueFive_ttf_start:
.incbin "QuinqueFive.ttf"
_binary_QuinqueFive_ttf_end:
.word 0x00
