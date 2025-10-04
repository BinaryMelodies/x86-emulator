ENTRY(_start)
PHDRS
{
	text PT_LOAD;
	data PT_LOAD;
}
SECTIONS
{
	.text :
	{
		*(.text)
	} :text

	HIDDEN(textend = .);
	.rodata :
	{
		*(.rodata)
	} :data

	.data :
	{
		*(.data)
	}

	.bss :
	{
		*(.bss)
	}
}
