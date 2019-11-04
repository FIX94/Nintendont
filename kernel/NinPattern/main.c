#include <stdio.h>
#include <malloc.h>
#include <string.h>
typedef struct FuncPattern
{
	unsigned int Length;
	unsigned int Loads;
	unsigned int Stores;
	unsigned int FCalls;
	unsigned int Branch;
	unsigned int Moves;
	const char *Name;
	unsigned int Group;
	unsigned int Found;
} FuncPattern;

void MPattern( char *Data, size_t Length, FuncPattern *FunctionPattern )
{
	unsigned int i;
	memset( FunctionPattern, 0, sizeof(FuncPattern) );

	for( i = 0; i < Length; i+=4 )
	{
		unsigned int word = __builtin_bswap32(*(unsigned int*)(Data + i));
		
		if( (word & 0xFC000003) ==  0x48000001 )
			FunctionPattern->FCalls++;

		if( (word & 0xFC000003) ==  0x48000000 )
			FunctionPattern->Branch++;
		if( (word & 0xFFFF0000) ==  0x40800000 )
			FunctionPattern->Branch++;
		if( (word & 0xFFFF0000) ==  0x41800000 )
			FunctionPattern->Branch++;
		if( (word & 0xFFFF0000) ==  0x40810000 )
			FunctionPattern->Branch++;
		if( (word & 0xFFFF0000) ==  0x41820000 )
			FunctionPattern->Branch++;
		
		if( (word & 0xFC000000) ==  0x80000000 )
			FunctionPattern->Loads++;
		if( (word & 0xFF000000) ==  0x38000000 )
			FunctionPattern->Loads++;
		if( (word & 0xFF000000) ==  0x3C000000 )
			FunctionPattern->Loads++;

		if( (word & 0xFC000000) ==  0x90000000 )
			FunctionPattern->Stores++;
		if( (word & 0xFC000000) ==  0x94000000 )
			FunctionPattern->Stores++;

		if( (word & 0xFF000000) ==  0x7C000000 )
			FunctionPattern->Moves++;
		if( word == 0x4E800020 )
			break;
	}
	FunctionPattern->Length = i;
}
int main()
{
	FILE *f = fopen("func.bin", "rb");
	if(!f)
		return 0;
	fseek(f, 0, SEEK_END);
	size_t size = ftell(f);
	rewind(f);
	char *buf = malloc(size);
	fread(buf, size, 1, f);
	fclose(f);
	FuncPattern func;
	MPattern(buf, size, &func);
		
	printf("{   0x%X,    %d,     %d,     %d,     %d,     %d,\n",
		func.Length, func.Loads, func.Stores, func.FCalls, func.Branch, func.Moves);
	free(buf);
	return 0;
}