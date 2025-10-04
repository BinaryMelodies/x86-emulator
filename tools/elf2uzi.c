
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

int main(int argc, char * argv[])
{
	if(argc < 3)
	{
		fprintf(stderr, "Usage: %s <input ELF file> <output UZI binary>\n", argv[0]);
		exit(0);
	}
	FILE * infile = fopen(argv[1], "rb");
	if(!infile)
	{
		fprintf(stderr, "Error: %s not found\n", argv[1]);
		exit(1);
	}
	struct stat Stat;
	fstat(fileno(infile), &Stat);
	// e_entry: procedure entry point
	fseek(infile, 24L, SEEK_SET);
	uint32_t e_entry;
	fread(&e_entry, sizeof e_entry, 1, infile);
	//printf("%X\n", e_entry);
	// e_phoff: program header
	uint32_t e_phoff;
	fread(&e_phoff, sizeof e_phoff, 1, infile);
	//printf("%X\n", e_phoff);
	// e_phentsize: program header entry size
	fseek(infile, 10L, SEEK_CUR);
	uint16_t e_phentsize;
	fread(&e_phentsize, sizeof e_phentsize, 1, infile);
	//printf("%X\n", e_phentsize);

	// e_phnum: program header entry count
	uint16_t e_phnum;
	fread(&e_phnum, sizeof e_phnum, 1, infile);
	//printf("%X\n", e_phnum);

	if(e_phnum != 1)
	{
		fprintf(stderr, "Error: Only able to convert files with one segments, this one has %d\n", e_phnum);
		exit(1);
	}

	uint32_t p_offset[1];
	uint32_t p_vaddr[1];
	uint32_t p_paddr[1];
	uint32_t p_filesz[1];
	uint32_t p_memsz[1];
	int i;
	for(i = 0; i < e_phnum; i++)
	{
		fseek(infile, (long)e_phoff + e_phentsize * i + 4, SEEK_SET);
		fread(&p_offset[i], sizeof p_offset[i], 1, infile);
		fread(&p_vaddr[i], sizeof p_vaddr[i], 1, infile);
		fread(&p_paddr[i], sizeof p_paddr[i], 1, infile);
		fread(&p_filesz[i], sizeof p_filesz[i], 1, infile);
		fread(&p_memsz[i], sizeof p_memsz[i], 1, infile);
	}

	FILE * outfile = fopen(argv[2], "wb");
	fchmod(fileno(outfile), Stat.st_mode);
	if(!outfile)
	{
		fprintf(stderr, "Error: Unable to write to %s\n", argv[2]);
		exit(1);
	}

	uint16_t data;
	/* magic number */
	fputc(0xC3, outfile);
	/* entry */
	data = e_entry;
	fwrite(&data, sizeof data, 1, outfile);
#if 0
	/* old FUZIX magic number */
	data = 'F' | ('Z' << 8);
	fwrite(&data, sizeof data, 1, outfile);
	data = 'X' | ('1' << 8);
	/* base page, not relevant for Z80 */
	fputc(p_vaddr[0] >> 16, outfile);
	/* memory size */
	data = 0;
	fwrite(&data, sizeof data, 1, outfile);
	/* text size */
	data = p_memsz[0];
	fwrite(&data, sizeof data, 1, outfile);
	/* data size */
	data = p_filesz[1];
	fwrite(&data, sizeof data, 1, outfile);
	/* bss size */
	data = p_memsz[1] - p_filesz[1];
	fwrite(&data, sizeof data, 1, outfile);
#endif

	fseek(infile, (long)p_offset[0] + 0x0103, SEEK_SET);
	for(i = 0; i < p_filesz[0] - 0x0103; i++)
	{
		fputc(fgetc(infile), outfile);
	}
	for(; i < p_memsz[0]; i++)
	{
		fputc(0, outfile);
	}

	fclose(outfile);
	fclose(infile);

	return 0;
}

