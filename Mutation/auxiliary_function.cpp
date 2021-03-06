#include "pch.h"
#include "Mutation_Protecting.h"
#include "auxiliary_function.h"
/*
该cpp的函数都是起辅助作用的函数
*/



//重定位imm和mem(disp)。有重定位时，基地址会赋给base_reg，偏移给offset。无重定位时，直接返回0
BOOL x86Insn_Mutation::RelocData_imm_mem(DWORD DataAddr, IN OUT x86::Gp base_reg, IN OUT UINT* offset)
{
	CA_FixOffset	CAFO_Struct = { 0 };
	x86::Assembler	a(&Mut_Code);
	if (objPE.m_PERelocDir.VirtualAddress)
	{
		//遍历重定位
		for (auto iter = objPE.m_RelocData.begin(); iter != objPE.m_RelocData.end(); iter++) {
			//有重定位
			if (DataAddr == iter->RelocAddr) {
				Label L0 = a.newLabel();
				x86_reg regs[] = { X86_REG_EAX, X86_REG_EBX, X86_REG_ECX, X86_REG_EDX, X86_REG_EBP, X86_REG_ESP, X86_REG_ESI, X86_REG_EDI };
				x86_reg randreg0;
				do {
					randreg0 = regs[rand() % (sizeof(regs) / sizeof(regs[0]))];
				} while (randreg0 == X86_REG_ESP || to_asmjit_reg(randreg0) == base_reg);
				auto rand0 = to_asmjit_reg(randreg0);


				a.pushfd();
				a.push(rand0);
				size_t	Call_CodeSize = Mut_Code.codeSize() + SingMut_Sec.Mut_CodeOffsetAddr;
				a.call(L0);
				size_t	Temp_CodeSize = Mut_Code.codeSize() + SingMut_Sec.Mut_CodeOffsetAddr;
				a.bind(L0);
				a.mov(rand0, 0);
				a.add(rand0, (UINT)objPE.m_dwImageSize);	//-镜像模块大小-自己代码的偏移（相对自己区段）
				size_t	Add_CodeSize = Mut_Code.codeSize() + SingMut_Sec.Mut_CodeOffsetAddr;
				a.add(rand0, (UINT)Temp_CodeSize);
				a.sub(ptr(x86::esp), rand0);
				a.pop(base_reg);							//基地址pop到base_reg

				a.pop(rand0);
				a.popfd();

				CAFO_Struct.Call_Addr = (DWORD)Final_MutMemory + Call_CodeSize;
				CAFO_Struct.Add_Addr = (DWORD)Final_MutMemory + Add_CodeSize;
				CA_Fix_Offset.push_back(CAFO_Struct);

				*offset = iter->Offset;
				return true;
			}
		}
	}
	//无重定位
	return false;

}

BOOL x86Insn_Mutation::DealWithReloc(DWORD DataAddr, DWORD NeedtoReloActuAddr)
{
	/*
	处理可能需要加入重定位表的数据
	返回值：0，无需重定位。1，有重定位
	DataAddr：被分析指令的imm或mem_disp地址
	NeedtoReloActuAddr：需要添加进重定位表的真实地址（imm或mem_disp地址）
	*/

	typedef struct _TYPEOFFSET 
	{
	WORD	offset : 12;						//偏移值
	WORD	Type : 4;							//重定位属性(方式)
	}TYPEOFFSET, *PTYPEOFFSET;
	bool	flag = 0;
	WORD	checked_offset = (WORD)(DataAddr & 0xFFF);
	DWORD	checked_VA = (DataAddr - (DWORD)objPE.m_pFileBuf) & 0xFFFFF000;
	WORD	added_offset = (WORD)(NeedtoReloActuAddr & 0xFFF);
	DWORD	added_VA = (NeedtoReloActuAddr - objPE.m_dwImageBase) & 0xFFFFF000;


	//判断是否有重定位表
	if (objPE.m_PERelocDir.VirtualAddress)
	{
		//1.获取重定位表结构体指针
		PIMAGE_BASE_RELOCATION	pPEReloc =
			(PIMAGE_BASE_RELOCATION)(objPE.m_pFileBuf + objPE.m_PERelocDir.VirtualAddress);
		//2.获取每个重定位块，判断DataAddr是否在重定位表
		while (pPEReloc->VirtualAddress)
		{
			//3.如果在当前重定位块
			if (pPEReloc->VirtualAddress == checked_VA) {
				PTYPEOFFSET pTypeOffset = (PTYPEOFFSET)(pPEReloc + 1);
				DWORD dwNumber = (pPEReloc->SizeOfBlock - 8) / 2;
				for (DWORD i = 0; i < dwNumber; i++)
				{
					//block为0
					if (*(PWORD)(&pTypeOffset[i]) == NULL)
						continue;
					//block相等，DataAddr在重定位表中，对其抹0
					if (checked_offset == pTypeOffset[i].offset && pTypeOffset[i].Type == 3) {
						flag = 1;
						*(PWORD)(&pTypeOffset[i]) = NULL;
						break;
					}
				}
				if (flag)
					break;
			}
			//2.1下一个区段
			pPEReloc = (PIMAGE_BASE_RELOCATION)((DWORD)pPEReloc + pPEReloc->SizeOfBlock);
		}
	}
	//存在重定位表中
	if (flag)
	{
		objPE.Add_DataToRelocDir(added_offset, added_VA);
	}

	return flag;
}
BOOL x86Insn_Mutation_again::DealWithReloc(DWORD DataAddr, DWORD NeedtoReloActuAddr)
{
	typedef struct _TYPEOFFSET
	{
		WORD	offset : 12;						//偏移值
		WORD	Type : 4;							//重定位属性(方式)
	}TYPEOFFSET, *PTYPEOFFSET;
	bool	flag = 0;
	WORD	checked_offset = (WORD)(DataAddr & 0xFFF);
	DWORD	checked_VA = (Jcc_ActuAddr(DataAddr) - objPE.m_dwImageBase) & 0xFFFFF000;
	WORD	added_offset = (WORD)(NeedtoReloActuAddr & 0xFFF);
	DWORD	added_VA = (NeedtoReloActuAddr - objPE.m_dwImageBase) & 0xFFFFF000;


	//判断是否有重定位表
	if (objPE.m_PERelocDir.VirtualAddress)
	{
		//1.获取重定位表结构体指针
		PIMAGE_BASE_RELOCATION	pPEReloc =
			(PIMAGE_BASE_RELOCATION)(objPE.m_pFileBuf + objPE.m_PERelocDir.VirtualAddress);
		//2.获取每个重定位块，判断DataAddr是否在重定位表
		while (pPEReloc->VirtualAddress)
		{
			//3.如果在当前重定位块
			if (pPEReloc->VirtualAddress == checked_VA) {
				PTYPEOFFSET pTypeOffset = (PTYPEOFFSET)(pPEReloc + 1);
				DWORD dwNumber = (pPEReloc->SizeOfBlock - 8) / 2;
				if (dwNumber != 1) {
					throw "上一次变异添加的 重定位结构体 错误";
				}
				for (DWORD i = 0; i < dwNumber; i++)
				{
					//block为0
					if (*(PWORD)(&pTypeOffset[i]) == NULL)
						continue;
					//block相等，DataAddr在重定位表中。
					//由于是二次变异，可以直接更新上一次变异时加入的结构体的 VA 和 offset
					if (checked_offset == pTypeOffset[i].offset && pTypeOffset[i].Type == 3) {
						flag = 1;
						pPEReloc->VirtualAddress = added_VA;
						*(PWORD)(&pTypeOffset[i]) = 0x3000 + added_offset;
						break;
					}
				}
				if (flag)
					break;
			}
			//2.1下一个区段
			pPEReloc = (PIMAGE_BASE_RELOCATION)((DWORD)pPEReloc + pPEReloc->SizeOfBlock);
		}
	}
	
	return flag;
}

//判断并修复jmp跳转
UINT x86Insn_Mutation::Fix_JmpOffset()
{
	UINT result = 0;
	DWORD jcc_offset = 0;
	DWORD jcc_addr = 0;
	uint8_t imm_offset = 0;
	
	//以当前指令地址为key，从map中查找是否有jcc/jmp需要跳转到当前指令地址
	 auto search = Fix_Offset.find(insn.address);
	 if (search != Fix_Offset.end()) {
		 result = 1;
		 for (auto& c : search->second) {
			 jcc_addr = c.address;
			 imm_offset = c.imm_offset;
			 //公式： jcc_addr + imm_offset + imm_size + jcc_offset = target_addr(SingMut_Sec.Mut_CodeStartAddr)
			 jcc_offset = SingMut_Sec.Mut_CodeStartAddr - imm_offset - 4 - jcc_addr;
			 //写入jcc/jmp的offset
			 memcpy_s((void*)(jcc_addr + imm_offset), 4, &jcc_offset, 4);
		}
	 }
	
	return result;
}

//转换jcc目标跳转地址为实际地址
UINT x86Insn_Mutation::Jcc_ActuAddr(DWORD Target_JumpAddr)
{
	return(Target_JumpAddr - (DWORD)objPE.m_pFileBuf + objPE.m_dwImageBase);
}
UINT x86Insn_Mutation_again::Jcc_ActuAddr(DWORD Target_JumpAddr)
{
	return(Target_JumpAddr - (DWORD)old_Final_MutMemory + (objPE.m_dwImageBase + objPE.m_dwImageSize));
}
//检查是否是那7个常用的寄存器（esp被排除了）。
bool Check_Reg(x86_reg capstone_reg)
{
	switch (capstone_reg) {
	case X86_REG_EAX:
		return true;
		break;
	case X86_REG_EBX:
		return true;
		break;
	case X86_REG_ECX:
		return true;
		break;
	case X86_REG_EDX:
		return true;
		break;
	case X86_REG_EBP:
		return true;
		break;
	case X86_REG_ESI:
		return true;
		break;
	case X86_REG_EDI:
		return true;
		break;
	default:
		return false;
		break;
	}
	return false;
}
//检查是否是段寄存器
bool Check_SReg(x86_reg capstone_reg)
{
	switch (capstone_reg) {
	case X86_REG_GS:
		return true;
		break;
	case X86_REG_FS:
		return true;
		break;
	case X86_REG_ES:
		return true;
		break;
	case X86_REG_DS:
		return true;
		break;
	case X86_REG_CS:
		return true;
		break;
	case X86_REG_SS:
		return true;
		break;
	default:
		return false;
		break;
	}
}
//用于取随机数时检查
x86_reg Low_reg_Check(x86_reg capstone_reg)
{
	if (capstone_reg == X86_REG_INVALID)
		return X86_REG_INVALID;

	switch (capstone_reg) {
	case X86_REG_AX: return X86_REG_EAX;	case X86_REG_AH: return X86_REG_EAX;	case X86_REG_AL: return X86_REG_EAX;
	case X86_REG_BX: return X86_REG_EBX;	case X86_REG_BH: return X86_REG_EBX;	case X86_REG_BL: return X86_REG_EBX;
	case X86_REG_CX: return X86_REG_ECX;	case X86_REG_CH: return X86_REG_ECX;	case X86_REG_CL: return X86_REG_ECX;
	case X86_REG_DX: return X86_REG_EDX;	case X86_REG_DH: return X86_REG_EDX;	case X86_REG_DL: return X86_REG_EDX;
	case X86_REG_BP: return X86_REG_EBP;
	case X86_REG_SP: return X86_REG_ESP;
	case X86_REG_SI: return X86_REG_ESI;
	case X86_REG_DI: return X86_REG_EDI;
	default:
		throw "没有相应寄存器";
		break;
	}

	return X86_REG_INVALID;
}
//从内存中寻找指定字符串
LPBYTE Find_MemoryString(LPBYTE pFinalBuf, DWORD size, const LPBYTE String)
{
	LPBYTE Addr = 0;
	for (DWORD j = 0; size - j > 0; j = Addr - pFinalBuf + 1)
	{
		Addr = (LPBYTE)memchr(pFinalBuf + j, String[0], size - j);
		if (Addr == NULL)
			return NULL;
		for (DWORD i = 0; i < strlen((char*)String); i++)
		{
			if (Addr[i] != String[i])
				break;
			if (i == strlen((char*)String) - 1)
				return Addr;
		}
	}
	return NULL;
}
//capstone转asmjit
x86::Gp to_asmjit_reg(x86_reg capstone_reg)
{
	switch (capstone_reg) {
	case X86_REG_EAX:		return x86::eax;		break;
	case X86_REG_EBX:		return x86::ebx;		break;
	case X86_REG_ECX:		return x86::ecx;		break;
	case X86_REG_EDX:		return x86::edx;		break;
	case X86_REG_EBP:		return x86::ebp;		break;
	case X86_REG_ESP:		return x86::esp;		break;
	case X86_REG_ESI:		return x86::esi;		break;
	case X86_REG_EDI:		return x86::edi;		break;
	case X86_REG_AX:		return x86::ax;			break;
	case X86_REG_BX:		return x86::bx;			break;
	case X86_REG_CX:		return x86::cx;			break;
	case X86_REG_DX:		return x86::dx;			break;
	case X86_REG_BP:		return x86::bp;			break;
	case X86_REG_SP:		return x86::sp;			break;
	case X86_REG_SI:		return x86::si;			break;
	case X86_REG_DI:		return x86::di;			break;
	case X86_REG_AH:		return x86::ah;			break;
	case X86_REG_AL:		return x86::al;			break;
	case X86_REG_BH:		return x86::bh;			break;
	case X86_REG_BL:		return x86::bl;			break;
	case X86_REG_CH:		return x86::ch;			break;
	case X86_REG_CL:		return x86::cl;			break;
	case X86_REG_DH:		return x86::dh;			break;
	case X86_REG_DL:		return x86::dl;			break;
	default:
		throw "没有相应寄存器";
		break;
	}
}
//capstone段寄存器 转asmjit段寄存器
x86::SReg to_asmjit_sreg(x86_reg capstone_reg)
{
	switch (capstone_reg) {
	case X86_REG_GS:		return x86::gs;			break;
	case X86_REG_FS:		return x86::fs;			break;
	case X86_REG_ES:		return x86::es;			break;
	case X86_REG_DS:		return x86::ds;			break;
	case X86_REG_CS:		return x86::cs;			break;
	case X86_REG_SS:		return x86::ss;			break;
	default:
		throw "没有相应寄存器";
		break;
	}
}
//运用低位reg
x86::Gp Low_reg(x86_reg capstone_reg, UINT Low_reg)
{
	switch (capstone_reg)
	{
	case X86_REG_EAX:
		switch (Low_reg)
		{
		case ax:
			return x86::ax;
			break;
		case ah:
			return x86::ah;
			break;
		case al:
			return x86::al;
			break;
		}
		break;
	case X86_REG_EBX:
		switch (Low_reg)
		{
		case ax:
			return x86::bx;
			break;
		case ah:
			return x86::bh;
			break;
		case al:
			return x86::bl;
			break;
		}
		break;
	case X86_REG_ECX:
		switch (Low_reg)
		{
		case ax:
			return x86::cx;
			break;
		case ah:
			return x86::ch;
			break;
		case al:
			return x86::cl;
			break;
		}
		break;
	case X86_REG_EDX:
		switch (Low_reg)
		{
		case ax:
			return x86::dx;
			break;
		case ah:
			return x86::dh;
			break;
		case al:
			return x86::dl;
			break;
		}
		break;
	case X86_REG_EBP:
		switch (Low_reg)
		{
		case ax:
			return x86::bp;
			break;
		default:
			throw "没有8位寄存器";
			break;
		}
		break;
	case X86_REG_ESP:
		switch (Low_reg)
		{
		case ax:
			return x86::sp;
			break;
		default:
			throw "没有8位寄存器";
			break;
		}
		break;
	case X86_REG_ESI:
		switch (Low_reg)
		{
		case ax:
			return x86::si;
			break;
		default:
			throw "没有8位寄存器";
			break;
		}
		break;
	case X86_REG_EDI:
		switch (Low_reg)
		{
		case ax:
			return x86::di;
			break;
		default:
			throw "没有8位寄存器";
			break;
		}
		break;
	default:
		throw "没有相应寄存器";
		break;
	}

}
//运用低位reg
x86_reg Low_reg_2(x86_reg capstone_reg, UINT Low_reg)
{
	switch (capstone_reg)
	{
	case X86_REG_EAX:
		switch (Low_reg)
		{
		case ax:
			return X86_REG_AX;
			break;
		case ah:
			return X86_REG_AH;
			break;
		case al:
			return X86_REG_AL;
			break;
		}
		break;
	case X86_REG_EBX:
		switch (Low_reg)
		{
		case ax:
			return X86_REG_BX;
			break;
		case ah:
			return X86_REG_BH;
			break;
		case al:
			return X86_REG_BL;
			break;
		}
		break;
	case X86_REG_ECX:
		switch (Low_reg)
		{
		case ax:
			return X86_REG_CX;
			break;
		case ah:
			return X86_REG_CH;
			break;
		case al:
			return X86_REG_CL;
			break;
		}
		break;
	case X86_REG_EDX:
		switch (Low_reg)
		{
		case ax:
			return X86_REG_DX;
			break;
		case ah:
			return X86_REG_DH;
			break;
		case al:
			return X86_REG_DL;
			break;
		}
		break;
	case X86_REG_EBP:
		switch (Low_reg)
		{
		case ax:
			return X86_REG_BP;
			break;
		default:
			throw "没有8位寄存器";
			break;
		}
		break;
	case X86_REG_ESP:
		switch (Low_reg)
		{
		case ax:
			return X86_REG_SP;
			break;
		default:
			throw "没有8位寄存器";
			break;
		}
		break;
	case X86_REG_ESI:
		switch (Low_reg)
		{
		case ax:
			return X86_REG_SI;
			break;
		default:
			throw "没有8位寄存器";
			break;
		}
		break;
	case X86_REG_EDI:
		switch (Low_reg)
		{
		case ax:
			return X86_REG_DI;
			break;
		default:
			throw "没有8位寄存器";
			break;
		}
		break;
	default:
		throw "没有相应寄存器";
		break;
	}

}