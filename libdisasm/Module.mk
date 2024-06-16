libs             += libdisasm

cflags += -w

src_libdisasm      :=  \
	ia32_implicit.c \
	ia32_insn.c \
	ia32_invariant.c \
	ia32_modrm.c \
	ia32_opcode_tables.c \
	ia32_operand.c \
	ia32_reg.c \
	ia32_settings.c \
	x86_disasm.c \
	x86_format.c \
	x86_imm.c \
	x86_insn.c \
	x86_misc.c \
	x86_operand_list.c