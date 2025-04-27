.section ".text"
.global _start
_start:
    bl fork_test
1: 
    b 1b