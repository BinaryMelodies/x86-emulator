
//// Memory and I/O management, including paging and debugging breakpoints

static inline void x86_check_canonical_address(x86_state_t * emu, x86_segnum_t segment_number, uaddr_t address, uoff_t error_code)
{
	if(x86_is_64bit_mode(emu))
	{
		uaddr_t mask;
		if((emu->cr[4] & X86_CR4_VA57) == 0)
		{
			mask = 0xFFFF000000000000;
		}
		else
		{
			mask = 0xFE00000000000000;
		}
		if((address & mask) != 0 && (address & mask) != mask)
		{
			if(segment_number == X86_R_SS)
				x86_trigger_interrupt(emu, X86_EXC_SS | X86_EXC_FAULT | X86_EXC_VALUE, error_code);
			else
				x86_trigger_interrupt(emu, X86_EXC_GP | X86_EXC_FAULT | X86_EXC_VALUE, error_code);
		}
	}
}

/*
	On the x86, most instructions provide a segmented address that consists of a 16-bit segment value and a 16/32/64-bit offset.
	This value gets converted to a linear address by adding the offset to a base value.
	If the CPU supports paging, this linear address (called a virtual address) is then converted to a physical address.

	Different CPUs have different sizes for these values.
	The 8086 had 16-bit segmented addresses (for offsets) and a 20-bit physical memory.
	The V33 offered an early form of paging with a 24-bit physical memory.
	The V55 and 286 expanded the linear address space to 24 bits and the 386 to 32 bits.
	PAE (Physical Address Extension) was introduced in the P6 (Pentium Pro) and AMD K7 (Athlon).
	This extended the physical address space to 36 bits.
	Finally, the introduction of x86-64 extended both linear and physical addresses to 64 bits.
	The following table gives a quick overview with a typical slicing of the bits of a linear address for paging:

	           8086    V33     V55     286     386        PAE        x64
	segment    16:16   16:16   16:16   16:16   16:32      16:32      16:64
	linear     20      20      24      24      32         32         64
	pages      -       6+14    -       -       10+10+12   10+10+12   9+9+9+9+12
	physical   20      24      24      24      32         36         64
*/

static inline uaddr_t x86_memory_segmented_to_linear(x86_state_t * emu, x86_segnum_t segment_number, uoff_t offset)
{
	if(x86_is_64bit_mode(emu))
	{
		uaddr_t address;
		if(segment_number < X86_R_FS)
		{
			address = offset;
		}
		else
		{
			address = emu->sr[segment_number].base + offset;
		}
		x86_check_canonical_address(emu, segment_number, address, 0);
		return address;
	}
	else
	{
		return emu->sr[segment_number].base + offset;
	}
}

// Reports linear memory size
// Note that this is the value used after applying segmentation but not paging
// Therefore V33 uses 20-bit (instead of 24-bit) and PAE is ignored (so 32-bit instead of 36-bit)
static inline uaddr_t x86_get_memory_mask(x86_state_t * emu)
{
	if(emu->cpu_type < X86_CPU_V55 || emu->cpu_type == X86_CPU_186)
		// 20 bits
		return 0x000FFFFF;
	else if(emu->cpu_type < X86_CPU_386)
		// 24 bits
		return 0x00FFFFFF;
	else if(!x86_is_long_mode_supported(emu))
		// 32 bits
		return 0xFFFFFFFF;
	else
		// 64 bits
		return 0xFFFFFFFFFFFFFFFF; // We allow full 64-bit, ignoring canonical addresses
}

// This access ignores the V25 internal memory, which happens during instruction fetch
void x86_memory_read_external(x86_state_t * emu, uaddr_t address, uaddr_t count, void * buffer)
{
	x86_cpu_level_t memory_space = emu->parser->user_mode ? X86_LEVEL_USER : emu->cpu_level;
	if(emu->cpu_type == X86_CPU_186 && (le16toh(emu->pcb[X86_PCB_PCR]) & X86_PCB_PCR_MIO) == 0)
	{
		// The 80186 checks for its internal registers
		uaddr_t actual_count;
		uaddr_t pcb_address = (uaddr_t)(le16toh(emu->pcb[X86_PCB_PCR]) & X86_PCB_PCR_ADDRESS) << 8;
		if(address < pcb_address)
		{
			actual_count = min(count, pcb_address - address);
			if(emu->memory_read != NULL)
				emu->memory_read(emu, memory_space, address, buffer, actual_count);
			if(actual_count == count)
				return;
			address = pcb_address;
			buffer = (char *)buffer + actual_count;
			count -= actual_count;
		}

		if(address < pcb_address + 0x100)
		{
			actual_count = min(count, 0x100);
			memcpy(buffer, (uint8_t *)emu->pcb + (address - pcb_address), actual_count);
			if(actual_count == count)
				return;
			address += actual_count;
			buffer = (char *)buffer + actual_count;
			count -= actual_count;
		}
	}
	if(emu->memory_read != NULL)
		emu->memory_read(emu, memory_space, address, buffer, count);
}

void x86_memory_write_external(x86_state_t * emu, uaddr_t address, uaddr_t count, const void * buffer)
{
	x86_cpu_level_t memory_space = emu->parser->user_mode ? X86_LEVEL_USER : emu->cpu_level;
	if(emu->cpu_type == X86_CPU_186 && (le16toh(emu->pcb[X86_PCB_PCR]) & X86_PCB_PCR_MIO) == 0)
	{
		// The 80186 checks for its internal registers
		uaddr_t actual_count;
		uaddr_t pcb_address = (uaddr_t)(le16toh(emu->pcb[X86_PCB_PCR]) & X86_PCB_PCR_ADDRESS) << 8;
		if(address < pcb_address)
		{
			actual_count = min(count, pcb_address - address);
			if(emu->memory_write != NULL)
				emu->memory_write(emu, memory_space, address, buffer, actual_count);
			if(actual_count == count)
				return;
			address = pcb_address;
			buffer = (char *)buffer + actual_count;
			count -= actual_count;
		}

		if(address < pcb_address + 0x100)
		{
			actual_count = min(count, 0x100);
			memcpy((uint8_t *)emu->pcb + (address - pcb_address), buffer, actual_count);
			if(actual_count == count)
				return;
			address += actual_count;
			buffer = (char *)buffer + actual_count;
			count -= actual_count;
		}
	}
	if(emu->memory_write != NULL)
		emu->memory_write(emu, memory_space, address, buffer, count);
}

// Memory access without paging, typically the same as external memory (only required for V25 which uses on-chip RAM)
static inline void x86_memory_read_no_paging(x86_state_t * emu, uaddr_t address, uaddr_t count, void * buffer)
{
	if(emu->cpu_type == X86_CPU_V25)
	{
		char * _buffer = buffer;
		uaddr_t idb = (uaddr_t)emu->iram[X86_SFR_IDB] << 12;
		uaddr_t mask = x86_get_memory_mask(emu);

		// access the memory below the internal data area
		size_t actual_count;
		address &= mask;
		if((emu->iram[X86_SFR_PRC] & X86_PRC_RAMEN) != 0)
		{
			if(address < idb + 0xE00)
			{
				actual_count = min(count, idb + 0xE00 - address);
				x86_memory_read_external(emu, address, actual_count, _buffer);
				if(actual_count == count)
					return;
				_buffer += actual_count;
				address += actual_count;
				count -= actual_count;
			}
			x86_store_register_bank(emu);
		}
		else
		{
			if(address < idb + 0xF00)
			{
				actual_count = min(count, idb + 0xF00 - address);
				x86_memory_read_external(emu, address, actual_count, _buffer);
				if(actual_count == count)
					return;
				_buffer += actual_count;
				address += actual_count;
				count -= actual_count;
			}
		}

		// access the memory in the internal data area
		if(address < idb + 0x1000)
		{
			actual_count = min(count, idb + 0x1000 - address);
			memcpy(_buffer, &emu->iram[address - (0xE00 + idb)], actual_count);
			if(actual_count == count)
				return;
			_buffer += actual_count;
			address += actual_count;
			count -= actual_count;
		}

		// access the memory above the internal data area
		if(address < 0xFFFFF)
		{
			actual_count = min(count, 0xFFFFF - address);
			x86_memory_read_external(emu, address, actual_count, _buffer);
			if(actual_count == count)
				return;
			_buffer += actual_count;
			address += actual_count;
			count -= actual_count;
		}

		// access the IDB register
		if(address < 0x100000)
		{
			actual_count = min(count, 0x10000);
			memcpy(_buffer, &emu->iram[X86_SFR_IDB], actual_count);
		}
	}
	else
	{
		x86_memory_read_external(emu, address, count, buffer);
	}
}

static inline void x86_memory_write_no_paging(x86_state_t * emu, uaddr_t address, uaddr_t count, const void * buffer)
{
	if(emu->cpu_type == X86_CPU_V25)
	{
		const char * _buffer = buffer;
		uaddr_t idb = (uaddr_t)emu->iram[X86_SFR_IDB] << 12;
		uaddr_t mask = x86_get_memory_mask(emu);

		// access the memory below the internal data area
		size_t actual_count;
		address &= mask;
		if((emu->iram[X86_SFR_PRC] & X86_PRC_RAMEN) != 0)
		{
			if(address < idb + 0xE00)
			{
				actual_count = min(count, idb + 0xE00 - address);
				x86_memory_write_external(emu, address, actual_count, _buffer);
				if(actual_count == count)
					return;
				_buffer += actual_count;
				address += actual_count;
				count -= actual_count;
			}
			x86_store_register_bank(emu);
		}
		else
		{
			if(address < idb + 0xF00)
			{
				actual_count = min(count, idb + 0xF00 - address);
				x86_memory_write_external(emu, address, actual_count, _buffer);
				if(actual_count == count)
					return;
				_buffer += actual_count;
				address += actual_count;
				count -= actual_count;
			}
		}

		// access the memory in the internal data area
		if(address < idb + 0x1000)
		{
			actual_count = min(count, idb + 0x1000 - address);
			memcpy(&emu->iram[address - (0xE00 + idb)], _buffer, actual_count);
			if(address < idb + 0xF00)
			{
				x86_load_register_bank(emu);
			}
			if(actual_count == count)
				return;
			_buffer += actual_count;
			address += actual_count;
			count -= actual_count;
		}

		// access the memory above the internal data area
		if(address < 0xFFFFF)
		{
			actual_count = min(count, 0xFFFFF - address);
			x86_memory_write_external(emu, address, actual_count, _buffer);
			if(actual_count == count)
				return;
			_buffer += actual_count;
			address += actual_count;
			count -= actual_count;
		}

		// access the IDB register
		if(address < 0x100000)
		{
			actual_count = min(count, 0x100000 - address);
			memcpy(&emu->iram[X86_SFR_IDB], _buffer, actual_count);
		}
	}
	else
	{
		x86_memory_write_external(emu, address, count, buffer);
	}
}

static inline uint32_t x86_page_fetch32(x86_state_t * emu, uaddr_t full_address, uaddr_t table_address, uoff_t index, bool write, bool exec, bool user)
{
	uint32_t entry = x86_memory_read32_external(emu, table_address + index * 4);
	// TODO: check other flags and when they were introduced
	uint32_t error_code = (write ? X86_EXC_VALUE_PF_WR : 0) | (user ? X86_EXC_VALUE_PF_US : 0) | (exec ? X86_EXC_VALUE_PF_ID : 0);
	if((entry & X86_PAGE_ENTRY_P) == 0)
	{
		emu->cr[2] = full_address;
		x86_trigger_interrupt(emu, X86_EXC_PF | X86_EXC_FAULT | X86_EXC_VALUE, error_code | X86_EXC_VALUE_PF_P);
	}
	if(write && (entry & X86_PAGE_ENTRY_WR) == 0 && (user || (emu->cr[0] & X86_CR0_WP) != 0))
	{
		emu->cr[2] = full_address;
		x86_trigger_interrupt(emu, X86_EXC_PF | X86_EXC_FAULT | X86_EXC_VALUE, error_code);
	}
	if(user && (entry & X86_PAGE_ENTRY_US) == 0)
	{
		emu->cr[2] = full_address;
		x86_trigger_interrupt(emu, X86_EXC_PF | X86_EXC_FAULT | X86_EXC_VALUE, error_code);
	}
	if((entry & X86_PAGE_ENTRY_A) == 0 || (write && (entry & X86_PAGE_ENTRY_D) == 0))
	{
		entry |= X86_PAGE_ENTRY_A;
		if(write)
			entry |= X86_PAGE_ENTRY_D;
		x86_memory_write8_external(emu, table_address + index * 4, entry);
	}
	return entry;
}

static inline uint64_t x86_page_fetch64(x86_state_t * emu, uaddr_t full_address, uaddr_t table_address, uoff_t index, bool write, bool exec, bool user)
{
	uint64_t entry = x86_memory_read64_external(emu, table_address + index * 4);
	// TODO: check other flags and when they were introduced
	uint32_t error_code = (write ? X86_EXC_VALUE_PF_WR : 0) | (user ? X86_EXC_VALUE_PF_US : 0) | (exec ? X86_EXC_VALUE_PF_ID : 0);
	if((entry & X86_PAGE_ENTRY_P) == 0)
	{
		emu->cr[2] = full_address;
		x86_trigger_interrupt(emu, X86_EXC_PF | X86_EXC_FAULT | X86_EXC_VALUE, error_code | X86_EXC_VALUE_PF_P);
	}
	if(write && (entry & X86_PAGE_ENTRY_WR) == 0 && (user || (emu->cr[0] & X86_CR0_WP) != 0))
	{
		emu->cr[2] = full_address;
		x86_trigger_interrupt(emu, X86_EXC_PF | X86_EXC_FAULT | X86_EXC_VALUE, error_code);
	}
	if(exec && (emu->efer & X86_EFER_NXE) != 0 && (entry & X86_PAGE_ENTRY_XD) != 0)
	{
		emu->cr[2] = full_address;
		x86_trigger_interrupt(emu, X86_EXC_PF | X86_EXC_FAULT | X86_EXC_VALUE, error_code);
	}
	if((entry & X86_PAGE_ENTRY_A) == 0 || (write && (entry & X86_PAGE_ENTRY_D) == 0))
	{
		entry |= X86_PAGE_ENTRY_A;
		if(write)
			entry |= X86_PAGE_ENTRY_D;
		x86_memory_write8_external(emu, table_address + index * 8, entry);
	}
	return entry;
}

/* Translates the virtual (linear) address into physical address and stores the number of bytes beyond the address that belong to the same page */
static inline uaddr_t x86_page_translate(x86_state_t * emu, uaddr_t full_address, bool write, bool exec, bool user, uoff_t * length)
{
	uaddr_t address = full_address;
	if(emu->cpu_type == X86_CPU_V33)
	{
		if((emu->v33_xam & X86_XAM_XA) != 0)
		{
			uint16_t page = le16toh(emu->v33_pgr[(address >> 14) & 0x3F]) & 0x3FF;
			address &= 0x3FFF;
			*length = 0x4000 - address;
			return ((uint32_t)page << 14) + (address & 0x3FFF);
		}
		else
		{
			// masking is done for other CPUs in the direct functions, but not the V33
			address &= 0xFFFFF;
			*length = 0x100000 - address;
			return address;
		}
	}
	else if((emu->cr[0] == X86_CR0_PG) == 0)
	{
		/* paging disabled */
		address &= x86_get_memory_mask(emu);
		*length = x86_get_memory_mask(emu) - address + 1;
		return address;
	}
	else if((emu->efer & X86_EFER_LMA) == 0)
	{
		if((emu->cr[4] & X86_CR4_PAE) != 0)
		{
			// 36-bit paging
			// TODO: other CR3 fields?
			uint64_t pml3 = x86_page_fetch64(emu, full_address, emu->cr[3] & ~0xFFF, (address >> 30) & 3, write, exec, user);
			uint64_t pml2 = x86_page_fetch64(emu, full_address, pml3 & 0x000FFFFFFFFFF000LL, (address >> 21) & 0x1FF, write, exec, user);
			if((pml2 & X86_PAGE_ENTRY_PS) != 0)
			{
				address &= 0x1FFFFF;
				*length = 0x200000 - address;
				return (pml2 & 0x000FFFFFFFE00000LL) + address;
			}
			else
			{
				uint64_t pml1 = x86_page_fetch64(emu, full_address, pml3 & 0x000FFFFFFFFFF000LL, (address >> 12) & 0x1FF, write, exec, user);
				address &= 0xFFF;
				*length = 0x1000 - address;
				return (pml1 & 0x000FFFFFFFFFF000LL) + address;
			}
		}
		else
		{
			// 32-bit paging
			// TODO: other CR3 fields
			uint32_t pml2 = x86_page_fetch32(emu, full_address, emu->cr[3] & ~0xFFF, (address >> 22) & 0xFFF, write, exec, user);
			if((emu->cr[4] & X86_CR4_PSE) != 0 && (pml2 & X86_PAGE_ENTRY_PS) != 0)
			{
				address &= 0x3FFFFF;
				*length = 0x400000 - address;
				return ((pml2 & 0xFFC00000) | ((uaddr_t)(pml2 & 0x003FE000) << 19)) + address;
			}
			else
			{
				uint64_t pml1 = x86_page_fetch32(emu, full_address, pml2 & 0xFFFFF000, (address >> 12) & 0xFFF, write, exec, user);
				address &= 0xFFF;
				*length = 0x1000 - address;
				return (pml1 & 0xFFFFF000) + address;
			}
		}
	}
	else if((emu->cr[4] & X86_CR4_VA57) == 0)
	{
		// 4-level paging
		// TODO: other CR3 fields
		uint64_t pml4 = x86_page_fetch64(emu, full_address, emu->cr[3] & ~0xFFF, (address >> 39) & 0x1FF, write, exec, user);
		if((pml4 & X86_PAGE_ENTRY_PS) != 0)
		{
			address &= 0x7FFFFFFFFF;
			*length = 0x8000000000 - address;
			return (pml4 & 0x000FFF8000000000LL) + address;
		}
		uint64_t pml3 = x86_page_fetch64(emu, full_address, pml4 & 0x000FFFFFFFFFF000LL, (address >> 30) & 0x1FF, write, exec, user);
		if((pml3 & X86_PAGE_ENTRY_PS) != 0)
		{
			address &= 0x3FFFFFFF;
			*length = 0x40000000 - address;
			return (pml3 & 0x000FFFFFC0000000LL) + address;
		}
		uint64_t pml2 = x86_page_fetch64(emu, full_address, pml3 & 0x000FFFFFFFFFF000LL, (address >> 21) & 0x1FF, write, exec, user);
		if((pml2 & X86_PAGE_ENTRY_PS) != 0)
		{
			address &= 0x1FFFFF;
			*length = 0x200000 - address;
			return (pml2 & 0x000FFFFFFFE00000LL) + address;
		}
		uint64_t pml1 = x86_page_fetch64(emu, full_address, pml2 & 0x000FFFFFFFFFF000LL, (address >> 12) & 0x1FF, write, exec, user);
		address &= 0xFFF;
		*length = 0x1000 - address;
		return (pml1 & 0xFFFFF000) + address;
	}
	else
	{
		// 5-level paging
		// TODO: other CR3 fields
		uint64_t pml5 = x86_page_fetch64(emu, full_address, emu->cr[3] & ~0xFFF, (address >> 48) & 0x1FF, write, exec, user);
		if((pml5 & X86_PAGE_ENTRY_PS) != 0)
		{
			address &= 0xFFFFFFFFFFFF;
			*length = 0x1000000000000 - address;
			return (pml5 & 0x000F000000000000LL) + address;
		}
		uint64_t pml4 = x86_page_fetch64(emu, full_address, pml5 & 0x000FFFFFFFFFF000LL, (address >> 39) & 0x1FF, write, exec, user);
		if((pml4 & X86_PAGE_ENTRY_PS) != 0)
		{
			address &= 0x7FFFFFFFFF;
			*length = 0x8000000000 - address;
			return (pml4 & 0x000FFF8000000000LL) + address;
		}
		uint64_t pml3 = x86_page_fetch64(emu, full_address, pml4 & 0x000FFFFFFFFFF000LL, (address >> 30) & 0x1FF, write, exec, user);
		if((pml3 & X86_PAGE_ENTRY_PS) != 0)
		{
			address &= 0x3FFFFFFF;
			*length = 0x40000000 - address;
			return (pml3 & 0x000FFFFFC0000000LL) + address;
		}
		uint64_t pml2 = x86_page_fetch64(emu, full_address, pml3 & 0x000FFFFFFFFFF000LL, (address >> 21) & 0x1FF, write, exec, user);
		if((pml2 & X86_PAGE_ENTRY_PS) != 0)
		{
			address &= 0x1FFFFF;
			*length = 0x200000 - address;
			return (pml2 & 0x000FFFFFFFE00000LL) + address;
		}
		uint64_t pml1 = x86_page_fetch64(emu, full_address, pml2 & 0x000FFFFFFFFFF000LL, (address >> 12) & 0x1FF, write, exec, user);
		address &= 0xFFF;
		*length = 0x1000 - address;
		return (pml1 & 0xFFFFF000) + address;
	}
}

static inline void x86_check_breakpoints(x86_state_t * emu, x86_access_t access_type, uaddr_t address, uoff_t count)
{
	if(access_type == X86_ACCESS_IO && (emu->cr[4] & X86_CR4_DE) == 0)
		return;

	for(int breakpoint_number = 0; breakpoint_number < 4; breakpoint_number++)
	{
		if((emu->dr[7] & ((X86_DR7_L0 | X86_DR7_G0) << (2 * breakpoint_number))) == 0)
			continue;

		if(access_type == ((emu->dr[7] >> (X86_DR7_RW0_SHIFT + 2 * breakpoint_number)) & 3))
		{
			static const size_t sizes[] = { 1, 2, 8, 4 };
			size_t size = sizes[(emu->dr[7] >> (X86_DR7_LEN0_SHIFT + 2 * breakpoint_number)) & 3];

			// TODO: check support for size 8
			if((address <= emu->dr[breakpoint_number] && (address > UADDR_MAX - count || address + count > emu->dr[breakpoint_number]))
			|| (address < emu->dr[breakpoint_number] + size && (address > UADDR_MAX - count || address + count >= emu->dr[breakpoint_number] + size)))
			{
				emu->dr[6] |= X86_DR6_B0 << breakpoint_number;
				x86_trigger_interrupt(emu,
					access_type == X86_ACCESS_EXECUTE
						? X86_EXC_DB | X86_EXC_FAULT
						: X86_EXC_DB | X86_EXC_TRAP,
					0);
			}
		}
	}
}

inline void x86_memory_read(x86_state_t * emu, uaddr_t address, uaddr_t count, void * buffer)
{
	x86_check_breakpoints(emu, X86_ACCESS_READ, address, count);

	while(count > 0)
	{
		uoff_t actual_length;
		uaddr_t physical_address = x86_page_translate(emu, address, false, false, emu->cpl == 3, &actual_length);
		if(actual_length > count || actual_length == 0)
			actual_length = count;
		x86_memory_read_no_paging(emu, physical_address, count, buffer);
		address += actual_length;
		buffer += actual_length;
		count -= actual_length;
	}
}

// accesses system memory, ignoring current privilege
static inline void x86_memory_read_system(x86_state_t * emu, uaddr_t address, uaddr_t count, void * buffer)
{
	x86_check_breakpoints(emu, X86_ACCESS_READ, address, count);

	while(count > 0)
	{
		uoff_t actual_length;
		uaddr_t physical_address = x86_page_translate(emu, address, false, false, false, &actual_length);
		if(actual_length > count || actual_length == 0)
			actual_length = count;
		x86_memory_read_no_paging(emu, physical_address, count, buffer);
		address += actual_length;
		buffer += actual_length;
		count -= actual_length;
	}
}

// Used for instruction fetches, this is needed for V25 that does not use its internal memory for instruction fetches
static inline void x86_memory_read_prefetch(x86_state_t * emu, uaddr_t address, uaddr_t count, void * buffer)
{
	while(count > 0)
	{
		uoff_t actual_length;
		uaddr_t physical_address = x86_page_translate(emu, address, false, true, emu->cpl == 3, &actual_length);
		if(actual_length > count || actual_length == 0)
			actual_length = count;
		x86_memory_read_external(emu, physical_address, count, buffer);
		address += actual_length;
		buffer += actual_length;
		count -= actual_length;
	}
}

// Used for instruction fetches, this is needed for V25 that does not use its internal memory for instruction fetches
static inline void x86_memory_read_exec(x86_state_t * emu, uaddr_t address, uaddr_t count, void * buffer)
{
	x86_check_breakpoints(emu, X86_ACCESS_EXECUTE, address, count);

	if(emu->prefetch_queue_data_size > 0)
	{
		uaddr_t queue_count = count;
		if(queue_count > emu->prefetch_queue_data_size)
			queue_count = emu->prefetch_queue_data_size;
		memcpy(buffer, &emu->prefetch_queue[emu->prefetch_queue_data_offset], queue_count);
		emu->prefetch_queue_data_offset += queue_count;
		emu->prefetch_queue_data_size -= queue_count;
		address += queue_count;
		buffer += queue_count;
		count -= queue_count;
	}

	x86_memory_read_prefetch(emu, address, count, buffer);
}

void x86_memory_write(x86_state_t * emu, uaddr_t address, uaddr_t count, const void * buffer)
{
	x86_check_breakpoints(emu, X86_ACCESS_WRITE, address, count);

	while(count > 0)
	{
		uoff_t actual_length;
		uaddr_t physical_address = x86_page_translate(emu, address, true, false, emu->cpl == 3, &actual_length);
		if(actual_length > count || actual_length == 0)
			actual_length = count;
		x86_memory_write_no_paging(emu, physical_address, count, buffer);
		address += actual_length;
		buffer += actual_length;
		count -= actual_length;
	}
}

// accesses system memory, ignoring current privilege
static inline void x86_memory_write_system(x86_state_t * emu, uaddr_t address, uaddr_t count, const void * buffer)
{
	x86_check_breakpoints(emu, X86_ACCESS_WRITE, address, count);

	while(count > 0)
	{
		uoff_t actual_length;
		uaddr_t physical_address = x86_page_translate(emu, address, true, false, false, &actual_length);
		if(actual_length > count || actual_length == 0)
			actual_length = count;
		x86_memory_write_no_paging(emu, physical_address, count, buffer);
		address += actual_length;
		buffer += actual_length;
		count -= actual_length;
	}
}

// prefetch queue handling

static inline void x86_prefetch_queue_rewind(x86_state_t * emu)
{
	// note: (R)IP must also be reset, otherwise the queue will be invalid
	emu->prefetch_queue_data_size += emu->prefetch_queue_data_offset;
	emu->prefetch_queue_data_offset = 0;
}

static inline void x86_prefetch_queue_flush(x86_state_t * emu)
{
	emu->prefetch_queue_data_size = 0;
	emu->prefetch_queue_data_offset = 0;
	emu->prefetch_pointer = emu->xip;
}

static inline void x86_prefetch_queue_fill(x86_state_t * emu)
{
	if(emu->xip + emu->prefetch_queue_data_size != emu->prefetch_pointer)
	{
		// invalid prefetch data, start from scratch
		x86_prefetch_queue_flush(emu);
	}

	if(emu->prefetch_queue_data_offset != 0 && emu->prefetch_queue_data_size != 0)
	{
		memcpy(&emu->prefetch_queue[0], &emu->prefetch_queue[emu->prefetch_queue_data_offset], emu->prefetch_queue_data_size);
		emu->prefetch_queue_data_offset = 0;
	}

	if(setjmp(emu->exc[emu->fetch_mode = FETCH_MODE_PREFETCH]) == 0)
	{
		while(emu->prefetch_queue_data_size < emu->cpu_traits.prefetch_queue_size)
		{
			// TODO: memory wrapping
			x86_memory_read_prefetch(emu,
				x86_memory_segmented_to_linear(emu, X86_R_CS, emu->prefetch_pointer),
				1,
				&emu->prefetch_queue[emu->prefetch_queue_data_offset + emu->prefetch_queue_data_size]);

			emu->prefetch_queue_data_size++;
			emu->prefetch_pointer++;
		}
	}

	emu->fetch_mode = FETCH_MODE_NORMAL;
}

static inline void x86_memory_segmented_read_exec(x86_state_t * emu, x86_segnum_t segment_number, uoff_t offset, uaddr_t count, void * buffer)
{
	assert(segment_number == X86_R_CS);

	if(emu->cpu_type == X86_CPU_8086 || emu->cpu_type == X86_CPU_V60 || emu->cpu_type == X86_CPU_V20 || emu->cpu_type == X86_CPU_V33 || emu->cpu_type == X86_CPU_UPD9002)
	{
		offset &= 0xFFFF;
	}

	if((emu->cpu_type == X86_CPU_8086 || emu->cpu_type == X86_CPU_V60 || emu->cpu_type == X86_CPU_V20 || emu->cpu_type == X86_CPU_V33 || emu->cpu_type == X86_CPU_UPD9002)
		&& offset + count > 0x10000)
	{
		// segment wrapping
		uaddr_t actual_count = min(count, 0x10000 - (offset & 0xFFFF));
		x86_memory_read_exec(emu,
			x86_memory_segmented_to_linear(emu, segment_number, offset),
			actual_count,
			buffer);
		offset = (offset + actual_count) & 0xFFFF;
		count -= actual_count;
		buffer = (char *)buffer + actual_count;
	}

	// the V25 explicitly permits 64K overflow
	if(segment_number < X86_SR_COUNT || segment_number == X86_R_FDS)
	{
		// segment registers (including FDS) check the limits
		x86_segment_check_limit(emu, segment_number, offset, count, 0);
		// otherwise it is a table register, limit checks are separate
	}

	x86_memory_read_exec(emu,
		x86_memory_segmented_to_linear(emu, segment_number, offset),
		count,
		buffer);
}

static inline uint8_t x86_memory_segmented_read8_exec(x86_state_t * emu, x86_segnum_t segment_number, uoff_t offset)
{
	uint8_t result = 0;
	x86_memory_segmented_read_exec(emu, segment_number, offset, 1, &result);
	return result;
}

static inline uint16_t x86_memory_segmented_read16_exec(x86_state_t * emu, x86_segnum_t segment_number, uoff_t offset)
{
	uint16_t result = 0;
	x86_memory_segmented_read_exec(emu, segment_number, offset, 2, &result);
	return le16toh(result);
}

static inline uint32_t x86_memory_segmented_read32_exec(x86_state_t * emu, x86_segnum_t segment_number, uoff_t offset)
{
	uint32_t result = 0;
	x86_memory_segmented_read_exec(emu, segment_number, offset, 4, &result);
	return le32toh(result);
}

static inline uint64_t x86_memory_segmented_read64_exec(x86_state_t * emu, x86_segnum_t segment_number, uoff_t offset)
{
	uint64_t result = 0;
	x86_memory_segmented_read_exec(emu, segment_number, offset, 8, &result);
	return le64toh(result);
}

static inline void x86_memory_segmented_read(x86_state_t * emu, x86_segnum_t segment_number, uoff_t offset, uaddr_t count, void * buffer)
{
	if((emu->cpu_type == X86_CPU_V55 || emu->cpu_type == X86_CPU_EXTENDED) && segment_number == X86_R_IRAM)
	{
		// IRAM uses the internal RAM
		char * _buffer = buffer;
		offset &= 0x1FF;
		x86_store_register_bank(emu);
		for(;;)
		{
			size_t actual_count = min(count, 0x200 - offset);
			memcpy(_buffer, &emu->iram[offset], actual_count);
			if(actual_count == count)
				return;
			_buffer += actual_count;
			offset = 0;
			count -= actual_count;
		}
	}
	else
	{
		if(emu->cpu_type == X86_CPU_8086 || emu->cpu_type == X86_CPU_V60 || emu->cpu_type == X86_CPU_V20 || emu->cpu_type == X86_CPU_V33 || emu->cpu_type == X86_CPU_UPD9002)
		{
			offset &= 0xFFFF;
		}

		if((emu->cpu_type == X86_CPU_8086 || emu->cpu_type == X86_CPU_V60 || emu->cpu_type == X86_CPU_V20 || emu->cpu_type == X86_CPU_V33 || emu->cpu_type == X86_CPU_UPD9002)
			&& offset + count > 0x10000)
		{
			while(count > 0)
			{
				// segment wrapping
				uaddr_t actual_count = min(count, 0x10000 - (offset & 0xFFFF));
				x86_memory_read(emu,
					x86_memory_segmented_to_linear(emu, segment_number, offset),
					actual_count,
					buffer);
				offset = (offset + actual_count) & 0xFFFF;
				count -= actual_count;
				buffer = (char *)buffer + actual_count;
			}
		}
		else if(segment_number < X86_SR_COUNT || segment_number == X86_R_FDS)
		{
			// TODO: check for null selector (here and read)?
			// segment registers (including FDS) check the limits
			x86_segment_check_read(emu, segment_number);
			x86_segment_check_limit(emu, segment_number, offset, count, 0);
			x86_memory_read(emu,
				x86_memory_segmented_to_linear(emu, segment_number, offset),
				count,
				buffer);
		}
		else
		{
			// otherwise it is a table register, limit checks are separate
			x86_memory_read_system(emu,
				x86_memory_segmented_to_linear(emu, segment_number, offset),
				count,
				buffer);
		}
	}
}

// Memory access on the 8087/808287/80387 might cause a CSO/MP exception if the access is beyond the first 2 bytes used by the x86 CPU
static inline void x87_memory_segmented_read(x86_state_t * emu, x86_segnum_t segment_number, uoff_t x86_offset, uoff_t offset, uaddr_t count, void * buffer)
{
	if((emu->cpu_type == X86_CPU_V55 || emu->cpu_type == X86_CPU_EXTENDED) && segment_number == X86_R_IRAM)
	{
		char * _buffer = buffer;
		offset &= 0x1FF;
		x86_store_register_bank(emu);
		for(;;)
		{
			size_t actual_count = min(count, 0x200 - offset);
			memcpy(_buffer, &emu->iram[offset], actual_count);
			if(actual_count == count)
				return;
			_buffer += actual_count;
			offset = 0;
			count -= actual_count;
		}
	}
	else
	{
		if(emu->x87.fpu_type != X87_FPU_INTEGRATED && x86_offset <= offset && offset < x86_offset + 1)
		{
			// The first 2 bytes have already been read
			uaddr_t actual_count = max(2, x86_offset + 2 - offset);
			memcpy(buffer, &emu->x87.operand_data[offset - x86_offset], actual_count);
			if(count == actual_count)
				return;
			offset = (offset + actual_count) & 0xFFFF;
			count -= actual_count;
			buffer = (char *)buffer + actual_count;
		}

		if(emu->cpu_type == X86_CPU_8086 || emu->cpu_type == X86_CPU_V60 || emu->cpu_type == X86_CPU_V20 || emu->cpu_type == X86_CPU_V33 || emu->cpu_type == X86_CPU_UPD9002)
		{
			offset &= 0xFFFF;
		}

		if((emu->cpu_type == X86_CPU_8086 || emu->cpu_type == X86_CPU_V60 || emu->cpu_type == X86_CPU_V20 || emu->cpu_type == X86_CPU_V33 || emu->cpu_type == X86_CPU_UPD9002)
			&& offset + count > 0x10000)
		{
			while(count > 0)
			{
				// segment wrapping
				uaddr_t actual_count = min(count, 0x10000 - (offset & 0xFFFF));
				x86_memory_read(emu,
					x86_memory_segmented_to_linear(emu, segment_number, offset),
					actual_count,
					buffer);
				offset = (offset + actual_count) & 0xFFFF;
				count -= actual_count;
				buffer = (char *)buffer + actual_count;
			}
		}
		else
		{
			// TODO: check for null selector (here and read)?
			// segment registers (including FDS) check the limits
			x86_segment_check_read(emu, segment_number);
			x87_segment_check_limit(emu, segment_number, x86_offset, offset, count, 0);
			x86_memory_read(emu,
				x86_memory_segmented_to_linear(emu, segment_number, offset),
				count,
				buffer);
		}
	}
}

static inline uint8_t x86_memory_segmented_read8(x86_state_t * emu, x86_segnum_t segment_number, uoff_t offset)
{
	uint8_t result = 0;
	emu->ind_register = offset;
	x86_memory_segmented_read(emu, segment_number, offset, 1, &result);
	return emu->opr_register = result;
}

static inline uint16_t x86_memory_segmented_read16(x86_state_t * emu, x86_segnum_t segment_number, uoff_t offset)
{
	uint16_t result = 0;
	emu->ind_register = offset;
	x86_memory_segmented_read(emu, segment_number, offset, 2, &result);
	return emu->opr_register = le16toh(result);
}

static inline uint32_t x86_memory_segmented_read32(x86_state_t * emu, x86_segnum_t segment_number, uoff_t offset)
{
	uint32_t result = 0;
	x86_memory_segmented_read(emu, segment_number, offset, 4, &result);
	return le32toh(result);
}

static inline uint64_t x86_memory_segmented_read64(x86_state_t * emu, x86_segnum_t segment_number, uoff_t offset)
{
	uint64_t result = 0;
	x86_memory_segmented_read(emu, segment_number, offset, 8, &result);
	return le64toh(result);
}

static inline x87_float80_t x86_memory_segmented_read80fp(x86_state_t * emu, x86_segnum_t segment_number, uoff_t offset)
{
	uint64_t fraction;
	uint16_t exponent;
	bool sign;
	fraction = x86_memory_segmented_read64(emu, segment_number, offset);
	exponent = x86_memory_segmented_read16(emu, segment_number, offset + 8);
	sign = (exponent & 0x8000) != 0;
	exponent &= 0x7FFF;
	return x87_convert_to_float80(fraction, exponent, sign);
}

static inline void x86_memory_segmented_read128(x86_state_t * emu, x86_segnum_t segment_number, uoff_t offset, x86_sse_register_t * value)
{
	value->XMM_Q(0) = x86_memory_segmented_read64(emu, segment_number, offset);
	value->XMM_Q(1) = x86_memory_segmented_read64(emu, segment_number, offset + 8);
}

static inline uint16_t x87_memory_segmented_read16(x86_state_t * emu, x86_segnum_t segment_number, uoff_t x86_offset, uoff_t offset)
{
	uint16_t result = 0;
	x87_memory_segmented_read(emu, segment_number, x86_offset, offset, 2, &result);
	return le16toh(result);
}

static inline uint32_t x87_memory_segmented_read32(x86_state_t * emu, x86_segnum_t segment_number, uoff_t x86_offset, uoff_t offset)
{
	uint32_t result = 0;
	x87_memory_segmented_read(emu, segment_number, x86_offset, offset, 4, &result);
	return le32toh(result);
}

static inline x87_float80_t x87_memory_segmented_read32fp(x86_state_t * emu, x86_segnum_t segment_number, uoff_t x86_offset, uoff_t offset)
{
	uint32_t result = 0;
	x87_memory_segmented_read(emu, segment_number, x86_offset, offset, 4, &result);
	return x87_convert64_to_float(emu, le32toh(result));
}

static inline uint64_t x87_memory_segmented_read64(x86_state_t * emu, x86_segnum_t segment_number, uoff_t x86_offset, uoff_t offset)
{
	uint64_t result = 0;
	x87_memory_segmented_read(emu, segment_number, x86_offset, offset, 8, &result);
	return le64toh(result);
}

static inline x87_float80_t x87_memory_segmented_read80fp(x86_state_t * emu, x86_segnum_t segment_number, uoff_t x86_offset, uoff_t offset)
{
	uint64_t fraction;
	uint16_t exponent;
	bool sign;
	fraction = x87_memory_segmented_read64(emu, segment_number, x86_offset, offset);
	exponent = x87_memory_segmented_read16(emu, segment_number, x86_offset, offset + 8);
	sign = (exponent & 0x8000) != 0;
	exponent &= 0x7FFF;
	return x87_convert_to_float80(fraction, exponent, sign);
}

static inline void x86_memory_segmented_write(x86_state_t * emu, x86_segnum_t segment_number, uoff_t offset, uaddr_t count, const void * buffer)
{
	if((emu->cpu_type == X86_CPU_V55 || emu->cpu_type == X86_CPU_EXTENDED) && segment_number == X86_R_IRAM)
	{
		const char * _buffer = buffer;
		offset &= 0x1FF;
		x86_store_register_bank(emu);
		for(;;)
		{
			size_t actual_count = min(count, 0x200 - offset);
			memcpy(&emu->iram[offset], _buffer, actual_count);
			x86_load_register_bank(emu);
			if(actual_count == count)
				return;
			_buffer += actual_count;
			offset = 0;
			count -= actual_count;
		}
	}
	else
	{
		if(emu->cpu_type == X86_CPU_8086 || emu->cpu_type == X86_CPU_V60 || emu->cpu_type == X86_CPU_V20 || emu->cpu_type == X86_CPU_V33 || emu->cpu_type == X86_CPU_UPD9002)
		{
			offset &= 0xFFFF;
		}

		if((emu->cpu_type == X86_CPU_8086 || emu->cpu_type == X86_CPU_V60 || emu->cpu_type == X86_CPU_V20 || emu->cpu_type == X86_CPU_V33 || emu->cpu_type == X86_CPU_UPD9002)
			&& offset + count > 0x10000)
		{
			while(count > 0)
			{
				// segment wrapping
				uaddr_t actual_count = min(count, 0x10000 - (offset & 0xFFFF));
				x86_memory_write(emu,
					x86_memory_segmented_to_linear(emu, segment_number, offset),
					actual_count,
					buffer);
				offset = (offset + actual_count) & 0xFFFF;
				count -= actual_count;
				buffer = (const char *)buffer + actual_count;
			}
		}
		else if(segment_number < X86_SR_COUNT || segment_number == X86_R_FDS)
		{
			// TODO: check for null selector (here and read)?
			// segment registers (including FDS) check the limits
			x86_segment_check_write(emu, segment_number);
			x86_segment_check_limit(emu, segment_number, offset, count, 0);
			x86_memory_write(emu,
				x86_memory_segmented_to_linear(emu, segment_number, offset),
				count,
				buffer);
		}
		else
		{
			// otherwise it is a table register, limit checks are separate
			x86_memory_write_system(emu,
				x86_memory_segmented_to_linear(emu, segment_number, offset),
				count,
				buffer);
		}
	}
}

static inline void x87_memory_segmented_write(x86_state_t * emu, x86_segnum_t segment_number, uoff_t x86_offset, uoff_t offset, uaddr_t count, const void * buffer)
{
	if((emu->cpu_type == X86_CPU_V55 || emu->cpu_type == X86_CPU_EXTENDED) && segment_number == X86_R_IRAM)
	{
		const char * _buffer = buffer;
		offset &= 0x1FF;
		x86_store_register_bank(emu);
		for(;;)
		{
			size_t actual_count = min(count, 0x200 - offset);
			memcpy(&emu->iram[offset], _buffer, actual_count);
			x86_load_register_bank(emu);
			if(actual_count == count)
				return;
			_buffer += actual_count;
			offset = 0;
			count -= actual_count;
		}
	}
	else
	{
		if(emu->cpu_type == X86_CPU_8086 || emu->cpu_type == X86_CPU_V60 || emu->cpu_type == X86_CPU_V20 || emu->cpu_type == X86_CPU_V33 || emu->cpu_type == X86_CPU_UPD9002)
		{
			offset &= 0xFFFF;
		}

		if((emu->cpu_type == X86_CPU_8086 || emu->cpu_type == X86_CPU_V60 || emu->cpu_type == X86_CPU_V20 || emu->cpu_type == X86_CPU_V33 || emu->cpu_type == X86_CPU_UPD9002)
			&& offset + count > 0x10000)
		{
			while(count > 0)
			{
				// segment wrapping
				uaddr_t actual_count = min(count, 0x10000 - (offset & 0xFFFF));
				x86_memory_write(emu,
					x86_memory_segmented_to_linear(emu, segment_number, offset),
					actual_count,
					buffer);
				offset = (offset + actual_count) & 0xFFFF;
				count -= actual_count;
				buffer = (const char *)buffer + actual_count;
			}
		}
		else
		{
			// TODO: check for null selector (here and read)?
			// segment registers (including FDS) check the limits
			x86_segment_check_write(emu, segment_number);
			x87_segment_check_limit(emu, segment_number, x86_offset, offset, count, 0);
			x86_memory_write(emu,
				x86_memory_segmented_to_linear(emu, segment_number, offset),
				count,
				buffer);
		}
	}
}

static inline void x86_memory_segmented_write8(x86_state_t * emu, x86_segnum_t segment_number, uoff_t offset, uint8_t value)
{
	emu->ind_register = offset;
	emu->opr_register = value;
	x86_memory_segmented_write(emu, segment_number, offset, 1, &value);
}

static inline void x86_memory_segmented_write16(x86_state_t * emu, x86_segnum_t segment_number, uoff_t offset, uint16_t value)
{
	emu->ind_register = offset;
	emu->opr_register = value;
	value = htole16(value);
	x86_memory_segmented_write(emu, segment_number, offset, 2, &value);
}

static inline void x86_memory_segmented_write32(x86_state_t * emu, x86_segnum_t segment_number, uoff_t offset, uint32_t value)
{
	value = htole32(value);
	x86_memory_segmented_write(emu, segment_number, offset, 4, &value);
}

static inline void x86_memory_segmented_write64(x86_state_t * emu, x86_segnum_t segment_number, uoff_t offset, uint64_t value)
{
	value = htole64(value);
	x86_memory_segmented_write(emu, segment_number, offset, 8, &value);
}

static inline void x86_memory_segmented_write80fp(x86_state_t * emu, x86_segnum_t segment_number, uoff_t offset, x87_float80_t value)
{
	uint64_t fraction;
	uint16_t exponent;
	bool sign;
	x87_convert_from_float80(value, &fraction, &exponent, &sign);
	x86_memory_segmented_write64(emu, segment_number, offset,     fraction);
	x86_memory_segmented_write16(emu, segment_number, offset + 8, exponent | (sign ? 0x8000 : 0));
}

static inline void x86_memory_segmented_write128(x86_state_t * emu, x86_segnum_t segment_number, uoff_t offset, x86_sse_register_t * value)
{
	x86_memory_segmented_write64(emu, segment_number, offset,     value->XMM_Q(0));
	x86_memory_segmented_write64(emu, segment_number, offset + 8, value->XMM_Q(1));
}

static inline void x87_memory_segmented_write16(x86_state_t * emu, x86_segnum_t segment_number, uoff_t x86_offset, uoff_t offset, uint16_t value)
{
	value = htole16(value);
	x87_memory_segmented_write(emu, segment_number, x86_offset, offset, 2, &value);
}

static inline void x87_memory_segmented_write32(x86_state_t * emu, x86_segnum_t segment_number, uoff_t x86_offset, uoff_t offset, uint32_t value)
{
	value = htole32(value);
	x87_memory_segmented_write(emu, segment_number, x86_offset, offset, 4, &value);
}

static inline void x87_memory_segmented_write64(x86_state_t * emu, x86_segnum_t segment_number, uoff_t x86_offset, uoff_t offset, uint64_t value)
{
	value = htole64(value);
	x87_memory_segmented_write(emu, segment_number, x86_offset, offset, 8, &value);
}

static inline void x87_memory_segmented_write80fp(x86_state_t * emu, x86_segnum_t segment_number, uoff_t x86_offset, uoff_t offset, x87_float80_t value)
{
	uint64_t fraction;
	uint16_t exponent;
	bool sign;
	x87_convert_from_float80(value, &fraction, &exponent, &sign);
	x87_memory_segmented_write64(emu, segment_number, x86_offset, offset,     fraction);
	x87_memory_segmented_write16(emu, segment_number, x86_offset, offset + 8, exponent | (sign ? 0x8000 : 0));
}

// I/O

static inline void x86_input(x86_state_t * emu, uint16_t port, uint16_t count, void * buffer)
{
	x86_check_breakpoints(emu, X86_ACCESS_IO, port, count);

	if(emu->cpu_type == X86_CPU_186 && (emu->pcb[X86_PCB_PCR] & X86_PCB_PCR_MIO) != 0)
	{
		// The 80186 checks for its internal registers
		uaddr_t actual_count;
		uint16_t pcb_port = (emu->pcb[X86_PCB_PCR] & X86_PCB_PCR_ADDRESS) << 8;
		if(port < pcb_port)
		{
			actual_count = min(count, pcb_port - port);
			if(emu->port_read != NULL)
				emu->port_read(emu, port, buffer, count);
			if(actual_count == count)
				return;
			port = pcb_port;
			buffer = (char *)buffer + actual_count;
			count -= actual_count;
		}

		if(port < pcb_port + 0x100)
		{
			actual_count = min(count, 0x100);
			memcpy(buffer, (uint8_t *)emu->pcb + (port - pcb_port), actual_count);
			if(actual_count == count)
				return;
			port += actual_count;
			buffer = (char *)buffer + actual_count;
			count -= actual_count;
		}
	}
	else if(emu->cpu_type == X86_CPU_V33)
	{
		// V33 internal registers
		uaddr_t actual_count;
		if(port < 0xFF00)
		{
			actual_count = min(count, 0xFF00 - port);
			if(emu->port_read != NULL)
				emu->port_read(emu, port, buffer, count);
			if(actual_count == count)
				return;
			port = 0xFF00;
			buffer = (char *)buffer + actual_count;
			count -= actual_count;
		}

		if(port < 0xFF81)
		{
			actual_count = min(count, 0x81);
			memcpy(buffer, &emu->v33_io[port - 0xFF00], actual_count);
			if(actual_count == count)
				return;
			port += actual_count;
			buffer = (char *)buffer + actual_count;
			count -= actual_count;
		}
	}
	else if(emu->cpu_type == X86_CPU_CYRIX)
	{
		// Cyrix configuration registers
		if(port == 0x0023 && emu->port22_accessed)
		{
			// otherwise, another I/O clears the access anyway
			*(char *)buffer = x86_cyrix_register_get(emu, emu->port_number);
			port += 1;
			buffer = (char *)buffer + 1;
			count -= 1;
		}
		emu->port22_accessed = false;

		if(count == 0)
			return;
		else
			emu->port22_accessed = false;
	}

	if(emu->port_read != NULL)
		emu->port_read(emu, port, buffer, count);
}

static inline uint8_t x86_input8(x86_state_t * emu, uint16_t port)
{
	uint8_t result = 0;
	x86_input(emu, port, 1, &result);
	return result;
}

static inline uint16_t x86_input16(x86_state_t * emu, uint16_t port)
{
	uint16_t result = 0;
	x86_input(emu, port, 2, &result);
	return le16toh(result);
}

static inline uint32_t x86_input32(x86_state_t * emu, uint16_t port)
{
	uint32_t result = 0;
	x86_input(emu, port, 4, &result);
	return le32toh(result);
}

static inline void x86_output(x86_state_t * emu, uint16_t port, uint16_t count, const void * buffer)
{
	x86_check_breakpoints(emu, X86_ACCESS_IO, port, count);

	if(emu->cpu_type == X86_CPU_186 && (emu->pcb[X86_PCB_PCR] & X86_PCB_PCR_MIO) != 0)
	{
		// The 80186 checks for its internal registers
		uaddr_t actual_count;
		uint16_t pcb_port = (emu->pcb[X86_PCB_PCR] & X86_PCB_PCR_ADDRESS) << 8;
		if(port < pcb_port)
		{
			actual_count = min(count, pcb_port - port);
			if(emu->port_write != NULL)
				emu->port_write(emu, port, buffer, count);
			if(actual_count == count)
				return;
			port = pcb_port;
			buffer = (const char *)buffer + actual_count;
			count -= actual_count;
		}

		if(port < pcb_port + 0x100)
		{
			actual_count = min(count, 0x100);
			memcpy((uint8_t *)emu->pcb + (port - pcb_port), buffer, actual_count);
			if(actual_count == count)
				return;
			port += actual_count;
			buffer = (const char *)buffer + actual_count;
			count -= actual_count;
		}
	}
	else if(emu->cpu_type == X86_CPU_V33)
	{
		// V33 internal registers
		uaddr_t actual_count;
		if(port < 0xFF00)
		{
			actual_count = min(count, 0xFF00 - port);
			if(emu->port_write != NULL)
				emu->port_write(emu, port, buffer, count);
			if(actual_count == count)
				return;
			port = 0xFF00;
			buffer = (const char *)buffer + actual_count;
			count -= actual_count;
		}

		if(port < 0xFF81)
		{
			actual_count = min(count, 0x81);
			memcpy(&emu->v33_io[port - 0xFF00], buffer, actual_count);
			if(actual_count == count)
				return;
			port += actual_count;
			buffer = (const char *)buffer + actual_count;
			count -= actual_count;
		}
	}
	else if(emu->cpu_type == X86_CPU_CYRIX)
	{
		// Cyrix configuration registers
		if(port < 0x0022)
		{
			size_t actual_count = min(count, 0x0022 - port);
			emu->port_write(emu, port, buffer, actual_count);
			if(actual_count == count)
				return;
			port += actual_count;
			buffer = (const char *)buffer + actual_count;
			count -= actual_count;

			emu->port22_accessed = true;
		}

		if(port == 0x0022)
		{
			emu->port_number = *(const char *)buffer;
			switch(emu->cpu_traits.cpu_subtype)
			{
			case X86_CPU_CYRIX_CX486SLC:
			case X86_CPU_CYRIX_CX486SLCE:
				emu->port22_accessed = 0xC0 <= emu->port_number && emu->port_number <= 0xCF;
				break;
			case X86_CPU_CYRIX_5X86:
			case X86_CPU_CYRIX_6X86:
				emu->port22_accessed =
					(0xC0 <= emu->port_number && emu->port_number <= 0xCF)
					|| 0xFE <= emu->port_number
					|| ((emu->ccr[3] & X86_CCR3_MAPEN_MASK) >> X86_CCR3_MAPEN_SHIFT) == 0x01;
				break;
			case X86_CPU_CYRIX_MEDIAGX:
			case X86_CPU_CYRIX_GXM:
			case X86_CPU_CYRIX_GX1:
				emu->port22_accessed =
					(0xC0 <= emu->port_number && emu->port_number <= 0xCF)
					|| 0xFE <= emu->port_number
					|| (emu->ccr[3] & X86_CCR3_MAPEN) != 0;
				break;
			default:
			case X86_CPU_CYRIX_GX2:
			case X86_CPU_CYRIX_LX:
				emu->port22_accessed = false;
				break;
			case X86_CPU_CYRIX_M2:
			case X86_CPU_CYRIX_III:
				emu->port22_accessed =
					(0xC0 <= emu->port_number && emu->port_number <= 0xCF)
					|| 0xFE <= emu->port_number
					|| ((emu->ccr[3] & X86_CCR3_MAPEN_MASK) >> X86_CCR3_MAPEN_SHIFT) != 0;
				break;
			}

			if(emu->port22_accessed)
			{
				*(char *)buffer = x86_cyrix_register_get(emu, emu->port_number);
				port += 1;
				buffer = (char *)buffer + 1;
				count -= 1;
			}
		}

		if(port == 0x0023 && emu->port22_accessed)
		{
			x86_cyrix_register_set(emu, emu->port_number, *(char *)buffer);
			port += 1;
			buffer = (char *)buffer + 1;
			count -= 1;
			emu->port22_accessed = false;
		}

		if(count == 0)
			return;
		else
			emu->port22_accessed = false;
	}

	if(emu->port_write != NULL)
		emu->port_write(emu, port, buffer, count);
}

static inline void x86_output8(x86_state_t * emu, uint16_t port, uint8_t value)
{
	x86_output(emu, port, 1, &value);
}

static inline void x86_output16(x86_state_t * emu, uint16_t port, uint16_t value)
{
	value = htole16(value);
	x86_output(emu, port, 2, &value);
}

static inline void x86_output32(x86_state_t * emu, uint16_t port, uint32_t value)
{
	value = htole32(value);
	x86_output(emu, port, 4, &value);
}

// 8080/8085/Z80 checks

static inline x86_state_t * _x80_get_x86(x80_state_t * emu)
{
	// Note: this only works if emu is stored inside an x86_state_t, otherwise this will fail
	return (void *)emu - offsetof(x86_state_t, x80);
}

// for emulated CPUs, reverts to x86 routines, otherwise it accesses its own callbacks

static inline uint8_t x80_memory_read8(x80_state_t * emu, uint16_t address)
{
	if(emu->cpu_method == X80_CPUMETHOD_EMULATED)
	{
		return x86_memory_segmented_read8(_x80_get_x86(emu), X86_R_DS, address);
	}
	else
	{
		uint8_t value;
		emu->memory_read(emu, address, &value, 1);
		return value;
	}
}

static inline uint16_t x80_memory_read16(x80_state_t * emu, uint16_t address)
{
	if(emu->cpu_method == X80_CPUMETHOD_EMULATED)
	{
		return x86_memory_segmented_read16(_x80_get_x86(emu), X86_R_DS, address);
	}
	else
	{
		uint16_t value;
		emu->memory_read(emu, address, &value, 2);
		return le16toh(value);
	}
}

static inline void x80_memory_write8(x80_state_t * emu, uint16_t address, uint8_t value)
{
	if(emu->cpu_method == X80_CPUMETHOD_EMULATED)
	{
		x86_memory_segmented_write8(_x80_get_x86(emu), X86_R_DS, address, value);
	}
	else
	{
		emu->memory_write(emu, address, &value, 1);
	}
}

static inline void x80_memory_write16(x80_state_t * emu, uint16_t address, uint16_t value)
{
	if(emu->cpu_method == X80_CPUMETHOD_EMULATED)
	{
		x86_memory_segmented_write16(_x80_get_x86(emu), X86_R_DS, address, value);
	}
	else
	{
		value = htole16(value);
		emu->memory_write(emu, address, &value, 2);
	}
}

static inline uint8_t x80_memory_fetch8(x80_state_t * emu)
{
	if(emu->peripheral_data_pointer < emu->peripheral_data_length)
	{
		uint8_t byte = emu->peripheral_data[emu->peripheral_data_pointer++];
		if(emu->peripheral_data_pointer == emu->peripheral_data_length)
			free(emu->peripheral_data);
		return byte;
	}
	else
	{
		uint16_t pc = emu->pc;
		emu->pc += 1;
		if(emu->cpu_method == X80_CPUMETHOD_EMULATED)
		{
			return x86_memory_segmented_read8_exec(_x80_get_x86(emu), X86_R_CS, pc);
		}
		else
		{
			uint8_t value;
			emu->memory_fetch(emu, pc, &value, 1);
			return value;
		}
	}
}

static inline uint16_t x80_memory_fetch16(x80_state_t * emu)
{
	if(emu->peripheral_data_pointer + 1 < emu->peripheral_data_length)
	{
		uint8_t byte = x80_memory_fetch8(emu);
		return byte | (x80_memory_fetch8(emu) << 8);
	}
	else if(emu->peripheral_data_pointer + 1 == emu->peripheral_data_length)
	{
		uint16_t word = le16toh(*(uint16_t *)&emu->peripheral_data[emu->peripheral_data_pointer]);
		emu->peripheral_data_pointer += 2;
		if(emu->peripheral_data_pointer == emu->peripheral_data_length)
			free(emu->peripheral_data);
		return word;
	}
	else
	{
		uint16_t pc = emu->pc;
		emu->pc += 2;
		if(emu->cpu_method == X80_CPUMETHOD_EMULATED)
		{
			return x86_memory_segmented_read16_exec(_x80_get_x86(emu), X86_R_CS, pc);
		}
		else
		{
			uint16_t value;
			emu->memory_fetch(emu, pc, &value, 2);
			return le16toh(value);
		}
	}
}

static inline uint8_t x80_input8(x80_state_t * emu, uint16_t port)
{
	if(emu->cpu_method == X80_CPUMETHOD_EMULATED)
	{
		return x86_input8(_x80_get_x86(emu), port);
	}
	else
	{
		return emu->port_read(emu, port);
	}
}

static inline void x80_output8(x80_state_t * emu, uint16_t port, uint8_t value)
{
	if(emu->cpu_method == X80_CPUMETHOD_EMULATED)
	{
		x86_output8(_x80_get_x86(emu), port, value);
	}
	else
	{
		emu->port_write(emu, port, value);
	}
}

