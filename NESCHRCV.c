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

static char infilename[256];
static char outfilename[256];

static int main_result = 0;
static int spr_num;

static int pal_cou;

static int *pal_buf = NULL;
static u_char *outbuf = NULL;

PDIB dibbuf = NULL;

static int opt_b = 0;													/* �x�^�o�̓I�v�V���� */
static int opt_d = 0;													/* �f�o�b�O�I�v�V���� */
static int opt_p = 0;													/* �p���b�g���K���I�v�V���� */
static int opt_n = 0;													/* �p���b�g���K���I�v�V�����Q */

static int getfilesize(char *);
static int readjob(void);
static int cvjob(void);

// �g�p�@�̕\��
static void usage(void)
{
    printf("usage: NESCHRCV infile[" INFILE_EXT "] OutFile\n"\
		   "\t-b[1-256]\t�p���b�g�œK���Ȃ��ŕϊ��i�ȗ����ɂ�256�j\n" \
		   "\t-n\t�F���p���b�g�O(0-31)�ɐ��K��\n"
		   "\t-p\t64�ȍ~�̐F��32��ɐ��K���i�����ݗp�j\n"
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
        usage();

	for (i=1; i<argc; i++)
	{
		ch = argv[i][0];
		if (ch == '-' || ch == '/')
		{
			// �X�C�b�`
			switch (argv[i][1])
			{
			case 'b':
				opt_b = atoi(argv[i]+2);
				if (opt_b == 0)
				{
					opt_b = 256;
				}
//				printf("opt_b=%d\n",opt_b);
				break;
			case 'd':
				opt_d = 1;
//				printf("opt_d\n");
				break;
			case 'p':
				opt_p = 1;
//				printf("opt_p\n");
				break;
			case 'n':
				opt_n = 1;
//				printf("opt_n\n");
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

	// �o�̓o�b�t�@�̊m��
	outbuf = (u_char *) malloc(64 * 256);
	if (outbuf == NULL)
	{
		printf("�o�̓o�b�t�@�͊m�ۂł��܂���\n");
		goto cvEnd;
	}

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

    // �ϊ�����
	if (cvjob() < 0)
	{
		goto cvEnd;
	}
    
cvEnd:
    // ��n��

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

    // �F�R�[�h���K��
    if (opt_p) {
        for (yl=0; yl<bi->biHeight; yl++) {
            pimg = (u_char *) dibbuf + (sizeof(BITMAPINFOHEADER)+sizeof(RGBQUAD)*bi->biClrUsed) + (yl * bi->biWidth);
            for (xl=0; xl<bi->biWidth; xl++) {
                a = *pimg;
                // �p���b�g�P�ɋ����i�����ݐ�p�j
                if (a > 64) {
                    a = (a & 0x1f) | 0x20;
                    *pimg = a;
                }
                pimg++;
            }
        }

    } else // if (opt_p)
    if (opt_n) {
        //�p���b�g���K�� 0-31
        for (yl=0; yl<bi->biHeight; yl++) {
            pimg = (u_char *) dibbuf + (sizeof(BITMAPINFOHEADER)+sizeof(RGBQUAD)*bi->biClrUsed) + (yl * bi->biWidth);
            for (xl=0; xl<bi->biWidth; xl++) {
                a = *pimg;
                // �p���b�g�O�ɋ���
                a &= 0x1f;
                *pimg = a;
                pimg++;
            }
        }

    } // if (opt_n)

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
	int i,j;
	int xl,yl;
	int a;
	u_char *pimg;
	u_char *outptr;
	RGBQUAD *dibpal, *paltmp;
	FILE *fp;

	bi = (BITMAPINFOHEADER *)dibbuf;
	spr_num = (bi->biWidth >> 3) * (bi->biHeight >> 3);					// �X�v���C�g��/

	// �����F�̐ݒ�
	if (!opt_b)
	{
		// �p���b�g�œK��
		pal_buf[0]=0;
		pPal_order[0]=0;
		pal_cou=1;
	}
	else
	{
		// �p���b�g�œK������
		for (i=0; i<256; i++)
		{
			pal_buf[i]=i;
			pPal_order[i]=i;
		}
		pal_cou=256;
	}

	// �ϊ�����
	outptr = outbuf;													// �X�v���C�g�o�̓o�b�t�@�̏�����
	for (yl=0; yl<bi->biHeight; yl+=8)
	{
		for (xl=0; xl<bi->biWidth; xl+=8)
		{
			pimg = (u_char *) dibbuf + (sizeof(BITMAPINFOHEADER)+sizeof(RGBQUAD)*bi->biClrUsed);
			pimg += ((yl * bi->biWidth) + xl);
			for (i=0; i<8; i++)
			{
				for (j=0; j<8; j++)
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
				pimg += (bi->biWidth - 8);
			}
		}

	}

	// �t�@�C���o��
	fp = fopen(outfilename,"wb");
	if (fp == NULL)
	{
		printf("Can't write '%s'.\n", outfilename);
		return -1;
	}

    if (opt_b > 0) {
        // �x�^�o�̓I�v�V������
        if (pal_cou < opt_b) {
//            printf("warning:-b�w��̐��l(%d)���g�p�F����葽���Ȃ��Ă��܂��B\n",opt_b);
        } else {
            printf("�p���b�g�o�͐���%d�ɐݒ肵�܂��B\n",opt_b);
            pal_cou = opt_b;
        }
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
