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

static int * pPal_order = NULL;
static u_char *outbuf = NULL;

static char infilename[256];
static char outfilename[256];

static int main_result = 0;
static int spr_num;

static int pal_cou;

static int width;
static int height;

PDIB dibbuf = NULL;

static int *col_num_buf = NULL;											// �J���[�g�p���o�b�t�@
static u_int *col_idx_buf = NULL;										// �p���b�g�C���f�b�N�X�o�b�t�@

static int opt_d = 0;													// �f�o�b�O�I�v�V����

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
	col_num_buf = malloc(sizeof(int)*(width/16)*(height/16));				// �J���[�g�p���o�b�t�@
	memset(col_num_buf, 0, sizeof(int)*(width / 16)*(height / 16));
	col_idx_buf = malloc(sizeof(u_int)*(width/16)*(height/16));				// �p���b�g�C���f�b�N�X�o�b�t�@
	memset(col_idx_buf, 0, sizeof(int)*(width/16)*(height/16));


	// �o�̓o�b�t�@�̊m��
	outbuf = (u_char *) malloc((width/8) * (height*2));
	if (outbuf == NULL)
	{
		printf("�o�̓o�b�t�@�͊m�ۂł��܂���\n");
		goto cvEnd;
	}

#if 0
	// �p���b�g�u���o�b�t�@�̊m��
	pPal_order = (int *) malloc(sizeof(int) * 256);
	if (pPal_order == NULL)
	{
		printf("�p���b�g�u���o�b�t�@�͊m�ۂł��܂���\n");
		goto cvEnd;
	}
	
	// �p���b�g�o�b�t�@�̊m��
	pal_buf = (int *) malloc(sizeof(int)*256);
	if (pal_buf == NULL)
	{
		printf("�p���b�g�o�b�t�@���m�ۂł��܂���\n");
		return -1;
	}
	memset(pal_buf, -1, sizeof(int)*256);
#endif

    // �ϊ�����
	if (cvjob() < 0)
	{
		goto cvEnd;
	}
    
cvEnd:
	// ��n��
	// �p���b�g�C���f�b�N�X�o�b�t�@
	if (col_idx_buf != NULL)
	{
		free(col_idx_buf);
	}

	// �p���b�g�g�p���o�b�t�@
	if (col_num_buf != NULL)
	{
		free(col_num_buf);
	}

/*
	// �p���b�g�o�b�t�@�J��
	if (pal_buf != NULL)
	{
		free(pal_buf);
		pal_buf = NULL;
	}
	
	// �p���b�g�u���o�͊J��
	if (pPal_order != NULL)
	{
		free(pPal_order);
	}
*/
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

	// EDGE�p���b�g�֕ϊ�
	for (yl=0; yl<height; yl++)
	{
		pimg = (u_char *) dibbuf + (sizeof(BITMAPINFOHEADER)+sizeof(RGBQUAD)*bi->biClrUsed) + (yl * width);
		for (xl=0; xl<width; xl++)
		{
			a = *pimg;
			a = edge2nes(a);
			pimg++;
		}
	}

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

///////////////////////////////
// �X�v���C�g�f�[�^�ɕϊ�����
///////////////////////////////
static int cvjob(void)
{
	BITMAPINFOHEADER *bi;
	int i,j,k;
	int xl,yl;
	int a;
	u_char *pimg;
	u_char *outptr;
	RGBQUAD *dibpal, *paltmp;
	FILE *fp;

	bi = (BITMAPINFOHEADER *)dibbuf;
	spr_num = (width >> 3) * (height >> 3);					// �X�v���C�g��

	// �ϊ�����
	outptr = outbuf;													// �X�v���C�g�o�̓o�b�t�@�̏�����
	int* ptr_col_num = col_num_buf;
	u_int* ptr_idx_buf = col_idx_buf;

	for (yl=0; yl<height; yl+=16)
	{
		for (xl=0; xl<width; xl+=16)
		{
			pimg = (u_char *) dibbuf + (sizeof(BITMAPINFOHEADER)+sizeof(RGBQUAD)*bi->biClrUsed);
			pimg += ((yl * bi->biWidth) + xl);
			int pal_col = 0;

			for (i=0; i<16; i++)
			{
				for (j=0; j<16; j++)
				{
					a = *(pimg++);
					if (pal_buf[a]>=0)
					{
						// ���Ɏg�p�ς݂̐F
						*(outptr++) = (u_char) pal_buf[a];
					}
					else
					{
						// �V�K�Ɍ������F
						pal_buf[a] = pal_cou;							// ���p�C���f�b�N�X
						pPal_order[pal_cou] = a;						// �t����
						*(outptr++) = (u_char) pal_buf[a];
						pal_cou++;
					}
					
				}
				pimg += (width - 16);
			}
			ptr_col_num++;
			ptr_idx_buf++;
		}

	}

	// �t�@�C���o��
	fp = fopen(outfilename,"wb");
	if (fp == NULL)
	{
		printf("Can't write '%s'.\n", outfilename);
		return -1;
	}
    
	// �p���b�g�o��
	dibpal = (RGBQUAD *)((u_char *) dibbuf + sizeof(BITMAPINFOHEADER));
	fputc(pal_cou-1, fp);												// �p���b�g��
	for (i=0; i<pal_cou; i++)
	{
		paltmp = dibpal + pPal_order[i];
		fputc(paltmp->rgbRed, fp);										// �p���b�g��
		fputc(paltmp->rgbGreen, fp);									// �p���b�g��
		fputc(paltmp->rgbBlue, fp);										// �p���b�g��
	}

	// �p�^�[���o��
	fputc(spr_num-1, fp);												// �p�^�[����
	a = fwrite(outbuf, 1, 64*spr_num, fp);
	if (a != (64*spr_num))
	{
		printf("'%s' �t�@�C�����������������߂܂���ł����I\n", outfilename);
	}

	fclose(fp);

	// ���ʏo��
	printf("�p���b�g��:%d\n",pal_cou);
	printf("�p�^�[����:%d\n",spr_num);
	printf("�X�v���C�g�f�[�^ '%s'���쐬���܂����B\n", outfilename);

	return 0;
}
