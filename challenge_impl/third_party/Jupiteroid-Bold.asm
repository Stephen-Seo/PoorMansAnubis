.section .data

.global _binary_Jupiteroid_ttf_start
.global _binary_Jupiteroid_ttf_end

_binary_Jupiteroid_ttf_start:
.incbin "Jupiteroid-Bold.ttf"
_binary_Jupiteroid_ttf_end:
.word 0x00
