.global main
main:

	li s3, 1
	li s1, 101
	xor s2, s2, s2
loop:
	add s2, s2, s3
	addi s3, s3, 1
	blt s3, s1, loop
	
end:
	j end
