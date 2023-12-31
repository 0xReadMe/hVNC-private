#pragma once

typedef struct
{
	unsigned char state[256];
	unsigned char x;
	unsigned char y;
} rc4_key;

__forceinline void swap_byte(unsigned char * a, unsigned char * b)
{
	unsigned char swapByte = *a;
	*a = *b;
	*b = swapByte;
}

void prepare_rc4_key(unsigned char * key_data_ptr, int key_data_len, rc4_key * key)
{
	unsigned char index1;
	unsigned char index2;
	unsigned char * state;

	state = &key->state[0];
	for (int counter = 0; counter < 256; counter++)
		state[counter] = (unsigned char)counter;

	key->x = 0; key->y = 0;
	index1 = 0;
	index2 = 0;
	for (int counter = 0; counter < 256; counter++)
	{
		index2 = (key_data_ptr[index1] + state[counter] + index2) % 256;
		swap_byte(&state[counter], &state[index2]);
		index1 = (index1 + 1) % (unsigned char)key_data_len;
	}
}

void rc4(unsigned char * buffer_ptr, int buffer_len, rc4_key * key)
{
	unsigned char x;
	unsigned char y;
	unsigned char * state;
	unsigned char xorIndex;

	x = key->x;
	y = key->y;
	state = &key->state[0];
	for (int counter = 0; counter < buffer_len; counter++)
	{
		x = (x + 1) % 256;
		y = (state[x] + y) % 256;
		swap_byte(&state[x], &state[y]);
		xorIndex = (state[x] + state[y]) % 256;
		buffer_ptr[counter] ^= state[xorIndex];
	}
	key->x = x;
	key->y = y;
}