#include	<all.h>

uchar	classa[100] =
{
	[ 0] 1,
	[ 1] 2,
	[ 2] 3,
	[ 3] 4,
	[ 4] 5,
	[ 5] 6,
	[ 6] 7,
	[ 7] 8,
	[ 8] 9,
	[10] 10,
	[11] 11,
	[12] 12,
	[13] 13,
	[14] 14,
	[15] 15,
	[16] 16,
	[17] 17,
	[18] 18,
	[20] 19,
	[21] 20,
	[22] 21,
	[23] 22,
	[24] 23,
	[25] 24,
	[26] 25,
	[27] 26,
	[28] 27,
	[30] 28,
	[31] 29,
	[32] 30,
	[33] 31,
	[34] 32,
	[35] 33,
	[36] 34,
	[37] 35,
	[38] 36,
	[40] 37,
	[41] 38,
	[42] 39,
	[43] 40,
	[44] 41,
	[45] 42,
	[46] 43,
	[47] 44,
	[48] 45,
	[50] 46,
	[51] 47,
	[52] 48,
	[53] 49,
	[60] 50,
	[61] 51,
	[62] 52,
	[63] 53,
	[64] 54,
	[65] 55,
	[70] 56,
	[71] 57,
	[72] 58,
	[73] 59,
};
uchar	classb[100] =
{
	[ 0] 60,
	[ 1] 61,
	[ 2] 62,
	[ 3] 63,
	[10] 64,
	[11] 65,
	[12] 66,
	[13] 67,
	[20] 68,
	[21] 69,
	[22] 70,
	[23] 71,
	[30] 72,
	[31] 73,
	[32] 74,
	[33] 75,
	[40] 76,
	[50] 77,
	[51] 78,
	[52] 79,
};
uchar	classc[100] =
{
	[ 0] 80,
	[10] 81,
	[20] 82,
	[30] 83,
	[31] 84,
};
uchar	classd[100] =
{
	[ 0] 85,
	[10] 86,
	[20] 87,
	[21] 88,
	[22] 89,
	[23] 90,
	[24] 91,
	[25] 92,
	[26] 93,
	[27] 94,
	[28] 95,
	[29] 96,
	[30] 97,
	[31] 98,
	[32] 99,
	[33] 100,
	[34] 101,
	[35] 102,
	[36] 103,
	[37] 104,
	[40] 105,
	[41] 106,
	[42] 107,
	[43] 108,
	[44] 109,
	[50] 110,
	[51] 111,
	[52] 112,
	[53] 113,
	[54] 114,
	[55] 115,
	[60] 116,
	[61] 117,
	[62] 118,
	[63] 119,
	[64] 120,
	[65] 121,
	[66] 122,
	[70] 123,
	[71] 124,
	[80] 125,
	[81] 126,
	[82] 127,
	[83] 128,
	[84] 129,
	[85] 130,
	[90] 131,
	[91] 132,
};
uchar	classe[100] =
{
	[ 0] 133,
	[10] 134,
	[20] 135,
	[21] 136,
	[22] 136,
};
uchar	classf[100] =
{
	[ 0] 137,
	[10] 138,
	[11] 139,
	[12] 140,
	[13] 141,
	[20] 142,
	[21] 143,
	[22] 144,
	[23] 145,
	[24] 146,
	[25] 147,
	[30] 148,
	[40] 149,
	[50] 150,
	[60] 151,
	[70] 152,
	[71] 153,
	[72] 154,
	[73] 155,
	[74] 156,
	[90] 185,
	[91] 186,
	[92] 187,
};
uchar	classh[100] =
{
	[ 0] 157,
	[ 1] 158,
	[ 2] 159,
	[10] 160,
	[11] 161,
	[12] 162,
	[13] 163,
	[20] 164,
	[21] 165,
	[22] 166,
	[30] 167,
	[31] 168,
	[32] 169,
	[40] 170,
	[41] 171,
	[42] 172,
	[50] 173,
	[51] 174,
	[53] 175,
	[60] 176,
	[70] 177,
	[71] 178,
	[72] 179,
	[73] 180,
	[74] 181,
	[75] 182,
	[80] 183,
	[81] 184,
};
uchar	classx[100] =
{
	[00] 188,
};

char*	big[] =
{
	/* roads */
	"A05", "A06", "A07", "A08",
	"A10", "A11", "A12", "A13", "A14", "A15", "A16", "A17", "A18",
	"A20", "A21", "A22", "A23", "A24", "A25", "A26", "A27", "A28",
	"A30", "A31", "A32", "A33", "A34", "A35", "A36", "A37", "A38",
	/* railroads */
	/* water */
	"H50", "H51", "H53",
	/* other */
	"F90", "F91", "F92",
	0
};

char*	small[] =
{
	/* roads */
	"A00", "A01", "A02", "A03", "A04", "A05", "A06", "A07", "A08",
	"A10", "A11", "A12", "A13", "A14", "A15", "A16", "A17", "A18",
	"A20", "A21", "A22", "A23", "A24", "A25", "A26", "A27", "A28",
	"A30", "A31", "A32", "A33", "A34", "A35", "A36", "A37", "A38",
	"A40", "A41", "A42", "A43", "A44", "A45", "A46", "A47", "A48",
	"A50", "A51", "A52", "A53",
	"A60", "A61", "A62", "A63", "A64", "A65",
	"A70", "A71", "A72", "A73",
	/* railroads */
	"B00", "B01", 
	"B02", "B03", 
	"B10", "B11", "B12", "B13",
	"B20", "B21", "B22", "B23",
	"B30", "B31", "B32", "B33",
	"B40",
	"B50", "B51", "B52",
	/* water */
	"H10", "H01",
	"H30", "H31",
	"H40", "H41",
	"H50", "H51", "H53",
	/* other */
	0
};

char*	light[] =
{
	"H10", "H01",
	"H30", "H31",
	"H40", "H41",
	"H50", "H51", "H53",
	0
};

char*	dark[] =
{
	"B00", "B01", 
	"B02", "B03", 
	"B10", "B11", "B12", "B13",
	"B20", "B21", "B22", "B23",
	"B30", "B31", "B32", "B33",
	"B40",
	"B50", "B51", "B52",
	0
};

Cursor	thinking =
{
	{ -7, -7 },
	{ 0 },
	{
		0x01, 0x00, 0x00, 0xE0, 0x00, 0x10, 0x03, 0xE0,
		0x04, 0x00, 0x0F, 0xE0, 0x12, 0x3C, 0x1F, 0xE2,
		0x10, 0x1A, 0x10, 0x1A, 0x10, 0x02, 0x10, 0x3C,
		0x18, 0x10, 0x6F, 0xEC, 0x40, 0x04, 0x3F, 0xF8
	},
};
Cursor	bullseye =
{
	{-7, -7},
	{
		0x1F, 0xF8, 0x3F, 0xFC, 0x7F, 0xFE, 0xFB, 0xDF,
	 	0xF3, 0xCF, 0xE3, 0xC7, 0xFF, 0xFF, 0xFF, 0xFF,
	 	0xFF, 0xFF, 0xFF, 0xFF, 0xE3, 0xC7, 0xF3, 0xCF,
	 	0x7B, 0xDF, 0x7F, 0xFE, 0x3F, 0xFC, 0x1F, 0xF8,
	},
	{
		0x00, 0x00, 0x0F, 0xF0, 0x31, 0x8C, 0x21, 0x84,
		0x41, 0x82, 0x41, 0x82, 0x41, 0x82, 0x7F, 0xFE,
		0x7F, 0xFE, 0x41, 0x82, 0x41, 0x82, 0x41, 0x82,
		0x21, 0x84, 0x31, 0x8C, 0x0F, 0xF0, 0x00, 0x00,
	}
};
Cursor	reading =
{
	{-7, -7},
	{ 0 },
	{
		0x00, 0x01, 0x80, 0x03, 0xe0, 0x0d, 0x98, 0x31, 
		0x87, 0xc9, 0xb1, 0x19, 0x9d, 0x61, 0xa5, 0x0d, 
		0xb9, 0x31, 0xb1, 0x25, 0x9d, 0x59, 0x81, 0x11, 
		0x85, 0x62, 0x61, 0x0c, 0x19, 0x30, 0x07, 0xc0, 
	}
};
