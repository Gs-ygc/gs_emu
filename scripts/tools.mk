.PHONY:encode test

encode:
	@echo "${instructions}" | llvm-mc -triple=riscv32  --show-encoding | tr -d '\n' | awk -F'[][]' '{print $$2}' | awk -F',' '{gsub("0x","");print "0x"$$4 $$3 $$2 $$1}'

test:
	