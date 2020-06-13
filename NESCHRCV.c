/////////////////////////////////////////////////////////////////////////////
// NESCHRCV.c
// programmed by pirota
/////////////////////////////////////////////////////////////////////////////

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "png.h"
#include "pngctrl.h"

#define INFILE_EXT  ".png"
#define OUTFILE_EXT ".chr"

#define PAL_LINES	64		// �ő�p���b�g�{��

#define swap_byte(a, b)	do { u_char tmp = *a; *a = *b; *b = tmp; } while(0)

typedef struct __pal_collection
{
	int		used;		// �g�p���ꂽ��
	u_char	pal[4];		// NES�p���b�g�C���f�b�N�X

} TPAL_COLLECTIONS, *pTPAL_COLLCECTION;

static TPAL_COLLECTIONS nespal[PAL_LINES];

static u_char *outbuf = NULL;
static u_char *pat_ptr = NULL;

static char infilename[256];
static char outfilename[256];

static int main_result = 0;
static int spr_num;

static int pal_cou;

static int width;
static int height;

PDIB dibbuf = NULL;

static int opt_d = 0;													// �f�o�b�O�I�v�V����

static int now_line = 0;



static int getfilesize(char *);
static int readjob(void);
static int cvjob(void);

// �g�p�@�̕\��
static void usage(void)
{
    printf("usage: NESCHRCV infile[" INFILE_EXT "] OutFile\n"\
		   "\t-d\tDIB�t�@�C�����o�́i�f�o�b�O�p�j\n"
		   );

    exit(0);
}

// EDGE�p���b�g�C���f�b�N�X����NES�p���b�g�C���f�b�N�X�ɕϊ�
static int edge2nes(int idx)
{
	int y = (idx >> 4);
	int x = (idx & 3);

	return (x << 4) + y;
}

// �o�u���\�[�g
static void bubblesort(u_char *a)
{
	BOOL isEnd = FALSE;
	int finAdjust = 1;
	int i;
	while (isEnd == FALSE)
	{
		BOOL wasSwap = FALSE;
		for (i = 0; i < 4 - finAdjust; i++)
		{
			if (a[i+1] < a[i])
			{
				swap_byte(&(a[i+1]), &(a[i]));
				wasSwap = TRUE;
			}
		}
		if (!wasSwap)
		{
			isEnd = TRUE;
		}
		finAdjust++;
	}
}

/////////////////////////////////////
// main
/////////////////////////////////////
int main(int argc, char *argv[])
{
	int i;
	int ch;
	
    char drive[ _MAX_DRIVE ];
    char dir[ _MAX_DIR ];
    char fname[ _MAX_FNAME ];
    char ext[ _MAX_EXT ];

	// �R�}���h���C�����
	memset(infilename, 0, sizeof(infilename) );
	memset(outfilename, 0, sizeof(outfilename) );
	
    printf("PNG to NESCHR Converter Ver0.00 " __DATE__ "," __TIME__ " Programmed by pirota\n");

	//----------- bubble sort test --------------
//	static u_char test[] = { 0, 3, 4, 1, 0 };
//	bubblesort(test);

    if (argc <= 1)
	{
		usage();
	}
        

	for (i=1; i<argc; i++)
	{
		ch = argv[i][0];
		if (ch == '-' || ch == '/')
		{
			// �X�C�b�`
			switch (argv[i][1])
			{
			case 'd':
				opt_d = 1;
//				printf("opt_d\n");
				break;
			default:
				printf("-%c �I�v�V�������Ⴂ�܂��B\n",argv[i][1]);
				break;
			}

			continue;
		}
		// �t�@�C��������
		if (!infilename[0])
		{
			strcpy(infilename, argv[i]);
			_splitpath(infilename , drive, dir, fname, ext );
			if (ext[0]==0)
				strcat(infilename, INFILE_EXT);							// �g���q�⊮

			continue;
		}
		
		// �o�̓t�@�C�����̍쐬
		if (!outfilename[0])
		{
			// �o�̓t�@�C���l�[��
			strcpy(outfilename, argv[i]);
		}
		
	}
	// �o�̓t�@�C�������ȗ�����Ă���
	if (!outfilename[0])
	{
		// �o�̓t�@�C���l�[��
		sprintf(outfilename, "nes.chr");
	}
	
    // �t�@�C���ǂݍ��ݏ���
    if (readjob()<0)
		goto cvEnd;


	// �e��o�b�t�@�̊m��

	// NES�p���b�g�̃��[�N
	memset(nespal, 0, sizeof(nespal));


	// �o�̓o�b�t�@�̊m��
	outbuf = (u_char *) malloc((width/8) * (height*2));
	if (outbuf == NULL)
	{
		printf("�o�̓o�b�t�@�͊m�ۂł��܂���\n");
		goto cvEnd;
	}

    // �ϊ�����
	if (cvjob() < 0)
	{
		goto cvEnd;
	}
    
cvEnd:
	// ��n��
	// NES�p�^�[���o�̓o�b�t�@�J��
	if (pat_ptr != NULL)
	{
		free(pat_ptr);
	}

	// �X�v���C�g�o�̓o�b�t�@�J��
	if (outbuf != NULL)
	{
		free(outbuf);
	}

	// �p�b�N�o�b�t�@�J��
	if (dibbuf != NULL)
	{
		free(dibbuf);
	}

	return main_result;
}

//----------------------
// �t�@�C���T�C�Y���擾
//----------------------
static int getfilesize(char *fname)
{
    int result = -1;
    FILE * fp;

    fp = fopen(fname, "rb");
    if (fp == NULL)
        return result;

    fseek(fp, 0, SEEK_END);
    result = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    fclose(fp);

    return result;
}

//----------------------------
// �o�m�f�ǂݍ��݂c�h�a�ɕϊ�
//----------------------------
// Out: 0=�n�j
//      -1 = �G���[
static int readjob(void)
{
	FILE *fp;
	BITMAPFILEHEADER bf;
	BITMAPINFOHEADER *bi;
	u_char *pimg;
	int bytes;
	int xl, yl;
    u_char a;
    
	dibbuf = PngOpenFile(infilename);
	if (dibbuf == NULL)
	{
		printf("Can't open '%s'.\n", infilename);
		return -1;
	}

	bi = (BITMAPINFOHEADER *)dibbuf;
	// �e�X�g
	memset(&bf, 0, sizeof(bf));
	bf.bfType = 'MB';
	bf.bfSize = sizeof(bf);
	bf.bfOffBits = sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER)+sizeof(RGBQUAD)*bi->biClrUsed;

	width = bi->biWidth;
	height = bi->biHeight;

	if (width != 128 || height != 128)
	{
		printf("Error:�摜�T�C�Y��128x128px �����󂯕t���܂���B\n");
		return -1;
	}

	if (bi->biBitCount != 8)
	{
		printf("Error:�摜�t�@�C���� �C���f�b�N�X�J���[ �����󂯕t���܂���B\n");
		return -1;
	}

	// EDGE��NES �p���b�g�֕ϊ�
	for (yl=0; yl<height; yl++)
	{
		pimg = (u_char *) dibbuf + (sizeof(BITMAPINFOHEADER)+sizeof(RGBQUAD)*bi->biClrUsed) + (yl * width);
		for (xl=0; xl<width; xl++)
		{
			a = *pimg;
			a = edge2nes(a);
			*pimg = a;
			pimg++;
		}
	}

	pat_ptr = (u_char *)malloc(width*height);
	memset(pat_ptr, 0, width*height);

	if (opt_d)
	{
		// �f�o�b�O�I�v�V������on�Ȃ�DIB�t�@�C���o��
		fp = fopen("test.bmp" , "wb");
		fwrite(&bf, 1, sizeof(bf), fp);
		
		bytes = bi->biSizeImage + (bi->biClrUsed * sizeof(RGBQUAD)) + sizeof(BITMAPINFOHEADER);
		
		fwrite(dibbuf, 1, bytes, fp);
		
		fclose(fp);
		printf("debug: 'test.bmp' wrote.\n");
	}

	return 0;
}

//
static void search_reg_palet(u_char *ptr, int stride)
{
	int i, j, k;
	int found;
	u_char * pimg = ptr;
	u_char a;

	TPAL_COLLECTIONS paltmp;
	memset(&paltmp, 0, sizeof(paltmp));

	// 16x16�Ŏg���Ă���F�𒲂ׂ�
	// 16x16 px 1block
	for (i = 0; i < 16; i++)
	{
		for (j = 0; j < 16; j++, pimg++)
		{
			a = *(pimg);
			if (a == 0)
			{
				continue;
			}
			found = 0;
			// ���݂̃p���b�g���C���̌���
			for (k = 1; k < 4; k++)
			{
				if (a == paltmp.pal[k])
				{
					// �g�p�ς݂̐F
					found = k;
					break;
				}
			}
			// �F��������Ȃ������ꍇ�A�V�K�o�^
			if (found == 0)
			{
				k = (++paltmp.used);
				if (k >= 4)
				{
					// �p���b�g�I�[�o�[�t���[
					printf("warning: �p���b�g�̎g�p����3�F���z���Ă��܂��B\n");
					k =	(--paltmp.used);
				}
				// �V�K�F�̓o�^
				paltmp.pal[k] = a;
			}

		} // j
		pimg += (stride - 16);

	}  // i
	// �p���b�g�̃n�b�V���쐬
	bubblesort(paltmp.pal);

	// �n�b�V���ɍ��킹�ăC���f�b�N�X������
	pimg = ptr;
	for (i = 0; i < 16; i++)
	{
		for (j = 0; j < 16; j++, pimg++)
		{
			a = *(pimg);
			if (a == 0)
			{
				continue;
			}
			found = 0;
			// ���݂̃p���b�g���C���̌���
			for (k = 1; k < 4; k++)
			{
				if (a == paltmp.pal[k])
				{
					// �g�p�ς݂̐F
					found = k;
					break;
				}
			}

			// �p���b�g���C���f�b�N�X�ɒu��
			*pimg = (u_char)found;

		} // j
		pimg += (stride - 16);
	} // i

	// �V�K�n�b�V���̎��̂݃p���b�g�o�^
	DWORD hash1 = *((DWORD *)&paltmp.pal);
	DWORD hash2 = *((DWORD *)&nespal[now_line].pal);
	if (hash1 != hash2)
	{
		if (now_line >= PAL_LINES)
		{
			printf("Error: �p���b�g�s���� %d ���z���܂����B", PAL_LINES);
		}
		else
		{
			nespal[now_line++] = paltmp;
			printf("\t.byte\t");
			for (i = 0; i < 4; i++)
			{
				printf("%2d", nespal[now_line - 1].pal[i]);
				if (i < 3)
				{
					putchar(',');
				}
				else
				{
					puts("");
				}
			}

		}
	}


}

///////////////////////////////
// �X�v���C�g�f�[�^�ɕϊ�����
///////////////////////////////
static int cvjob(void)
{
	BITMAPINFOHEADER *bi;
	int xl,yl;
	u_char *pimg;
	u_char *outptr;
	FILE *fp;

	bi = (BITMAPINFOHEADER *)dibbuf;
	spr_num = (width >> 3) * (height >> 3);					// �X�v���C�g��

	// �ϊ�����
	outptr = outbuf;										// �X�v���C�g�o�̓o�b�t�@�̏�����

	pimg = NULL;
	for (yl=0; yl<height; yl+=16)
	{
		for (xl=0; xl<width; xl+=16)
		{
			pimg = (u_char *) dibbuf + (sizeof(BITMAPINFOHEADER)+sizeof(RGBQUAD)*bi->biClrUsed);
			pimg += ((yl * bi->biWidth) + xl);
			search_reg_palet(pimg, width);
		} // xl

		pimg += (width - 16);

	} // yl

	printf("�p���b�g�{���G%d\n", now_line);


	// �t�@�C���o��
	fp = fopen(outfilename,"wb");
	if (fp == NULL)
	{
		printf("Can't write '%s'.\n", outfilename);
		return -1;
	}
    
	// �p���b�g�o��
#if 0
	dibpal = (RGBQUAD *)((u_char *) dibbuf + sizeof(BITMAPINFOHEADER));
	fputc(pal_cou-1, fp);												// �p���b�g��
	for (i=0; i<pal_cou; i++)
	{
		paltmp = dibpal + pPal_order[i];
		fputc(paltmp->rgbRed, fp);										// �p���b�g��
		fputc(paltmp->rgbGreen, fp);									// �p���b�g��
		fputc(paltmp->rgbBlue, fp);										// �p���b�g��
	}
#endif

	// �p�^�[���o��
#if 0
	fputc(spr_num-1, fp);												// �p�^�[����
	a = fwrite(outbuf, 1, 64*spr_num, fp);
	if (a != (64*spr_num))
	{
		printf("'%s' �t�@�C�����������������߂܂���ł����I\n", outfilename);
	}
#endif
	fclose(fp);

	// ���ʏo��
	printf("�p���b�g��:%d\n",pal_cou);
	printf("�p�^�[����:%d\n",spr_num);
	printf("�X�v���C�g�f�[�^ '%s'���쐬���܂����B\n", outfilename);

	return 0;
}
