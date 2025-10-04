ENTRY(_start)
SECTIONS
{
	.text 0x103 :
	{
		*(.text)
	}

	.rodata :
	{
		*(.rodata)
	}

	HIDDEN(data_start = .);
	.data :
	{
		*(INITIALIZER)
		*(.data)
	}

	.bss :
	{
		*(.bss)
	}

	.fake_data data_start :
	{
		*(INITIALIZED)
	}
}
