; Authored by finalpatch (GitHub)
;   Source: https://gist.github.com/finalpatch/91ae4449875c3cf217327bfdcbeab0c5
;   Tests: https://github.com/dosbox-staging/dosbox-staging/issues/2247
;
; tasm fpu_test
; tlink fpu_test

.model small
.stack 100h

.data
fval dd 123.1, 223.1, 323.1, 423.1, 523.1, 623.1, 723.1, 823.1
fdst dd 8 dup(0.0)
sok  db "ok$"
sbad db "bad$"

.code
modcode proc near
   ; rewrite the instruction at l2
   lea bp, l2
   ; starting from +1
   ; skipping the fwait added by assembler
   ; first overwrite with NOPs
   ; then rewrite to original instruction
   mov byte ptr es:[bp+1], 90h
   mov byte ptr es:[bp+2], 90h
   mov byte ptr es:[bp+3], 20h
   mov byte ptr es:[bp+4], 90h
   mov byte ptr es:[bp+1], 0d9h ;fstp fdst[bx]
   mov byte ptr es:[bp+2], 9fh
   mov byte ptr es:[bp+3], 20h
   mov byte ptr es:[bp+4], 00h
   ret
modcode endp

main proc near
   mov ax, @data
   mov ds, ax
   ; point es to code seg
   ; so we can modify code va es
   mov ax, cs
   mov es, ax

   ; reset fpu
   finit

   ; fill ST0-ST7
   mov cx, 8
   mov bx, 0
l1:
   fld fval[bx]
   add bx, 4
   loop l1

   ; store ST7-ST0
   mov cx, 8
   mov bx, 28
l2:
   fstp dword ptr fdst[bx]
   ; invalidate the previous instruciton
   ; force it to run on normal core
   call modcode
   sub bx, 4
   loop l2

   ; compare 32 bytes at fval and fdst
   lea si, fval
   lea di, fdst
   mov ax, @data
   mov es, ax
   cld
   mov cx, 32
   repe cmpsb
   je good
bad:
   lea dx, sbad
   jmp print
good:
   lea dx, sok
print:
   mov ah, 09h
   int 21h

   ; reset fpu and quit
   fninit
   mov ah,4ch
   int 21h
main endp
end main
