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
	.rodata 0 : AT(textend)
	{
		*(.rodata)
	} :data

	.data SIZEOF(.rodata) : AT(textend + SIZEOF(.rodata))
	{
		*(.data)
	} :data

	.bss SIZEOF(.rodata) + SIZEOF(.data) : AT(textend + SIZEOF(.rodata) + SIZEOF(.data))
	{
		*(.bss)
	} :data
}
